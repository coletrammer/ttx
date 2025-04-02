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
#include "dius/tty.h"
#include "ttx/mouse.h"
#include "ttx/mouse_event.h"
#include "ttx/paste_event.h"
#include "ttx/renderer.h"
#include "ttx/terminal.h"
#include "ttx/utf8_stream_decoder.h"

namespace ttx {
static auto spawn_child(di::Vector<di::TransparentStringView> command, dius::SyncFile& pty, Size const& size)
    -> di::Result<dius::system::ProcessHandle> {
    auto tty_path = TRY(pty.get_psuedo_terminal_path());

#ifdef __linux__
    // On linux, we can set the terminal size in on the controlling pty. On MacOS, we need to do it in
    // the child. Doing so requires using fork() instead of posix_spawn() when spawning the process, so
    // we try to avoid it. Additionally, opening the psudeo terminal implicitly will make it the controlling
    // terminal, so there's no need to call ioctl(TIOCSCTTY).
    TRY(pty.set_tty_window_size(size.as_window_size()));
#endif

    return dius::system::Process(command | di::transform(di::to_owned) | di::to<di::Vector>())
        .with_new_session()
        .with_env("TERM"_ts, "xterm-256color"_ts)
        .with_env("COLORTERM"_ts, "truecolor"_ts)
        .with_file_open(0, di::move(tty_path), dius::OpenMode::ReadWrite)
        .with_file_dup(0, 1)
        .with_file_dup(0, 2)
#ifndef __linux__
        .with_tty_window_size(0, size)
        .with_controlling_tty(0)
#endif
        .spawn();
}

auto Pane::create_from_replay(di::PathView replay_path, di::Optional<di::Path> save_state_path, Size const& size,
                              di::Function<void(Pane&)> did_exit, di::Function<void(Pane&)> did_update,
                              di::Function<void(di::Span<byte const>)> did_selection,
                              di::Function<void(di::StringView)> apc_passthrough) -> di::Result<di::Box<Pane>> {
    auto replay_file = TRY(dius::open_sync(replay_path, dius::OpenMode::Readonly));

    // When replaying content, there is no need for any threads, a psudeo terminal or a sub-process.
    auto pane = di::make_box<Pane>(dius::SyncFile(), size, dius::system::ProcessHandle(), di::move(did_exit),
                                   di::move(did_update), di::move(did_selection), di::move(apc_passthrough));

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
        di::erase_if(parser_result, [&](auto const& event) {
            if (auto ev = di::get_if<APC>(event)) {
                if (pane->m_apc_passthrough) {
                    pane->m_apc_passthrough(ev->data);
                }
                return true;
            }
            return false;
        });

        auto events = pane->m_terminal.with_lock([&](Terminal& terminal) {
            terminal.on_parser_results(parser_result.span());
            return terminal.outgoing_events();
        });

        for (auto&& event : events) {
            di::visit(di::overload([&](SetClipboard&& ev) {
                          if (pane->m_did_selection) {
                              pane->m_did_selection(ev.data.span());
                          }
                      }),
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

auto Pane::create(CreatePaneArgs args, Size const& size, di::Function<void(Pane&)> did_exit,
                  di::Function<void(Pane&)> did_update, di::Function<void(di::Span<byte const>)> did_selection,
                  di::Function<void(di::StringView)> apc_passthrough) -> di::Result<di::Box<Pane>> {
    if (args.replay_path) {
        return create_from_replay(*args.replay_path, di::move(args.save_state_path), size, di::move(did_exit),
                                  di::move(did_update), di::move(did_selection), di::move(apc_passthrough));
    }

    auto capture_file = di::Optional<dius::SyncFile> {};
    if (args.capture_command_output_path) {
        capture_file = TRY(dius::open_sync(args.capture_command_output_path.value(), dius::OpenMode::WriteClobber));

        // Write out inital terminal size information.
        di::writer_print<di::String::Encoding>(capture_file.value(), "\033[4;{};{}t"_sv, size.ypixels, size.xpixels);
        di::writer_print<di::String::Encoding>(capture_file.value(), "\033[8;{};{}t"_sv, size.rows, size.cols);
    }

    auto pty_controller = TRY(dius::open_psuedo_terminal_controller(dius::OpenMode::ReadWrite));
    auto process = TRY(spawn_child(di::move(args.command), pty_controller, size));
    auto pane = di::make_box<Pane>(di::move(pty_controller), size, process, di::move(did_exit), di::move(did_update),
                                   di::move(did_selection), di::move(apc_passthrough));

    pane->m_process_thread = TRY(dius::Thread::create([&pane = *pane] mutable {
        auto guard = di::ScopeExit([&] {
            pane.m_done.store(true, di::MemoryOrder::Release);

            if (pane.m_did_exit) {
                pane.m_did_exit(pane);
            }
        });

        (void) pane.m_process.wait();
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
                di::erase_if(parser_result, [&](auto const& event) {
                    if (auto ev = di::get_if<APC>(event)) {
                        if (pane.m_apc_passthrough) {
                            pane.m_apc_passthrough(ev->data);
                        }
                        return true;
                    }
                    return false;
                });

                // (void) di::writer_println<di::String::Encoding>(dius::stderr, "{:?}"_sv, utf8_string);
                // (void) di::writer_println<di::String::Encoding>(dius::stderr, "{}"_sv, parser_result);

                auto events = pane.m_terminal.with_lock([&](Terminal& terminal) {
                    terminal.on_parser_results(parser_result.span());
                    return terminal.outgoing_events();
                });

                for (auto&& event : events) {
                    di::visit(di::overload([&](SetClipboard&& ev) {
                                  if (pane.m_did_selection) {
                                      pane.m_did_selection(ev.data.span());
                                  }
                              }),
                              di::move(event));
                }

                if (pane.m_did_update) {
                    pane.m_did_update(pane);
                }
            }
        }));

    return pane;
}

auto Pane::create_mock() -> di::Box<Pane> {
    auto fake_psuedo_terminal = dius::SyncFile();
    return di::make_box<Pane>(di::move(fake_psuedo_terminal), Size(1, 1), dius::system::ProcessHandle(), nullptr,
                              nullptr, nullptr, nullptr);
}

Pane::~Pane() {
    // TODO: timeout/skip waiting for processes to die after sending SIGHUP.
    (void) m_process.signal(dius::Signal::Hangup);
    (void) m_reader_thread.join();
    (void) m_process_thread.join();
}

auto Pane::draw(Renderer& renderer) -> RenderedCursor {
    return m_terminal.with_lock([&](Terminal& terminal) {
        if (terminal.allowed_to_draw()) {
            auto& screen = terminal.active_screen().screen;
            for (auto r : di::range(screen.max_height())) {
                for (auto [c, cell, text, graphics, hyperlink] : screen.iterate_row(r)) {
                    if (r < m_vertical_scroll_offset || c < m_horizontal_scroll_offset) {
                        continue;
                    }

                    if (text.empty()) {
                        text = " "_sv;
                    }

                    if (!cell.stale) {
                        // TODO: selection
                        auto gfx = graphics;
                        if (terminal.reverse_video()) {
                            gfx.inverted = !gfx.inverted;
                        }
                        renderer.put_cell(text, r - m_vertical_scroll_offset, c - m_horizontal_scroll_offset, gfx);
                        cell.stale = true;
                    }
                }
                // Clear any blank cols after the terminal.
                for (auto c :
                     di::range(screen.max_width() - m_horizontal_scroll_offset, terminal.visible_size().cols)) {
                    renderer.put_cell(" "_sv, r, c, { .inverted = terminal.reverse_video() });
                }
            }

            // Clear any blank rows after the terminal.
            for (auto r : di::range(screen.max_height() - m_vertical_scroll_offset, terminal.visible_size().rows)) {
                renderer.clear_row(r, { .inverted = terminal.reverse_video() });
            }
        }
        return RenderedCursor {
            .cursor_row = terminal.cursor_row() - m_vertical_scroll_offset,
            .cursor_col = terminal.cursor_col() - m_horizontal_scroll_offset,
            .style = terminal.cursor_style(),
            .hidden = terminal.cursor_hidden() || !terminal.allowed_to_draw() ||
                      terminal.cursor_row() < m_vertical_scroll_offset ||
                      terminal.cursor_row() - m_vertical_scroll_offset >= terminal.visible_size().rows ||
                      terminal.cursor_col() < m_horizontal_scroll_offset ||
                      terminal.cursor_col() - m_horizontal_scroll_offset >= terminal.visible_size().cols,
        };
    });
}

auto Pane::event(KeyEvent const& event) -> bool {
    // Clear the selection and scrolling on key presses that send text.
    if (!event.text().empty()) {
        clear_selection();
    }

    auto [application_cursor_keys_mode, key_reporting_flags] = m_terminal.with_lock([&](Terminal& terminal) {
        return di::Tuple { terminal.application_cursor_keys_mode(), terminal.key_reporting_flags() };
    });

    auto serialized_event = ttx::serialize_key_event(event, application_cursor_keys_mode, key_reporting_flags);
    if (serialized_event) {
        (void) m_pty_controller.write_exactly(di::as_bytes(serialized_event.value().span()));
        return true;
    }
    return false;
}

auto Pane::event(MouseEvent const& event) -> bool {
    auto [application_cursor_keys_mode, alternate_scroll_mode, mouse_protocol, mouse_encoding,
          in_alternate_screen_buffer, window_size, row_offset] = m_terminal.with_lock([&](Terminal& terminal) {
        return di::Tuple {
            terminal.application_cursor_keys_mode(),
            terminal.alternate_scroll_mode(),
            terminal.mouse_protocol(),
            terminal.mouse_encoding(),
            terminal.in_alternate_screen_buffer(),
            terminal.size(),
            terminal.row_offset(),
        };
    });

    auto serialized_event = serialize_mouse_event(
        event, mouse_protocol, mouse_encoding, m_last_mouse_position,
        { alternate_scroll_mode, application_cursor_keys_mode, in_alternate_screen_buffer }, window_size);
    if (serialized_event.has_value()) {
        (void) m_pty_controller.write_exactly(di::as_bytes(serialized_event.value().span()));
        return true;
    }
    m_last_mouse_position = event.position();

    // Support mouse scrolling.
    if (event.button() == MouseButton::ScrollUp && event.type() == MouseEventType::Press) {
        scroll(Direction::Vertical, -1);
        m_terminal.with_lock([&](Terminal& terminal) {
            terminal.scroll_up();
        });
        return true;
    }
    if (event.button() == MouseButton::ScrollDown && event.type() == MouseEventType::Press) {
        scroll(Direction::Vertical, 1);
        return true;
    }

    // Selection logic.
    auto scroll_adjusted_position = event.position().in_cells();
    scroll_adjusted_position = { scroll_adjusted_position.x() + m_horizontal_scroll_offset,
                                 scroll_adjusted_position.y() + m_vertical_scroll_offset + row_offset };
    if (event.button() == MouseButton::Left && event.type() == MouseEventType::Press) {
        // Start selection.
        m_selection_start = m_selection_end = scroll_adjusted_position;
        return true;
    }

    if (m_selection_start.has_value() && event.button() == MouseButton::Left && event.type() == MouseEventType::Move) {
        m_selection_end = scroll_adjusted_position;
        return true;
    }

    if (m_selection_start.has_value() && event.button() == MouseButton::Left &&
        event.type() == MouseEventType::Release) {
        auto text = selection_text();
        if (!text.empty() && m_did_selection) {
            m_did_selection(di::as_bytes(text.span()));
        }
        clear_selection();
        return true;
    }

    // Clear selection by default on other events.
    clear_selection();
    return false;
}

auto Pane::event(FocusEvent const& event) -> bool {
    auto [focus_event_mode] = m_terminal.with_lock([&](Terminal& terminal) {
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
    clear_selection();

    auto [bracketed_paste_mode] = m_terminal.with_lock([&](Terminal& terminal) {
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
    clear_selection();
    reset_scroll();
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
                    terminal.scroll_up();
                }
                amount_in_cells++;
            }
            while (amount_in_cells > 0) {
                if (terminal.visible_size().rows < terminal.row_count() &&
                    m_vertical_scroll_offset < terminal.row_count() - terminal.visible_size().rows) {
                    m_vertical_scroll_offset++;
                    terminal.invalidate_all();
                } else {
                    terminal.scroll_down();
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
}

auto Pane::save_state(di::PathView path) -> di::Result<> {
    auto file = TRY(dius::open_sync(path, dius::OpenMode::WriteNew));
    auto contents = m_terminal.with_lock([&](Terminal& terminal) {
        return terminal.state_as_escape_sequences();
    });
    return file.write_exactly(di::as_bytes(contents.span()));
}

void Pane::stop_capture() {
    m_capture.store(false, di::MemoryOrder::Release);
}

void Pane::exit() {
    (void) m_process.signal(dius::Signal::Hangup);
}

auto Pane::in_selection(MouseCoordinate coordinate) -> bool {
    if (!m_selection_start || !m_selection_end || m_selection_start == m_selection_end) {
        return false;
    }

    auto comparator = [](MouseCoordinate const& a, MouseCoordinate const& b) {
        return di::Tuple { a.y(), a.x() } <=> di::Tuple { b.y(), b.x() };
    };
    auto start = di::min({ *m_selection_start, *m_selection_end }, comparator);
    auto end = di::max({ *m_selection_start, *m_selection_end }, comparator);

    auto row = coordinate.y();
    auto col = coordinate.x();
    if (row > start.y() && row < end.y()) {
        return true;
    }

    if (row == start.y()) {
        return col >= start.x() && (row == end.y() ? col < end.x() : true);
    }

    return row == end.y() && col < end.x();
}

auto Pane::selection_text() -> di::String {
    if (!m_selection_start || !m_selection_end || m_selection_start == m_selection_end) {
        return {};
    }

    auto comparator = [](MouseCoordinate const& a, MouseCoordinate const& b) {
        return di::Tuple { a.y(), a.x() } <=> di::Tuple { b.y(), b.x() };
    };
    auto start = di::min({ *m_selection_start, *m_selection_end }, comparator);
    auto end = di::max({ *m_selection_start, *m_selection_end }, comparator);

    return m_terminal.with_lock([&](Terminal& terminal) -> di::String {
        auto text = ""_s;
        // TODO: selection text with scroll back
        // for (auto r = start.y(); r <= end.y(); r++) {
        //     auto row_text = ""_s;
        //     auto iter_start_col = r == start.y() ? start.x() : 0;
        //     auto iter_end_col = r == end.y() ? end.x() : terminal.col_count();
        //
        // for (auto c = iter_start_col; c < iter_end_col; c++) {
        //     row_text.push_back(terminal.row_at_scroll_relative_offset(r)[c].ch);
        // }

        //     while (!row_text.empty() && *row_text.back() == ' ') {
        //         row_text.pop_back();
        //     }
        //
        //     text += row_text;
        //     if (iter_end_col == terminal.col_count()) {
        //         text.push_back('\n');
        //     }
        // }
        return text;
    });
}

void Pane::clear_selection() {
    m_selection_start = m_selection_end = {};
}

void Pane::reset_scroll() {
    m_vertical_scroll_offset = m_horizontal_scroll_offset = 0;
}
}
