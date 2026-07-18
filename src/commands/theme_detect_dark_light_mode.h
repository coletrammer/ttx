#pragma once

#include "di/cli/prelude.h"

namespace ttx {
struct ThemeDetectDarkLightMode {
    bool help { false };

    constexpr static auto get_cli_parser() {
        return di::cli_parser<ThemeDetectDarkLightMode>("detect-dark-light-mode"_tsv,
                                                        "Detect whether the whether dark or light mode is preferred"_sv)
            .help();
    }
};

auto main(ThemeDetectDarkLightMode& args) -> di::Result<>;
}
