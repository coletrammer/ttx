#include "di/cli/parser.h"
#include "di/container/string/string_view.h"
#include "di/io/writer_print.h"
#include "di/sync/synchronized.h"
#include "dius/main.h"
#include "dius/sync_file.h"
#include "dius/system/process.h"
#include "input.h"
#include "layout_state.h"
#include "render.h"
#include "save_layout.h"

namespace ttx {
struct Args {
    di::Vector<di::TransparentStringView> command;
    Key prefix { Key::B };
    bool hide_status_bar { false };
    bool print_keybinds { false };
    di::Optional<di::PathView> save_state_path;
    di::Optional<di::PathView> capture_command_output_path;
    di::Optional<di::TransparentStringView> layout_save_name;
    bool replay { false };
    bool headless { false };
    bool help { false };

    constexpr static auto get_cli_parser() {
        return di::cli_parser<Args>("ttx"_sv, "Terminal multiplexer"_sv)
            .option<&Args::prefix>('p', "prefix"_tsv, "Prefix key for key bindings"_sv)
            .option<&Args::hide_status_bar>('s', "hide-status-bar"_tsv, "Hide the status bar"_sv)
            .option<&Args::print_keybinds>('k', "keybinds"_tsv, "Print key bindings"_sv)
            .option<&Args::capture_command_output_path>('c', "capture-command-output-path"_tsv,
                                                        "Capture command output to a file"_sv)
            .option<&Args::save_state_path>('S', "save-state-path"_tsv,
                                            "Save state path when triggering saving a pane's state"_sv)
            .option<&Args::headless>('h', "headless"_tsv, "Headless mode"_sv)
            .option<&Args::replay>('r', "replay-path"_tsv,
                                   "Replay capture output (file paths are passed via positional args)"_sv)
            .option<&Args::layout_save_name>('l', "layout-save"_tsv,
                                             "Name of a saved layout, automatically synced by ttx"_sv)
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
    auto layout_save_thread = TRY([&] -> di::Result<di::Box<SaveLayoutThread>> {
        return TRY(SaveLayoutThread::create(layout_state, TRY(get_session_save_dir()),
                                            args.layout_save_name.transform(di::to_owned)));
    }());
    auto _ = di::ScopeExit([&] {
        layout_save_thread->request_exit();
    });
    if (args.layout_save_name) {
        layout_state.get_assuming_no_concurrent_accesses().set_layout_did_update([&] {
            layout_save_thread->request_save_layout();
        });
    }

    // Setup - render thread.
    auto render_thread = TRY(RenderThread::create(layout_state, set_done));
    auto _ = di::ScopeExit([&] {
        render_thread->request_exit();
    });

    // Setup - input thread.
    auto input_thread = TRY(
        InputThread::create(command.clone(), di::move(key_binds), layout_state, *render_thread, *layout_save_thread));
    auto _ = di::ScopeExit([&] {
        input_thread->request_exit();
    });

    // Setup - remove all panes and tabs on exit.
    auto _ = di::ScopeExit([&] {
        layout_state.lock()->set_layout_did_update(nullptr);
        layout_state.with_lock([&](LayoutState& state) {
            while (!state.empty()) {
                auto& session = *state.sessions().front();
                while (!session.empty()) {
                    auto last_tab = session.tabs().size() == 1;
                    auto& tab = **session.tabs().front();
                    for (auto* pane : tab.panes()) {
                        state.remove_pane(session, tab, pane);
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
    TRY(layout_state.with_lock([&](LayoutState& state) -> di::Result<> {
        if (replay_mode) {
            for (auto replay_path : args.command) {
                if (state.empty()) {
                    TRY(state.add_session(
                        {
                            .replay_path = di::PathView(replay_path).to_owned(),
                            .save_state_path = args.save_state_path.transform(di::to_owned),
                        },
                        *render_thread));
                } else {
                    // Horizontal split (means vertical layout)
                    TRY(state.add_pane(*state.active_session(), *state.active_tab(),
                                       { .replay_path = di::PathView(replay_path).to_owned() }, Direction::Vertical,
                                       *render_thread));
                }
            }
        } else {
            TRY(state.add_session(
                {
                    .command = di::clone(command),
                    .capture_command_output_path = args.capture_command_output_path.transform(di::to_owned),
                },
                *render_thread));
        }
        return {};
    }));

    // In headless mode, exit immediately.
    if (args.headless) {
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

    return {};
}
}

DIUS_MAIN(ttx::Args, ttx)
