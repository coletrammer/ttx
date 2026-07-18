#include "server_start.h"

#include "paths.h"
#include "server.h"

namespace ttx {
auto main(ServerStart& args) -> di::Result<> {
    auto socket_path = get_runtime_dir();
    socket_path /= args.session;
    return run_server(di::move(socket_path));
}
}
