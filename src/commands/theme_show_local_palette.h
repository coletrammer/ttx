#pragma once

#include "di/cli/prelude.h"

namespace ttx {
struct ThemeShowLocalPalette {
    bool help { false };

    constexpr static auto get_cli_parser() {
        return di::cli_parser<ThemeShowLocalPalette>("show-local-palette"_tsv,
                                                     "Show the terminal's local palette colors as a ttx theme"_sv)
            .help();
    }
};

auto main(ThemeShowLocalPalette& args) -> di::Result<>;
}
