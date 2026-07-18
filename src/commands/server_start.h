#pragma once

#include "di/cli/prelude.h"

namespace ttx {
struct ServerStart {
    di::TransparentStringView session { "main"_tsv };
    bool help { false };

    constexpr static auto get_cli_parser() {
        return di::cli_parser<ServerStart>("start"_tsv, "Start a ttx server instance"_sv)
            .argument<&ServerStart::session>("SESSION"_sv, "The session identifier for the server instance"_sv)
            .help();
    }
};

auto main(ServerStart& args) -> di::Result<>;
}
