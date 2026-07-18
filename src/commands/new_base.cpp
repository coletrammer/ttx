#include "new_base.h"

#include "commands/new_base.h"
#include "config.h"
#include "config_json.h"
#include "di/container/hash/hash.h"
#include "di/container/string/conversion.h"
#include "di/container/string/string_view.h"
#include "di/io/writer_print.h"
#include "di/sync/synchronized.h"
#include "di/vocab/error/string_error.h"
#include "dius/filesystem/operations.h"
#include "dius/sync_file.h"
#include "dius/system/process.h"
#include "input.h"
#include "layout_state.h"
#include "paths.h"
#include "render.h"
#include "save_layout.h"
#include "ttx/features.h"
#include "ttx/terminal/capability.h"

namespace ttx {
static auto maybe_get_terminfo_dir(di::TransparentStringView term, bool force_local_terminfo)
    -> di::Result<di::Optional<di::Path>> {
    // If the user is overriding TERM, don't setup our terminfo.
    if (term != "ttx"_tsv && term != "xterm-ttx"_tsv) {
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

    // To avoid redundant recompilations, hash our serialized terminfo and see if we're already written
    // it out.
    auto const& terminfo = terminal::get_ttx_terminfo();
    auto serialized_terminfo = terminfo.serialize();
    auto terminfo_hash = di::hash(serialized_terminfo);
    if (auto result = dius::read_to_string_sync(terminfo_dir.clone() / "ttx.terminfo.hash"_pv)) {
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

auto main(NewBase& args) -> di::Result<> {
    auto const replay_mode = !args.replay_paths.empty();
    if (args.headless || replay_mode) {
        args.profile = ""_tsv;
    }

    if (args.profile.ends_with('/')) {
        return di::Unexpected(di::format_error("--profile cannot be a directory"_sv));
    }
    auto features = FeatureResult { .features = Feature::All };
    if (!args.headless) {
        features = TRY(detect_features(dius::std_in).transform_error([](di::Error error) {
            return di::format_error("Failed to detect terminal features: {}"_sv, error);
        }));
    }
    auto config_from_args = config_json::v1::Config {
        .theme = {
            .name = args.theme.transform(di::to_utf8_string_lossy),
        },
        .input = {
            .prefix = args.prefix,
            .save_state_path = args.save_state_path.transform([](di::PathView path) { return di::to_utf8_string_lossy(path.data()); }),
        },
        .clipboard = {
            .mode = args.clipboard_mode,
        },
        .session = {
            .restore_layout = args.disable_layout_restore ? di::Optional(false) : di::nullopt,
            .save_layout = args.disable_layout_save ? di::Optional(false) : di::nullopt,
            .layout_name = args.layout_name.transform(di::to_utf8_string_lossy),
        },
        .shell = {
            .command = !args.command.empty () ? di::Optional(args.command | di::transform(di::to_utf8_string_lossy) | di::to<di::Vector>()) : di::nullopt,
        },
        .status_bar = {
            .hide = args.hide_status_bar,
        },
        .terminfo = {
            .term = args.term.transform(di::to_utf8_string_lossy),
            .force_local_terminfo = args.force_local_terminfo ? di::Optional(true) : di::nullopt,
        },
    };
    auto config = TRY(config_json::v1::resolve_profile(args.profile, features.theme_mode, features.palette,
                                                       di::clone(config_from_args))
                          .transform_error([&](auto&& error) {
                              return di::format_error("Failed to resolve profile '{}': {}"_sv, args.profile, error);
                          }));

    // Setup - log to file.
    [[maybe_unused]] auto& log = dius::std_err = TRY(dius::open_sync("/tmp/ttx.log"_pv, dius::OpenMode::WriteClobber));

    // Setup - potentially compile terminfo database (this config only applies at startup)
    auto maybe_terminfo_dir =
        TRY(maybe_get_terminfo_dir(config.terminfo.term.view(), config.terminfo.force_local_terminfo));

    // Setup - initialize pane arguments
    auto global_palette = config.colors;
    for (auto index_number : di::range(u32(terminal::PaletteIndex::Count))) {
        auto index = terminal::PaletteIndex(index_number);
        if (global_palette.get(index).is_default()) {
            global_palette.set(index, features.palette.get(index));
        }
    }
    auto base_create_pane_args = CreatePaneArgs {
        .command = config.shell.command.clone(),
        .capture_command_output_path = args.capture_command_output_path.transform(di::to_owned),
        .save_state_path = config.input.save_state_path.clone(),
        .terminfo_dir = di::move(maybe_terminfo_dir),
        .term = config.terminfo.term.clone(),
        .global_palette = global_palette,
        .theme_mode = features.theme_mode,
    };
    if (replay_mode) {
        base_create_pane_args.replay_path = di::PathView(args.replay_paths[0]).to_owned();
        if (!args.save_state_path) {
            base_create_pane_args.save_state_path = {};
        }
    }

    // Setup - in headless mode there is no terminal. Ensure stdin is not valid.
    if (args.headless) {
        (void) dius::std_in.close();
    }

    // Setup - initial state and terminal size.
    auto initial_size = args.headless ? Size { 24, 80, 24 * 16, 80 * 16 }
                                      : Size::from_window_size(TRY(dius::std_in.get_tty_window_size()));
    auto layout_state = di::Synchronized(LayoutState(initial_size, di::clone(config)));

    // Setup - raw mode
    auto _ = args.headless ? di::ScopeExit(di::Function<void()>([] {})) : TRY(dius::std_in.enter_raw_mode());

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
            return SaveLayoutThread::create_mock(layout_state);
        }
        return SaveLayoutThread::create(layout_state, session_save_dir.clone(), di::clone(config.session));
    }());
    auto _ = di::ScopeExit([&] {
        if (layout_save_thread) {
            layout_save_thread->request_exit();
        }
    });
    if (layout_save_thread) {
        // layout_state.get_assuming_no_concurrent_accesses().set_layout_did_update([&] {
        //     layout_save_thread->request_save_layout();
        // });
    }

    // Setup - render thread.
    auto render_thread =
        TRY(RenderThread::create(layout_state, set_done, di::clone(config), features.features, features.palette));
    auto _ = di::ScopeExit([&] {
        render_thread->request_exit();
    });

    // Setup - input thread.
    auto input_thread = TRY([&] -> di::Result<di::Box<InputThread>> {
        if (args.headless) {
            return InputThread::create_mock(layout_state, *render_thread, *layout_save_thread);
        }
        return InputThread::create(base_create_pane_args.clone(), di::clone(config), di::clone(config_from_args),
                                   args.profile, layout_state, features, *render_thread, *layout_save_thread);
    }());
    auto _ = di::ScopeExit([&] {
        if (input_thread) {
            input_thread->request_exit();
        }
    });

    // Setup - remove all panes and tabs on exit.
    auto _ = di::ScopeExit([&] {
        // layout_state.lock()->set_layout_did_update(nullptr);
        layout_state.with_lock([&](LayoutState& state) {
            if (state.active_popup()) {
                state.remove_popup();
            }
            while (!state.empty()) {
                auto& session = *state.workspaces().front();
                while (!session->empty()) {
                    auto last_tab = session->tabs().size() == 1;
                    // auto& tab = **session->tabs().front();
                    // for (auto* pane : tab.panes()) {
                    //     state.remove_pane(*session, tab, pane);
                    // }
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
            for (auto replay_path : args.replay_paths) {
                auto create_pane_args = base_create_pane_args.clone();
                create_pane_args.replay_path = di::PathView(replay_path).to_owned();
                // if (state.empty()) {
                // TRY(state.add_session(di::move(create_pane_args), *render_thread, *input_thread));
                // } else {
                // Horizontal split (means vertical layout)
                // TRY(state.add_pane(*state.active_workspace(), *state.active_tab(), di::move(create_pane_args),
                //                    Direction::Vertical, *render_thread, *input_thread));
                // }
            }
        } else {
            // auto make_pane_args = [&] -> CreatePaneArgs {
            //     auto result = base_create_pane_args.clone();
            //     if (args.headless) {
            //         result.hooks.did_exit = [&](Pane&, di::Optional<dius::system::ProcessResult> result) {
            //             if (result && result.value().exited() && result.value().exit_code() == 0) {
            //                 exit_status = 0;
            //             } else {
            //                 exit_status = 1;
            //             }
            //             render_thread->request_exit();
            //         };
            //     }
            //     return result;
            // };

            // Attempt to restore layout when running in auto-layout mode, or when specifically requested.
            if (config.session.restore_layout) {
                auto path = session_save_dir.clone();
                path /= config.session.layout_name;
                path += ".json"_tsv;

                // Ignore file not found errors.
                auto file = dius::open_sync(path, dius::OpenMode::Readonly);
                if (file) {
                    // auto string = TRY(di::read_to_string(file.value()));
                    // auto json = TRY(di::from_json_string<json::Layout>(string));
                    // TRY(state.restore_json(json, make_pane_args(), *render_thread, *input_thread));
                } else if (file.error() != dius::PosixError::NoSuchFileOrDirectory) {
                    return di::Unexpected(di::move(file).error());
                }
            }

            if (state.empty()) {
                // TRY(state.add_session(make_pane_args(), *render_thread, *input_thread));
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

        auto size = dius::std_in.get_tty_window_size();
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
