#pragma once

#include "di/cli/prelude.h"

namespace ttx {
struct ThemeApplyLocalPalette {
    di::TransparentStringView name { "ansi"_tsv };
    bool help { false };

    constexpr static auto get_cli_parser() {
        return di::cli_parser<ThemeApplyLocalPalette>(
                   "apply-local-palette"_tsv,
                   "Apply the specified theme to the current terminal's palette. This only affects the current terminal and only configures terminal specific colors"_sv)
            .argument<&ThemeApplyLocalPalette::name>("NAME"_sv, "The theme to apply"_sv)
            .help();
    }
};

auto main(ThemeApplyLocalPalette& args) -> di::Result<>;
}
