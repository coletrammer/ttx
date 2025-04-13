#include "ttx/popup.h"

#include "ttx/layout.h"
#include "ttx/size.h"

namespace ttx {
auto Popup::layout(Size const& size) -> LayoutEntry {
    auto cols = (di::Rational(layout_config.relative_width, max_layout_precision) * size.cols).round();
    auto rows = (di::Rational(layout_config.relative_height, max_layout_precision) * size.rows).round();
    cols = di::max(cols, 1_i64);
    rows = di::max(rows, 1_i64);

    auto empty_rows = u32(di::max(size.rows - rows, 0_i64));
    auto empty_cols = u32(di::max(size.cols - cols, 0_i64));

    auto layout_size = Size(rows, cols, size.xpixels / cols, size.ypixels / rows);
    auto [r, c] = [&] -> di::Tuple<u32, u32> {
        switch (layout_config.alignment) {
            case PopupAlignment::Left:
                return { di::divide_round_up(empty_rows, 2u), 0 };
            case PopupAlignment::Right:
                return { di::divide_round_up(empty_rows, 2u), empty_cols };
            case PopupAlignment::Top:
                return { 0, di::divide_round_up(empty_cols, 2u) };
            case PopupAlignment::Bottom:
                return { empty_rows, di::divide_round_up(empty_cols, 2u) };
            case PopupAlignment::Center:
                return { di::divide_round_up(empty_rows, 2u), di::divide_round_up(empty_cols, 2u) };
        }
        return { 0, 0 };
    }();
    if (pane) {
        pane->resize(layout_size);
    }
    return LayoutEntry { r, c, layout_size, nullptr, pane.get() };
}
}
