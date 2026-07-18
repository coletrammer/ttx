#include "theme_command.h"

namespace ttx {
auto main(ThemeCommand& args) -> di::Result<> {
    return di::visit(
        [&](auto& subcommand) -> di::Result<> {
            if constexpr (di::SameAs<di::Void, di::meta::RemoveCVRef<decltype(subcommand)>>) {
                ASSERT(false);
                return {};
            } else {
                return main(subcommand);
            }
        },
        args.subcommand);
}
}
