#include "di/cli/parser.h"
#include "di/container/ring/ring.h"
#include "di/container/string/string_view.h"
#include "di/io/writer_print.h"
#include "di/serialization/base64.h"
#include "di/sync/synchronized.h"
#include "di/util/construct.h"
#include "dius/condition_variable.h"
#include "dius/main.h"
#include "dius/sync_file.h"
#include "dius/system/process.h"
#include "dius/thread.h"
#include "dius/tty.h"
#include "input.h"
#include "layout_state.h"
#include "render.h"
#include "ttx/focus_event.h"
#include "ttx/layout.h"
#include "ttx/pane.h"
#include "ttx/terminal_input.h"
#include "ttx/utf8_stream_decoder.h"

namespace ttx {
struct Args {
    di::Vector<di::TransparentStringView> command;
    Key prefix { Key::B };
    bool hide_status_bar { false };
    bool print_keybinds { false };
    di::Optional<di::PathView> save_state_path;
    di::Optional<di::PathView> capture_command_output_path;
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
            .argument<&Args::command>("COMMAND"_sv, "Program to run in terminal"_sv)
            .help();
    }
};

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

    // Setup - log to file.
    [[maybe_unused]] auto& log = dius::stderr = TRY(dius::open_sync("/tmp/ttx.log"_pv, dius::OpenMode::WriteClobber));

    // Setup - in headless mode there is no terminal. Ensure stdin is not valid.
    if (args.headless) {
        (void) dius::stdin.close();
    }

    // Setup - initial state and terminal size.
    auto initial_size =
        args.headless ? dius::tty::WindowSize { 25, 80, 25 * 16, 80 * 16 } : TRY(dius::stdin.get_tty_window_size());
    auto layout_state = di::Synchronized(LayoutState(initial_size, args.hide_status_bar));

    // Setup - raw mode
    auto _ = args.headless ? di::ScopeExit(di::Function<void()>([] {})) : TRY(dius::stdin.enter_raw_mode());

    // Setup - alternate screen buffer.
    di::writer_print<di::String::Encoding>(dius::stdin, "\033[?1049h\033[H\033[2J"_sv);
    auto _ = di::ScopeExit([&] {
        di::writer_print<di::String::Encoding>(dius::stdin, "\033[?1049l\033[?25h"_sv);
    });

    // Setup - disable autowrap.
    di::writer_print<di::String::Encoding>(dius::stdin, "\033[?7l"_sv);
    auto _ = di::ScopeExit([&] {
        di::writer_print<di::String::Encoding>(dius::stdin, "\033[?7h"_sv);
    });

    // Setup - kitty key mode.
    di::writer_print<di::String::Encoding>(dius::stdin, "\033[>31u"_sv);
    auto _ = di::ScopeExit([&] {
        di::writer_print<di::String::Encoding>(dius::stdin, "\033[<u"_sv);
    });

    // Setup - capture all mouse events and use SGR mosue reporting.
    di::writer_print<di::String::Encoding>(dius::stdin, "\033[?1003h\033[?1006h"_sv);
    auto _ = di::ScopeExit([&] {
        di::writer_print<di::String::Encoding>(dius::stdin, "\033[?1006l\033[?1003l"_sv);
    });

    // Setup - enable focus events.
    di::writer_print<di::String::Encoding>(dius::stdin, "\033[?1004h"_sv);
    auto _ = di::ScopeExit([&] {
        di::writer_print<di::String::Encoding>(dius::stdin, "\033[?1004l"_sv);
    });

    // Setup - bracketed paste.
    di::writer_print<di::String::Encoding>(dius::stdin, "\033[?2004h"_sv);
    auto _ = di::ScopeExit([&] {
        di::writer_print<di::String::Encoding>(dius::stdin, "\033[?2004l"_sv);
    });

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

    // Setup - render thread.
    auto render_thread = TRY(RenderThread::create(layout_state, set_done));
    auto _ = di::ScopeExit([&] {
        render_thread->request_exit();
    });

    // Setup - input thread.
    auto input_thread =
        TRY(InputThread::create(di::clone(args.command), di::move(key_binds), layout_state, *render_thread));
    auto _ = di::ScopeExit([&] {
        input_thread->request_exit();
    });

    // Setup - remove all panes and tabs on exit.
    auto _ = di::ScopeExit([&] {
        layout_state.with_lock([&](LayoutState& state) {
            while (!state.empty()) {
                auto& tab = **state.tabs().front();
                for (auto* pane : tab.panes()) {
                    state.remove_pane(tab, pane);
                }
            }
        });
    });

    // Setup - initial tab and pane.
    if (replay_mode) {
        auto& state = layout_state.get_assuming_no_concurrent_accesses();
        for (auto replay_path : args.command) {
            if (state.empty()) {
                TRY(state.add_tab(
                    {
                        .replay_path = di::PathView(replay_path).to_owned(),
                        .save_state_path = args.save_state_path.transform(di::to_owned),
                    },
                    *render_thread));
            } else {
                // Horizontal split (means vertical layout)
                TRY(state.add_pane(*state.active_tab(), { .replay_path = di::PathView(replay_path).to_owned() },
                                   Direction::Vertical, *render_thread));
            }
        }
    } else {
        TRY(layout_state.get_assuming_no_concurrent_accesses().add_tab(
            {
                .command = di::clone(args.command),
                .capture_command_output_path = args.capture_command_output_path.transform(di::to_owned),
            },
            *render_thread));
    }

    // In headless mode, exit immediately.
    if (args.headless) {
        return {};
    }

    render_thread->request_render();

    // For testing, exit immediately after writing the replaying content and writing the save state.
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

        render_thread->push_event(size.value());
    }

    return {};
}
}

DIUS_MAIN(ttx::Args, ttx)
