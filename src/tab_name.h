#pragma once

#include "di/reflect/prelude.h"

namespace ttx {
class Tab;

enum class TabNameSource {
    Manual,
    WindowTitle,
    CurrentWorkingDirectory,
};

constexpr static auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<TabNameSource>) {
    using enum TabNameSource;
    return di::make_enumerators<"TabNameSource">(
        di::enumerator<"manual", Manual, "The name explicitly you explicitly set">,
        di::enumerator<"window-title", WindowTitle, "The window title set by the application via OSC 0 / OSC 2">,
        di::enumerator<
            "current-working-directory", CurrentWorkingDirectory,
            "The current working directory (not the full path - just the last part) set by the application via OSC 7">);
}

auto evaluate_tab_name(di::Span<TabNameSource const> sources, Tab& tab, usize index) -> di::String;
}
