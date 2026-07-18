#pragma once

#include "di/cli/prelude.h"

namespace ttx {
struct ConfigNix {
    bool help { false };

    constexpr static auto get_cli_parser() {
        return di::cli_parser<ConfigNix>("nix"_tsv, "Print the nix option schema for ttx configuration"_sv).help();
    }
};

auto main(ConfigNix& args) -> di::Result<>;
}
