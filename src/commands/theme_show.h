#pragma once

#include "di/cli/prelude.h"

namespace ttx {
struct ThemeShow {
    di::TransparentStringView name { "auto"_tsv };
    bool help { false };

    constexpr static auto get_cli_parser() {
        return di::cli_parser<ThemeShow>("show"_tsv, "Print the JSON configuration for ttx theme"_sv)
            .argument<&ThemeShow::name>("NAME"_sv, "The theme to show"_sv)
            .help();
    }
};

auto main(ThemeShow& args) -> di::Result<>;
}
