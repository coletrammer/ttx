#include "ttx/pane.h"

#include "di/container/string/string_view.h"
#include "di/container/vector/vector.h"
#include "di/function/container/function.h"
#include "di/sync/atomic.h"
#include "di/sync/memory_order.h"
#include "di/vocab/pointer/box.h"
#include "dius/print.h"
#include "dius/sync_file.h"
#include "dius/system/process.h"
#include "ttx/modifiers.h"
#include "ttx/mouse.h"
#include "ttx/mouse_event.h"
#include "ttx/paste_event.h"
#include "ttx/renderer.h"
#include "ttx/terminal.h"
#include "ttx/terminal/escapes/osc_52.h"
#include "ttx/terminal/multi_cell_info.h"
#include "ttx/terminal/screen.h"
#include "ttx/utf8_stream_decoder.h"

namespace ttx {
static auto spawn_child(CreatePaneArgs& args, dius::SyncFile& pty, Size const& size, i32 stdin_fd, i32 stdout_fd,
                        di::Vector<i32> const& close_fds) -> di::Result<dius::system::ProcessHandle> {
    auto tty_path = TRY(pty.get_psuedo_terminal_path());

#ifdef __linux__
    // On linux, we can set the terminal size in on the controlling pty. On MacOS, we need to do it in
    // the child. Doing so requires using fork() instead of posix_spawn() when spawning the process, so
    // we try to avoid it. Additionally, opening the psudeo terminal on linux implicitly will make it
    // the controlling terminal, so there's no need to call ioctl(TIOCSCTTY).
    TRY(pty.set_tty_window_size(size.as_window_size()));
#endif

    auto result = dius::system::Process(di::move(args.command));
    if (args.cwd) {
        result = di::move(result).with_optional_current_working_directory(args.cwd.value().clone());
    }
    if (args.terminfo_dir) {
        result = di::move(result).with_env("TERMINFO"_ts, args.terminfo_dir.value().data().to_owned());
    }
    result = di::move(result)
                 .with_new_session()
                 .with_env("TERM"_ts, args.term.to_owned())
                 .with_env("COLORTERM"_ts, "truecolor"_ts)
                 .with_env("TERM_PROGRAM"_ts, "ttx"_ts)
                 .with_file_open(2, di::move(tty_path), dius::OpenMode::ReadWrite)
                 .with_file_dup(stdin_fd, 0)
                 .with_file_dup(stdout_fd, 1)
#ifndef __linux__
                 .with_tty_window_size(2, size.as_window_size())
                 .with_controlling_tty(2)
#endif
        ;
    for (auto close_fd : close_fds) {
        result = di::move(result).with_file_close(close_fd);
    }

    return di::move(result).spawn();
}

auto Pane::create_from_replay(u64 id, di::Optional<di::Path> cwd, di::PathView replay_path,
                              di::Optional<di::Path> save_state_path, Size const& size, PaneHooks hooks)
    -> di::Result<di::Box<Pane>> {
    auto replay_file = TRY(dius::open_sync(replay_path, dius::OpenMode::Readonly));

    // When replaying content, there is no need for any threads, a psudeo terminal or a sub-process.
    auto pane =
        di::make_box<Pane>(id, di::move(cwd), dius::SyncFile(), size, dius::system::ProcessHandle(), di::move(hooks));

    // Allow the terminal to use CSI 8; height; width; t. Normally this would be ignored since application
    // shouldn't control this.
    pane->m_terminal.get_assuming_no_concurrent_accesses().set_allow_force_terminal_size();

    // Now read the replay file and play it back directly into the terminal. By doing this synchronously any test system
    // can look at the pane content's afterwards and no we've finished.
    auto buffer = di::Vector<byte> {};
    buffer.resize(16384);

    auto parser = EscapeSequenceParser();
    auto utf8_decoder = Utf8StreamDecoder {};
    for (;;) {
        auto nread = replay_file.read_some(buffer.span());
        if (!nread.has_value() || nread == 0) {
            break;
        }

        auto utf8_string = utf8_decoder.decode(buffer | di::take(*nread));

        auto parser_result = parser.parse_application_escape_sequences(utf8_string);

        auto events = pane->m_terminal.with_lock([&](Terminal& terminal) {
            terminal.on_parser_results(parser_result.span());
            return terminal.outgoing_events();
        });

        for (auto&& event : events) {
            di::visit(di::overload(
                          [&](terminal::OSC52&& osc52) {
                              if (pane->m_hooks.did_selection) {
                                  pane->m_hooks.did_selection(di::move(osc52), false);
                              }
                          },
                          [&](APC&& apc) {
                              if (pane->m_hooks.apc_passthrough) {
                                  pane->m_hooks.apc_passthrough(apc.data.view());
                              }
                          },
                          [&](terminal::OSC7&&) {}),
                      di::move(event));
        }
    }

    // If requested, write out the terminal state now that everything has been replayed.
    // This is useful to testing.
    for (auto const& path : save_state_path) {
        TRY(pane->save_state(path));
    }

    // Ensure mouse works even if the replayed file enabled mouse reporting.
    pane->m_terminal.get_assuming_no_concurrent_accesses().reset_mouse_reporting();

    return pane;
}

auto Pane::create(u64 id, CreatePaneArgs args, Size const& size) -> di::Result<di::Box<Pane>> {
    if (args.mock) {
        return create_mock(id, di::move(args.cwd));
    }

    if (args.replay_path) {
        return create_from_replay(id, di::move(args.cwd), *args.replay_path, di::move(args.save_state_path), size,
                                  di::move(args.hooks));
    }

    auto capture_file = di::Optional<dius::SyncFile> {};
    if (args.capture_command_output_path) {
        capture_file = TRY(dius::open_sync(args.capture_command_output_path.value(), dius::OpenMode::WriteClobber));

        // Write out inital terminal size information.
        di::writer_print<di::String::Encoding>(capture_file.value(), "\033[4;{};{}t"_sv, size.ypixels, size.xpixels);
        di::writer_print<di::String::Encoding>(capture_file.value(), "\033[8;{};{}t"_sv, size.rows, size.cols);
    }

    auto pty_controller = TRY(dius::open_psuedo_terminal_controller(dius::OpenMode::ReadWrite));
#ifdef __linux__
    auto restore_termios = TRY(pty_controller.get_termios_restorer());
#endif

    // This logic allows piping input and output from the shell command. This ends up being very complicated...
    auto stdin_fd = 2;
    auto stdout_fd = 2;
    auto close_fds = di::Vector<i32> {};
    auto write_pipes = di::Optional<di::Tuple<dius::SyncFile, dius::SyncFile>> {};
    auto read_pipes = di::Optional<di::Tuple<dius::SyncFile, dius::SyncFile>> {};
    if (args.pipe_input) {
        write_pipes = TRY(dius::open_pipe(dius::OpenFlags::KeepAfterExec));
        stdin_fd = di::get<0>(write_pipes.value()).file_descriptor();
        close_fds.push_back(di::get<0>(write_pipes.value()).file_descriptor());
        close_fds.push_back(di::get<1>(write_pipes.value()).file_descriptor());
    }
    if (args.pipe_output) {
        read_pipes = TRY(dius::open_pipe(dius::OpenFlags::KeepAfterExec));
        stdout_fd = di::get<1>(read_pipes.value()).file_descriptor();
        close_fds.push_back(di::get<0>(read_pipes.value()).file_descriptor());
        close_fds.push_back(di::get<1>(read_pipes.value()).file_descriptor());
    }

    auto process = TRY(spawn_child(args, pty_controller, size, stdin_fd, stdout_fd, close_fds));
    auto pane =
        di::make_box<Pane>(id, di::move(args.cwd), di::move(pty_controller), size, process, di::move(args.hooks));
#ifdef __linux__
    pane->m_restore_termios = di::move(restore_termios);
#endif

    pane->m_process_thread = TRY(dius::Thread::create([&pane = *pane] mutable {
        auto result = pane.m_process.wait();
        pane.m_done.store(true, di::MemoryOrder::Release);

        if (pane.m_hooks.did_exit) {
            pane.m_hooks.did_exit(pane, result.optional_value());
        }
    }));

    pane->m_reader_thread =
        TRY(dius::Thread::create([&pane = *pane, capture_file = di::move(capture_file)] mutable -> void {
            auto parser = EscapeSequenceParser();
            auto utf8_decoder = Utf8StreamDecoder {};

            auto buffer = di::Vector<byte> {};
            buffer.resize(16384);

            while (!pane.m_done.load(di::MemoryOrder::Acquire)) {
                auto nread = pane.m_pty_controller.read_some(buffer.span());
                if (!nread.has_value()) {
                    break;
                }

                if (capture_file) {
                    if (pane.m_capture.load(di::MemoryOrder::Acquire)) {
                        (void) di::write_exactly(*capture_file, buffer | di::take(*nread));
                    } else {
                        capture_file = {};
                    }
                }

                auto utf8_string = utf8_decoder.decode(buffer | di::take(*nread));

                auto parser_result = parser.parse_application_escape_sequences(utf8_string);

                auto events = pane.m_terminal.with_lock([&](Terminal& terminal) {
                    terminal.on_parser_results(parser_result.span());
                    return terminal.outgoing_events();
                });

                for (auto&& event : events) {
                    di::visit(di::overload(
                                  [&](terminal::OSC52&& osc52) {
                                      if (pane.m_hooks.did_selection) {
                                          pane.m_hooks.did_selection(di::move(osc52), false);
                                      }
                                  },
                                  [&](APC&& apc) {
                                      if (pane.m_hooks.apc_passthrough) {
                                          pane.m_hooks.apc_passthrough(apc.data.view());
                                      }
                                  },
                                  [&](terminal::OSC7&& osc7) {
                                      pane.update_cwd(di::move(osc7));
                                  }),
                              di::move(event));
                }

                if (pane.m_hooks.did_update) {
                    pane.m_hooks.did_update(pane);
                }
            }
        }));

    if (args.pipe_input) {
        pane->m_pipe_writer_thread = TRY(dius::Thread::create(
            [&pane = *pane, pipe = di::move(write_pipes).value(), input = di::move(args.pipe_input).value()] mutable {
                auto& [read, write] = pipe;
                (void) read.close();
                (void) write.write_exactly(di::as_bytes(input.span()));
                (void) write.close();
            }));
    }
    if (args.pipe_output) {
        pane->m_pipe_reader_thread =
            TRY(dius::Thread::create([&pane = *pane, pipe = di::move(read_pipes).value()] mutable {
                auto& [read, write] = pipe;
                (void) write.close();

                auto utf8_decoder = Utf8StreamDecoder {};

                auto buffer = di::Vector<byte> {};
                buffer.resize(16384);

                auto contents = di::String {};
                while (!pane.m_done.load(di::MemoryOrder::Acquire)) {
                    auto nread = read.read_some(buffer.span());
                    if (!nread.has_value()) {
                        break;
                    }

                    auto utf8_string = utf8_decoder.decode(buffer | di::take(*nread));
                    contents.append(utf8_string);
                }

                (void) read.close();

                if (pane.m_hooks.did_finish_output) {
                    pane.m_hooks.did_finish_output(contents.view());
                }
            }));
    }

    return pane;
}

auto Pane::create_mock(u64 id, di::Optional<di::Path> cwd) -> di::Box<Pane> {
    auto fake_psuedo_terminal = dius::SyncFile();
    return di::make_box<Pane>(id, di::move(cwd), di::move(fake_psuedo_terminal), Size(1, 1),
                              dius::system::ProcessHandle(), PaneHooks {});
}

Pane::~Pane() {
    // TODO: timeout/skip waiting for processes to die after sending SIGHUP.
    (void) m_process.signal(dius::Signal::Hangup);
    (void) m_pipe_reader_thread.join();
    (void) m_pipe_writer_thread.join();
    (void) m_reader_thread.join();
    (void) m_process_thread.join();
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
auto Pane::draw(Renderer& renderer) -> RenderedCursor {
    return m_terminal.with_lock([&](Terminal& terminal) {
        auto& screen = terminal.active_screen().screen;
        if (terminal.allowed_to_draw()) {
            auto whole_screen_dirty = screen.whole_screen_dirty();
            auto end_row = 0_u32;
            for (auto r :
                 di::range(m_vertical_scroll_offset, m_vertical_scroll_offset + terminal.visible_size().rows)) {
                if (r + screen.visual_scroll_offset() >= screen.absolute_row_end()) {
                    break;
                }
                end_row = r - m_vertical_scroll_offset + 1;

                auto end_col = 0_u32;
                auto [row_index, row_group] = screen.find_row(r + screen.visual_scroll_offset());
                auto const& row = row_group.rows()[row_index];
                for (auto [c, cell, text, graphics, hyperlink, multi_cell_info] : row_group.iterate_row(row_index)) {
                    if (c < m_horizontal_scroll_offset || cell.is_nonprimary_in_multi_cell()) {
                        continue;
                    }

                    if (!cell.stale || !row.stale || whole_screen_dirty) {
                        auto selected = !text.empty() && screen.in_selection({ r + screen.visual_scroll_offset(),
                                                                               c - m_horizontal_scroll_offset });
                        auto gfx = graphics;
                        if (terminal.reverse_video()) {
                            gfx.inverted = !gfx.inverted;
                        }
                        if (selected) {
                            // Taken from catpuccin mocha
                            gfx.fg = Color(0xcd, 0xd6, 0xf4);
                            gfx.bg = Color(0x58, 0x5b, 0x70);
                            gfx.inverted = false;
                        }
                        renderer.put_cell(text, r - m_vertical_scroll_offset, c - m_horizontal_scroll_offset, gfx,
                                          hyperlink, multi_cell_info, cell.explicitly_sized,
                                          cell.complex_grapheme_cluster);
                        cell.stale = true;
                    }
                    end_col = c - m_horizontal_scroll_offset + 1;
                }
                // Clear any blank cols after the terminal.
                if (end_col < terminal.visible_size().cols) {
                    for (auto c : di::range(end_col, terminal.visible_size().cols)) {
                        renderer.put_cell(""_sv, r - m_vertical_scroll_offset, c,
                                          { .inverted = terminal.reverse_video() }, {},
                                          terminal::narrow_multi_cell_info, false, false);
                    }
                }
                row.stale = true;
            }

            // Clear any blank rows after the terminal.
            if (end_row < terminal.visible_size().rows) {
                for (auto r : di::range(end_row, terminal.visible_size().rows)) {
                    renderer.clear_row(r, { .inverted = terminal.reverse_video() });
                }
            }
            screen.clear_whole_screen_dirty_flag();
        }

        auto absolute_cursor_position = screen.absolute_row_screen_start() + terminal.cursor_row();
        auto relative_cursor_offset = u32(screen.absolute_row_screen_start() - screen.visual_scroll_offset());
        return RenderedCursor {
            .cursor_row = terminal.cursor_row() - m_vertical_scroll_offset + relative_cursor_offset,
            .cursor_col = terminal.cursor_col() - m_horizontal_scroll_offset,
            .style = terminal.cursor_style(),
            .hidden = terminal.cursor_hidden() || !terminal.allowed_to_draw() ||
                      terminal.cursor_row() < m_vertical_scroll_offset ||
                      terminal.cursor_row() - m_vertical_scroll_offset >= terminal.visible_size().rows ||
                      absolute_cursor_position < screen.visual_scroll_offset() ||
                      absolute_cursor_position >= screen.visual_scroll_offset() + terminal.visible_size().rows ||
                      terminal.cursor_col() < m_horizontal_scroll_offset ||
                      terminal.cursor_col() - m_horizontal_scroll_offset >= terminal.visible_size().cols,

        };
    });
}

auto Pane::event(KeyEvent const& event) -> bool {
    auto [application_cursor_keys_mode, key_reporting_flags] = m_terminal.with_lock([&](Terminal& terminal) {
        return di::Tuple { terminal.application_cursor_keys_mode(), terminal.key_reporting_flags() };
    });

    auto serialized_event = ttx::serialize_key_event(event, application_cursor_keys_mode, key_reporting_flags);
    if (serialized_event) {
        m_terminal.with_lock([&](Terminal& terminal) {
            // Clear the selection and scroll to the bottom when sending keys to the application.
            terminal.active_screen().screen.visual_scroll_to_bottom();
            terminal.active_screen().screen.clear_selection();
            m_pending_selection_start = {};
        });

        (void) m_pty_controller.write_exactly(di::as_bytes(serialized_event.value().span()));
        return true;
    }
    return false;
}

auto Pane::event(MouseEvent const& event) -> bool {
    auto consecutive_clicks = m_mouse_click_tracker.track(event);

    auto [application_cursor_keys_mode, alternate_scroll_mode, mouse_protocol, mouse_encoding, shift_escape_options,
          in_alternate_screen_buffer, window_size, row_offset, selection] =
        m_terminal.with_lock([&](Terminal& terminal) {
            return di::Tuple {
                terminal.application_cursor_keys_mode(),
                terminal.alternate_scroll_mode(),
                terminal.mouse_protocol(),
                terminal.mouse_encoding(),
                terminal.shift_escape_options(),
                terminal.in_alternate_screen_buffer(),
                terminal.size(),
                terminal.visual_scroll_offset(),
                terminal.active_screen().screen.selection(),
            };
        });

    auto serialized_event =
        serialize_mouse_event(event, mouse_protocol, mouse_encoding, m_last_mouse_position,
                              { alternate_scroll_mode, application_cursor_keys_mode, in_alternate_screen_buffer },
                              shift_escape_options, window_size);
    m_last_mouse_position = event.position();
    if (serialized_event.has_value()) {
        (void) m_pty_controller.write_exactly(di::as_bytes(serialized_event.value().span()));
        return true;
    }

    // Support mouse scrolling.
    if (event.button() == MouseButton::ScrollUp && event.type() == MouseEventType::Press) {
        scroll(Direction::Vertical, -1);
        return true;
    }
    if (event.button() == MouseButton::ScrollDown && event.type() == MouseEventType::Press) {
        scroll(Direction::Vertical, 1);
        return true;
    }

    // Selection logic.
    auto scroll_adjusted_position =
        terminal::SelectionPoint { event.position().in_cells().y(), event.position().in_cells().x() };
    scroll_adjusted_position = {
        scroll_adjusted_position.row + m_vertical_scroll_offset + row_offset,
        scroll_adjusted_position.col + m_horizontal_scroll_offset,
    };
    if (event.button() == MouseButton::Left && event.type() == MouseEventType::Press) {
        // Start selection.
        m_terminal.with_lock([&](Terminal& terminal) {
            ASSERT_LT_EQ(consecutive_clicks, 3);
            ASSERT_GT_EQ(consecutive_clicks, 1);
            if (consecutive_clicks > 1) {
                m_pending_selection_start = {};
                terminal.active_screen().screen.begin_selection(
                    scroll_adjusted_position, terminal::Screen::BeginSelectionMode(consecutive_clicks - 1));
            } else {
                m_pending_selection_start = scroll_adjusted_position;
            }
        });
        return true;
    }

    if ((selection.has_value() || m_pending_selection_start.has_value()) && event.button() == MouseButton::Left &&
        event.type() == MouseEventType::Move) {
        m_terminal.with_lock([&](Terminal& terminal) {
            if (m_pending_selection_start) {
                terminal.active_screen().screen.begin_selection(m_pending_selection_start.value(),
                                                                terminal::Screen::BeginSelectionMode::Single);
                m_pending_selection_start = {};
            }
            terminal.active_screen().screen.update_selection(scroll_adjusted_position);
        });
        return true;
    }

    if (selection.has_value() && event.button() == MouseButton::Left && event.type() == MouseEventType::Release) {
        auto text = m_terminal.with_lock([&](Terminal& terminal) {
            auto result = terminal.active_screen().screen.selected_text();
            terminal.active_screen().screen.clear_selection();
            m_pending_selection_start = {};
            return result;
        });
        if (!text.empty() && m_hooks.did_selection) {
            // Simulate a selection as an OSC 52 request.
            auto osc52 = terminal::OSC52 {};
            (void) osc52.selections.push_back(terminal::SelectionType::Clipboard);
            osc52.data = di::Base64<>(di::as_bytes(text.span()) | di::to<di::Vector>());
            m_hooks.did_selection(di::move(osc52), true);
        }
        return true;
    }

    // Clear selection by default on other events.
    m_terminal.with_lock([&](Terminal& terminal) {
        terminal.active_screen().screen.clear_selection();
        m_pending_selection_start = {};
    });
    return false;
}

auto Pane::event(FocusEvent const& event) -> bool {
    auto [focus_event_mode] = m_terminal.with_lock([&](Terminal& terminal) {
        if (event.is_focus_out()) {
            terminal.active_screen().screen.clear_selection();
            m_pending_selection_start = {};
        }
        return di::Tuple { terminal.focus_event_mode() };
    });

    auto serialized_event = serialize_focus_event(event, focus_event_mode);
    if (serialized_event) {
        (void) m_pty_controller.write_exactly(di::as_bytes(serialized_event.value().span()));
        return true;
    }
    return false;
}

auto Pane::event(PasteEvent const& event) -> bool {
    auto [bracketed_paste_mode] = m_terminal.with_lock([&](Terminal& terminal) {
        terminal.active_screen().screen.clear_selection();
        m_pending_selection_start = {};
        return di::Tuple { terminal.bracked_paste_mode() };
    });

    auto serialized_event = serialize_paste_event(event, bracketed_paste_mode);
    (void) m_pty_controller.write_exactly(di::as_bytes(serialized_event.span()));
    return true;
}

void Pane::invalidate_all() {
    m_terminal.with_lock([&](Terminal& terminal) {
        terminal.invalidate_all();
    });
}

void Pane::resize(Size const& size) {
    reset_viewport_scroll();
    m_terminal.with_lock([&](Terminal& terminal) {
        terminal.set_visible_size(size);
    });
}

void Pane::scroll(Direction direction, i32 amount_in_cells) {
    if (direction == Direction::None) {
        return;
    }

    if (direction == Direction::Vertical) {
        m_terminal.with_lock([&](Terminal& terminal) {
            while (amount_in_cells < 0) {
                if (m_vertical_scroll_offset > 0) {
                    m_vertical_scroll_offset--;
                    terminal.invalidate_all();
                } else {
                    terminal.active_screen().screen.visual_scroll_up();
                }
                amount_in_cells++;
            }
            while (amount_in_cells > 0) {
                if (terminal.visible_size().rows < terminal.row_count() &&
                    m_vertical_scroll_offset < terminal.row_count() - terminal.visible_size().rows &&
                    terminal.active_screen().screen.visual_scroll_at_bottom()) {
                    m_vertical_scroll_offset++;
                    terminal.invalidate_all();
                } else {
                    terminal.active_screen().screen.visual_scroll_down();
                }
                amount_in_cells--;
            }
        });
    } else if (direction == Direction::Horizontal) {
        m_terminal.with_lock([&](Terminal& terminal) {
            while (amount_in_cells < 0) {
                if (m_horizontal_scroll_offset > 0) {
                    m_horizontal_scroll_offset--;
                    terminal.invalidate_all();
                }
                amount_in_cells++;
            }
            while (amount_in_cells > 0) {
                if (terminal.visible_size().cols < terminal.col_count() &&
                    m_horizontal_scroll_offset < terminal.col_count() - terminal.visible_size().cols) {
                    m_horizontal_scroll_offset++;
                    terminal.invalidate_all();
                }
                amount_in_cells--;
            }
        });
    }

    auto selection = m_terminal.with_lock([&](Terminal& terminal) {
        return terminal.active_screen().screen.selection();
    });
    if ((selection || m_pending_selection_start) && m_last_mouse_position) {
        // Simulate a mouse move event. Because we've just scrolled the screen and are
        // in the middle of a selection, we can be sure the user is holding the left
        // mouse button. By simulating this mouse move, we update the selection based
        // on the scrolling which just occurred. Use shift to ensure the mouse event
        // isn't sent to the application.
        event(MouseEvent(MouseEventType::Move, MouseButton::Left, m_last_mouse_position.value(), Modifiers::Shift));
    }
}

auto Pane::save_state(di::PathView path) -> di::Result<> {
    auto file = TRY(dius::open_sync(path, dius::OpenMode::WriteNew));
    auto contents = m_terminal.with_lock([&](Terminal& terminal) {
        return terminal.state_as_escape_sequences();
    });
    return file.write_exactly(di::as_bytes(contents.span()));
}

void Pane::send_clipboard(terminal::SelectionType selection_type, di::Vector<byte> data) {
    auto osc52 = terminal::OSC52 {};
    (void) osc52.selections.push_back(selection_type);
    osc52.data = di::Base64<>(di::move(data));

    auto string = osc52.serialize();
    (void) m_pty_controller.write_exactly(di::as_bytes(string.span()));
}

void Pane::stop_capture() {
    m_capture.store(false, di::MemoryOrder::Release);
}

void Pane::soft_reset() {
    m_terminal.with_lock([&](Terminal& terminal) {
        terminal.soft_reset();
    });

    if (m_restore_termios) {
        m_restore_termios();
    }
}

void Pane::update_cwd(terminal::OSC7&& path_with_hostname) {
    // First, validate the host name. This ensures we don't mistakenly
    // set the cwd in sesssions connected over ssh.
    auto expected_hostname = dius::system::get_hostname();
    if (path_with_hostname.hostname != expected_hostname) {
        return;
    }

    if (path_with_hostname.path == m_cwd) {
        return;
    }

    m_cwd = di::move(path_with_hostname).path;
    if (m_hooks.did_update_cwd) {
        m_hooks.did_update_cwd();
    }
}

void Pane::exit() {
    (void) m_process.signal(dius::Signal::Hangup);
}

void Pane::reset_viewport_scroll() {
    m_vertical_scroll_offset = m_horizontal_scroll_offset = 0;
}
}
