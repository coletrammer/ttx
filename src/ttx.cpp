#include "di/cli/parser.h"
#include "di/container/string/string_view.h"
#include "di/io/writer_print.h"
#include "di/sync/synchronized.h"
#include "dius/filesystem/operations.h"
#include "dius/main.h"
#include "dius/sync_file.h"
#include "dius/system/process.h"
#include "input.h"
#include "layout_state.h"
#include "render.h"
#include "save_layout.h"
#include "ttx/features.h"
#include "ttx/terminal/capability.h"

namespace ttx {
struct Args {
    di::Vector<di::TransparentStringView> command;
    Key prefix { Key::B };
    bool hide_status_bar { false };
    bool print_keybinds { false };
    di::Optional<di::PathView> save_state_path;
    di::Optional<di::PathView> capture_command_output_path;
    di::Optional<di::TransparentStringView> layout_save_name;
    di::Optional<di::TransparentStringView> layout_restore_name;
    di::Optional<di::TransparentStringView> print_terminfo_mode;
    di::Optional<di::TransparentStringView> term;
    ClipboardMode clipboard_mode { ClipboardMode::System };
    bool replay { false };
    bool headless { false };
    bool print_features { false };
    bool force_local_terminfo { false };
    bool help { false };

    constexpr static auto get_cli_parser() {
        return di::cli_parser<Args>("ttx"_sv, "Terminal multiplexer"_sv)
            .option<&Args::prefix>('p', "prefix"_tsv, "Prefix key for key bindings"_sv)
            .option<&Args::hide_status_bar>('s', "hide-status-bar"_tsv, "Hide the status bar"_sv)
            .option<&Args::print_keybinds>('k', "keybinds"_tsv, "Print key bindings"_sv)
            .option<&Args::capture_command_output_path>('c', "capture-command-output-path"_tsv,
                                                        "Capture command output to a file"_sv)
            .option<&Args::print_features>('f', "features"_tsv, "Print out detected terminal features"_sv)
            .option<&Args::save_state_path>('S', "save-state-path"_tsv,
                                            "Save state path when triggering saving a pane's state"_sv)
            .option<&Args::headless>('h', "headless"_tsv, "Headless mode"_sv)
            .option<&Args::replay>('r', "replay-path"_tsv,
                                   "Replay capture output (file paths are passed via positional args)"_sv)
            .option<&Args::term>('t', "term"_tsv, "Set TERM environment variable (default xterm-ttx)"_sv)
            .option<&Args::clipboard_mode>({}, "clipboard"_tsv, "Set the clipboard mode"_sv)
            .option<&Args::print_terminfo_mode>({}, "terminfo"_tsv,
                                                "Print terminfo (mode can be one of: [terminfo, verbose])"_sv)
            .option<&Args::force_local_terminfo>(
                {}, "force-local-terminfo"_tsv,
                "Always try and compile built-in terminfo, and set TERMINFO env variable"_sv)
            .option<&Args::layout_save_name>(
                'l', "layout-save"_tsv,
                "Name of a saved layout, automatically synced by ttx (including restore at startup)"_sv)
            .option<&Args::layout_restore_name>('R', "layout-restore"_tsv,
                                                "Name of a saved layout, to be restored on startup"_sv)
            .argument<&Args::command>("COMMAND"_sv, "Program to run in terminal"_sv)
            .help();
    }
};

static auto get_session_save_dir() -> di::Result<di::Path> {
    auto const& env = dius::system::get_environment();
    auto data_home = env.at("XDG_DATA_HOME"_tsv)
                         .transform([&](di::TransparentStringView path) {
                             return di::PathView(path).to_owned();
                         })
                         .or_else([&] {
                             return env.at("HOME"_tsv).transform([&](di::TransparentStringView home) {
                                 return di::PathView(home).to_owned() / ".local"_tsv / "share"_tsv;
                             });
                         });
    if (!data_home) {
        return di::Unexpected(di::BasicError::NoSuchFileOrDirectory);
    }

    auto& result = data_home.value();
    result /= "ttx"_tsv;
    result /= "layouts"_tsv;
    return di::move(result);
}

static auto get_local_terminfo_dir() -> di::Result<di::Path> {
    auto const& env = dius::system::get_environment();
    auto data_home = env.at("XDG_STATE_HOME"_tsv)
                         .transform([&](di::TransparentStringView path) {
                             return di::PathView(path).to_owned();
                         })
                         .or_else([&] {
                             return env.at("HOME"_tsv).transform([&](di::TransparentStringView home) {
                                 return di::PathView(home).to_owned() / ".local"_tsv / "state"_tsv;
                             });
                         });
    if (!data_home) {
        return di::Unexpected(di::BasicError::NoSuchFileOrDirectory);
    }

    auto& result = data_home.value();
    result /= "ttx"_tsv;
    result /= "terminfo"_tsv;
    return di::move(result);
}

static auto maybe_get_terminfo_dir(di::Optional<di::TransparentStringView> term, bool force_local_terminfo)
    -> di::Result<di::Optional<di::Path>> {
    // If the user is overriding TERM, don't setup our terminfo.
    if (term.has_value() && term != "ttx"_tsv && term != "xterm-ttx"_tsv) {
        return {};
    }

    if (!force_local_terminfo) {
        // First, start by searching for an existing terminfo for ttx. We could try and implement
        // this check ourselves, but its probably better to rely on the actual curses implementation.
        // This does slow down start-up time, but we can rework this logic later on. For now, this
        // is very convenient.
        auto null = TRY(dius::open_sync("/dev/null"_pv, dius::OpenMode::ReadWrite));
        auto process_result = TRY(dius::system::Process(di::Array {
                                                            "tput"_ts,
                                                            "-T"_ts,
                                                            "xterm-ttx"_ts,
                                                            "colors"_ts,
                                                        } |
                                                        di::to<di::Vector>())
                                      .with_file_dup(null.file_descriptor(), 1)
                                      .with_file_dup(null.file_descriptor(), 2)
                                      .spawn_and_wait());
        if (process_result.exited() && process_result.exit_code() == 0) {
            return {};
        }
    }

    // In this case, we're going to compile our terminfo ourselves and then return the
    // PATH to it. We will store the data in $XDG_STATE_HOME/ttx/terminfo.
    auto terminfo_dir = TRY(get_local_terminfo_dir());
    TRY(dius::filesystem::create_directories(terminfo_dir));

    // To avoid recessive recompilations, hash our serialized terminfo and see if we're already written
    // it out.
    auto const& terminfo = terminal::get_ttx_terminfo();
    auto serialized_terminfo = terminfo.serialize();
    auto terminfo_hash = di::hash(serialized_terminfo);
    if (auto result = dius::read_to_string(terminfo_dir.clone() / "ttx.terminfo.hash"_pv)) {
        if (result.value() == di::to_string(terminfo_hash)) {
            return terminfo_dir;
        }
    }

    auto terminfo_file = TRY(dius::open_sync(terminfo_dir.clone() / "ttx.terminfo"_pv, dius::OpenMode::WriteClobber));
    di::writer_print<di::String::Encoding>(terminfo_file, "{}"_sv, serialized_terminfo);

    auto null = TRY(dius::open_sync("/dev/null"_pv, dius::OpenMode::ReadWrite));
    auto process_result = TRY(dius::system::Process(di::Array {
                                                        "tic"_ts,
                                                        "-x"_ts,
                                                        "-o"_ts,
                                                        terminfo_dir.data().to_owned(),
                                                        (terminfo_dir.clone() / "ttx.terminfo"_pv).data().to_owned(),
                                                    } |
                                                    di::to<di::Vector>())
                                  .with_file_dup(null.file_descriptor(), 1)
                                  .with_file_dup(null.file_descriptor(), 2)
                                  .spawn_and_wait());
    if (!process_result.exited() || process_result.exit_code() != 0) {
        return di::Unexpected(di::BasicError::InvalidArgument);
    }

    auto terminfo_hash_file =
        TRY(dius::open_sync(terminfo_dir.clone() / "ttx.terminfo.hash"_pv, dius::OpenMode::WriteClobber));
    di::writer_print<di::String::Encoding>(terminfo_hash_file, "{}"_sv, di::to_string(terminfo_hash));

    return terminfo_dir;
}

static auto main(Args& args) -> di::Result<void> {
    auto const replay_mode = args.replay;
    auto key_binds = make_key_binds(
        args.prefix, args.save_state_path.value_or("/tmp/ttx-save-state.ansi"_pv).to_owned(), replay_mode);
    if (args.print_keybinds) {
        for (auto const& bind : key_binds) {
            dius::println("{}"_sv, bind);
        }
        return {};
    }

    if (args.print_terminfo_mode) {
        auto const& terminfo = terminal::get_ttx_terminfo();
        if (args.print_terminfo_mode == "terminfo"_tsv) {
            dius::print("{}"_sv, terminfo.serialize());
            return {};
        }
        if (args.print_terminfo_mode == "verbose"_tsv) {
            dius::println("{}: {}"_sv, di::Styled("Names"_sv, di::FormatEffect::Bold),
                          terminfo.names | di::transform(di::to_string) | di::join_with(", "_sv) |
                              di::to<di::String>());

            for (auto const& capability : terminfo.capabilities | di::filter(&terminal::Capability::enabled)) {
                dius::println("\t{: <32}{: <90}{: <80}"_sv, di::Styled(capability.long_name, di::FormatEffect::Bold),
                              capability.description, capability.serialize());
            }
            return {};
        }
        return di::Unexpected(di::BasicError::InvalidArgument);
    }

    auto features = Feature::All;
    if (!args.headless) {
        features = TRY(detect_features(dius::stdin));
    }
    if (args.print_features) {
        dius::println("Feature: {}"_sv, features);
        return {};
    }

    args.hide_status_bar |= replay_mode;
    if (args.command.empty()) {
        if (!args.replay) {
            dius::eprintln("error: ttx requires a command argument to know what to launch"_sv);
            return di::Unexpected(di::BasicError::InvalidArgument);
        }
        dius::eprintln("error: ttx requires at least 1 argument to know what file to replay"_sv);
        return di::Unexpected(di::BasicError::InvalidArgument);
    }
    auto command = args.command | di::transform(di::to_owned) | di::to<di::Vector>();

    // Setup - log to file.
    [[maybe_unused]] auto& log = dius::stderr = TRY(dius::open_sync("/tmp/ttx.log"_pv, dius::OpenMode::WriteClobber));

    // Setup - potentially compile terminfo database
    auto maybe_terminfo_dir = TRY(maybe_get_terminfo_dir(args.term, args.force_local_terminfo));

    // Setup - initialize pane arguments
    auto base_create_pane_args = CreatePaneArgs {
        .command = command.clone(),
        .capture_command_output_path = args.capture_command_output_path.transform(di::to_owned),
        .save_state_path = args.save_state_path.transform(di::to_owned),
        .terminfo_dir = di::move(maybe_terminfo_dir),
        .term = args.term.value_or("xterm-ttx"_tsv),
    };

    // Setup - in headless mode there is no terminal. Ensure stdin is not valid.
    if (args.headless) {
        (void) dius::stdin.close();
    }

    // Setup - initial state and terminal size.
    auto initial_size = args.headless ? Size { 24, 80, 24 * 16, 80 * 16 }
                                      : Size::from_window_size(TRY(dius::stdin.get_tty_window_size()));
    auto layout_state = di::Synchronized(LayoutState(initial_size, args.hide_status_bar));

    // Setup - raw mode
    auto _ = args.headless ? di::ScopeExit(di::Function<void()>([] {})) : TRY(dius::stdin.enter_raw_mode());

    // Setup - block SIGWINCH.
    TRY(dius::system::mask_signal(dius::Signal::WindowChange));

    // Callback to exit the main thread.
    auto done = di::Atomic<bool>(false);
    auto set_done = [&] {
        if (!done.exchange(true, di::MemoryOrder::Release)) {
            // Ensure the SIGWINCH (main) thread exits.
            (void) dius::system::ProcessHandle::self().signal(dius::Signal::WindowChange);
        }
    };

    // Setup - layout save thread.
    auto session_save_dir = TRY(get_session_save_dir());
    auto layout_save_thread = TRY([&] -> di::Result<di::Box<SaveLayoutThread>> {
        if (args.headless) {
            return nullptr;
        }
        return SaveLayoutThread::create(layout_state, session_save_dir.clone(),
                                        args.layout_save_name.transform(di::to_owned));
    }());
    auto _ = di::ScopeExit([&] {
        if (layout_save_thread) {
            layout_save_thread->request_exit();
        }
    });
    if (layout_save_thread && args.layout_save_name) {
        layout_state.get_assuming_no_concurrent_accesses().set_layout_did_update([&] {
            layout_save_thread->request_save_layout();
        });
    }

    // Setup - render thread.
    auto render_thread = TRY(RenderThread::create(layout_state, set_done, args.clipboard_mode, features));
    auto _ = di::ScopeExit([&] {
        render_thread->request_exit();
    });

    // Setup - input thread.
    auto input_thread = TRY([&] -> di::Result<di::Box<InputThread>> {
        if (args.headless) {
            return nullptr;
        }
        return InputThread::create(base_create_pane_args.clone(), di::move(key_binds), layout_state, features,
                                   *render_thread, *layout_save_thread);
    }());
    auto _ = di::ScopeExit([&] {
        if (input_thread) {
            input_thread->request_exit();
        }
    });

    // Setup - remove all panes and tabs on exit.
    auto _ = di::ScopeExit([&] {
        layout_state.lock()->set_layout_did_update(nullptr);
        layout_state.with_lock([&](LayoutState& state) {
            while (!state.empty()) {
                auto& session = *state.sessions().front();
                while (!session->empty()) {
                    auto last_tab = session->tabs().size() == 1;
                    auto& tab = **session->tabs().front();
                    for (auto* pane : tab.panes()) {
                        state.remove_pane(*session, tab, pane);
                    }
                    // We must explicitly check this because the session object is destroyed
                    // after the last tab is removed.
                    if (last_tab) {
                        break;
                    }
                }
            }
        });
    });

    // Setup - initial tab and pane.
    auto exit_status = 0;
    TRY(layout_state.with_lock([&](LayoutState& state) -> di::Result<> {
        if (replay_mode) {
            for (auto replay_path : args.command) {
                auto create_pane_args = base_create_pane_args.clone();
                create_pane_args.replay_path = di::PathView(replay_path).to_owned();
                if (state.empty()) {
                    TRY(state.add_session(di::move(create_pane_args), *render_thread));
                } else {
                    // Horizontal split (means vertical layout)
                    TRY(state.add_pane(*state.active_session(), *state.active_tab(), di::move(create_pane_args),
                                       Direction::Vertical, *render_thread));
                }
            }
        } else {
            auto make_pane_args = [&] -> CreatePaneArgs {
                auto result = base_create_pane_args.clone();
                if (args.headless) {
                    result.hooks.did_exit = [&](Pane&, di::Optional<dius::system::ProcessResult> result) {
                        if (result && result.value().exited() && result.value().exit_code() == 0) {
                            exit_status = 0;
                        } else {
                            exit_status = 1;
                        }
                        render_thread->request_exit();
                    };
                }
                return result;
            };

            // Attempt to restore layout when running in auto-layout mode, or when specifically requested.
            if (args.layout_save_name || args.layout_restore_name) {
                auto path = session_save_dir.clone();
                path /= args.layout_restore_name.has_value() ? args.layout_restore_name.value()
                                                             : args.layout_save_name.value();
                path += ".json"_tsv;

                // Ignore errors, like the file not existing, when in auto-layout mode.
                auto file = dius::open_sync(path, dius::OpenMode::Readonly);
                if (file) {
                    auto string = TRY(di::read_to_string(file.value()));
                    auto json = TRY(di::from_json_string<json::Layout>(string));
                    TRY(state.restore_json(json, make_pane_args(), *render_thread));
                } else if (args.layout_restore_name) {
                    return di::Unexpected(di::move(file).error());
                }
            }

            if (state.empty()) {
                TRY(state.add_session(make_pane_args(), *render_thread));
            }
        }
        return {};
    }));

    // In headless and replay mode, exit immediately.
    if (args.headless && replay_mode) {
        return {};
    }

    render_thread->request_render();

#ifndef __linux__
    // On MacOS, we need to install a useless signal handlers for sigwait() to
    // actually work...
    dius::system::install_dummy_signal_handler(dius::Signal::WindowChange);
#endif

    while (!done.load(di::MemoryOrder::Acquire)) {
        if (!dius::system::wait_for_signal(dius::Signal::WindowChange)) {
            break;
        }
        if (done.load(di::MemoryOrder::Acquire)) {
            break;
        }

        auto size = dius::stdin.get_tty_window_size();
        if (!size) {
            continue;
        }

        render_thread->push_event(Size::from_window_size(size.value()));
    }

    if (exit_status) {
        return di::Unexpected(di::BasicError::InvalidArgument);
    }
    return {};
}
}

DIUS_MAIN(ttx::Args, ttx)
