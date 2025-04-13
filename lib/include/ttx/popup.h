#pragma once

#include "ttx/layout.h"
#include "ttx/size.h"

namespace ttx {
enum class PopupAlignment {
    Left,
    Right,
    Top,
    Bottom,
    Center,
};

struct PopupLayout {
    PopupAlignment alignment { PopupAlignment::Center };
    i64 relative_width { max_layout_precision / 2 };  // 50% width default
    i64 relative_height { max_layout_precision / 2 }; // 50% height default
};

struct Popup {
    di::Box<Pane> pane {};
    PopupLayout layout_config;

    auto layout(Size const& size) -> LayoutEntry;
};
}
