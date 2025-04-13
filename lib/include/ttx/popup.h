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

namespace detail {
    struct RelatizeSizeTag {
        using Type = i64;
    };
    struct AbsoluteSizeTag {
        using Type = u32;
    };
}

using RelatizeSize = di::StrongInt<detail::RelatizeSizeTag>;
using AbsoluteSize = di::StrongInt<detail::AbsoluteSizeTag>;

using PopupSize = di::Variant<RelatizeSize, AbsoluteSize>;

struct PopupLayout {
    PopupAlignment alignment { PopupAlignment::Center };
    PopupSize width { RelatizeSize(max_layout_precision / 2) };  // 50% width default
    PopupSize height { RelatizeSize(max_layout_precision / 2) }; // 50% height default
};

struct Popup {
    di::Box<Pane> pane {};
    PopupLayout layout_config;

    auto layout(Size const& size) -> LayoutEntry;
};
}
