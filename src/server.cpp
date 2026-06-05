#include "server.h"

#include "di/execution/algorithm/first_successful.h"
#include "di/execution/algorithm/sync_wait.h"
#include "di/execution/algorithm/transform_error.h"
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

namespace ttx {
auto run_server(di::Path path) -> di::Result<> {
    auto runtime_dir = path.parent_path().value();
    auto created_dir =
        TRY(dius::filesystem::create_directory(runtime_dir, runtime_dir.parent_path().value())
                .transform_error(di::prefix_errorf("Failed to create runtime directory ({})"_sv, runtime_dir)));
    if (created_dir) {
        TRY(dius::filesystem::permissions(runtime_dir, dius::filesystem::Perms::OwnerAll)
                .transform_error(di::prefix_error("Faild to set runtime directory permissions"_sv)));
    }

    auto context =
        TRY(di::create<dius::IoContext>().transform_error(di::prefix_error("Failed to create io context"_sv)));
    auto scheduler = context.get_scheduler();

    namespace ex = di::execution;

    auto address = dius::net::UnixAddress(path.clone().take_underlying_string());
    auto bound = false;
    auto accept_task = ex::use_resources(
                           [&](auto listener_socket, auto scope) -> di::Task<> {
                               co_await (dius::bind(listener_socket, address.clone()) |
                                         ex::transform_error(di::prefix_error("Faild to bind socket"_sv)));
                               bound = true;
                               co_await dius::listen(listener_socket, 5);

                               for (;;) {
                                   co_await ex::use_resources(
                                       [&](auto socket) -> di::Task<> {
                                           co_return;
                                       },
                                       dius::accept(listener_socket));
                               }
                           },
                           dius::make_unix_socket(scheduler), di::make_deferred<di::CountingScope<>>()) |
                       ex::into_result;
    auto stop_task = ex::first_successful(dius::signalled(scheduler, dius::Signal::Terminate),
                                          dius::signalled(scheduler, dius::Signal::Interrupt)) |
                     ex::into_result;

    auto _ = di::ScopeExit([&] {
        if (bound) {
            (void) dius::filesystem::remove(path);
        }
    });
    return ex::sync_wait_on(context, ex::first_successful(di::move(accept_task), di::move(stop_task))).value();
}
}
