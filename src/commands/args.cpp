#include "args.h"

namespace ttx {
auto main(Args& args) -> di::Result<> {
    return di::visit(
        [&](auto& subcommand) {
            return main(subcommand);
        },
        args.subcommand);
}
}
