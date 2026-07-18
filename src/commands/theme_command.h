#pragma once

#include "commands/theme_apply_local_palette.h"
#include "commands/theme_detect_dark_light_mode.h"
#include "commands/theme_list.h"
#include "commands/theme_show.h"
#include "commands/theme_show_local_palette.h"
#include "di/cli/prelude.h"

namespace ttx {
struct ThemeCommand {
    di::Variant<di::Void, ThemeList, ThemeApplyLocalPalette, ThemeShowLocalPalette, ThemeDetectDarkLightMode, ThemeShow>
        subcommand;
    bool help { false };

    constexpr static auto get_cli_parser() {
        return di::cli_parser<ThemeCommand>("theme"_tsv, "Act on the available ttx themes"_sv)
            .subcommands<&ThemeCommand::subcommand>()
            .help();
    }
};

auto main(ThemeCommand& args) -> di::Result<>;
}
