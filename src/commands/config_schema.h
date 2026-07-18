#pragma once

#include "di/cli/prelude.h"

namespace ttx {
struct ConfigSchema {
    bool help { false };

    constexpr static auto get_cli_parser() {
        return di::cli_parser<ConfigSchema>("schema"_tsv, "Print the JSON schema for ttx configuration"_sv).help();
    }
};

auto main(ConfigSchema& args) -> di::Result<>;
}
