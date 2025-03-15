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
    bool help { false };

    constexpr static auto get_cli_parser() {
        return di::cli_parser<Args>("ttx"_sv, "Terminal multiplexer"_sv)
            .option<&Args::prefix>('p', "prefix"_tsv, "Prefix key for key bindings"_sv)
            .argument<&Args::command>("COMMAND"_sv, "Program to run in terminal"_sv, true)
            .help();
    }
};

static auto main(Args& args) -> di::Result<void> {
    // Setup - log to file.
    [[maybe_unused]] auto& log = dius::stderr = TRY(dius::open_sync("/tmp/ttx.log"_pv, dius::OpenMode::WriteClobber));

    // Setup - initial state and terminal size.
    auto initial_size = TRY(dius::stdin.get_tty_window_size());
    auto layout_state = di::Synchronized(LayoutState(initial_size));

    // Setup - raw mode
    auto _ = TRY(dius::stdin.enter_raw_mode());

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
    auto input_thread = TRY(InputThread::create(di::clone(args.command), args.prefix, layout_state, *render_thread));
    auto _ = di::ScopeExit([&] {
        input_thread->request_exit();
    });

    // Setup - initial tab and pane.
    TRY(layout_state.get_assuming_no_concurrent_accesses().add_tab(di::clone(args.command), *render_thread));

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
