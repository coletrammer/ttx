#pragma once

#include "di/cli/prelude.h"

namespace ttx {
struct ConfigShow {
    di::TransparentStringView profile { "main"_tsv };
    di::Optional<di::TransparentStringView> theme;
    bool resolve_theme { false };
    bool help { false };

    constexpr static auto get_cli_parser() {
        return di::cli_parser<ConfigShow>(
                   "show"_tsv, "Print the resolved configuration for the profile as a valid JSON configuration file"_sv)
            .option<&ConfigShow::profile>(
                'p', "profile"_tsv, "Profile name to use for this session (empty string loads no configuration)"_sv)
            .option<&ConfigShow::resolve_theme>(
                {}, "resolve-theme"_tsv, "Resolve the theme and include the results in the displayed configuration"_sv)
            .option<&ConfigShow::theme>({}, "theme"_tsv, "Theme to use (overrides configuration if specified)"_sv)
            .help();
    }
};

auto main(ConfigShow& args) -> di::Result<>;
}
