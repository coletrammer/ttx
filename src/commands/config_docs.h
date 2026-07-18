#pragma once

#include "di/cli/prelude.h"

namespace ttx {
struct ConfigDocs {
    bool help { false };

    constexpr static auto get_cli_parser() {
        return di::cli_parser<ConfigDocs>("docs"_tsv, "Print the makrdown documentation for ttx configuration"_sv)
            .help();
    }
};

auto main(ConfigDocs& args) -> di::Result<>;
}
