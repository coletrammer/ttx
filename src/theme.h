#pragma once

#include "di/container/string/prelude.h"
#include "di/container/tree/tree_map.h"
#include "di/reflect/prelude.h"
#include "ttx/terminal/color.h"
#include "ttx/terminal/palette.h"

namespace ttx {
enum class ThemeSource {
    Custom,
    BuiltIn,
    Iterm2ColorSchemes,
    All,
};

constexpr static auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<ThemeSource>) {
    using enum ThemeSource;
    return di::make_enumerators<"ColorSchemeSource">(
        di::enumerator<"custom", Custom, "themes defined in configuration files">,
        di::enumerator<"built-in", BuiltIn, "themes built-in to ttx">,
        di::enumerator<"iterm2-color-schemes", Iterm2ColorSchemes,
                       "themes part of the iTerm2-Color-Schemes repository">,
        di::enumerator<"all", All, "All themes">);
}

struct ListedTheme {
    di::TransparentString name;
    ThemeSource source { ThemeSource::BuiltIn };
    terminal::ThemeMode theme_mode { terminal::ThemeMode::Dark };
    terminal::Palette palette;
};
}
