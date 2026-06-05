#include "client.h"

#include "config.h"
#include "di/container/string/utf8_stream_decoder.h"
#include "di/execution/algorithm/first_successful.h"
#include "di/execution/algorithm/sync_wait.h"
#include "di/execution/algorithm/use_resources.h"
#include "di/execution/coroutine/task.h"
#include "di/execution/scope/counting_scope.h"
#include "di/function/monad/monad_interface.h"
#include "di/vocab/error/string_error.h"
#include "dius/filesystem/operations.h"
#include "dius/filesystem/query/exists.h"
#include "dius/io.h"
#include "dius/io_context.h"
#include "dius/print.h"
#include "ttx/features.h"
#include "ttx/terminal_input.h"

namespace ttx {
static auto input_task(auto stdin_token, Feature features) -> di::Task<> {
    auto parser = TerminalInputParser {};
    auto utf8_decoder = di::Utf8StreamDecoder {};

    auto buffer = di::Vector<byte> {};
    buffer.resize(16384);
    for (;;) {
        auto nread = co_await dius::read_some(stdin_token, buffer.span());
        if (nread == 0) {
            break;
        }

        auto utf8_string = utf8_decoder.decode(buffer | di::take(nread));
        auto events = parser.parse(utf8_string, features);

        for (auto& event : events) {
            if (auto ev = di::get_if<KeyEvent>(event)) {
                co_return;
            }
        }
    }
}

auto run_client(Config config, Feature features) -> di::Result<> {
    auto context = TRY(di::create<dius::IoContext>().transform_error([](di::Error error) {
        return di::format_error("Failed to create io context: {}"_sv, error);
    }));
    auto scheduler = context.get_scheduler();

    // Setup - raw mode
    auto _ = TRY(dius::std_in.enter_raw_mode());

    namespace ex = di::execution;

    auto task = ex::use_resources(
        [&](auto stdin_token) {
            return input_task(stdin_token, features);
        },
        dius::adopt_file(scheduler, dius::SyncFile(dius::SyncFile::Owned::No, 0)));

    return ex::sync_wait_on(context, di::move(task));
}
}
