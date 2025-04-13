#include "ttx/popup.h"

#include "ttx/layout.h"
#include "ttx/size.h"

namespace ttx {
struct ResolveSize {
    static auto operator()(Size const& total_size, RelatizeSize dimension, bool width) -> u32 {
        auto size_dimension = width ? total_size.cols : total_size.rows;
        return u32((di::Rational(dimension.raw_value(), max_layout_precision) * size_dimension).round());
    }

    static auto operator()(Size const&, AbsoluteSize dimension, bool) -> u32 { return dimension.raw_value(); }
};

auto Popup::layout(Size const& size) -> LayoutEntry {
    auto resolve_size = di::bind_front(ResolveSize {}, size);
    auto cols = di::visit(di::bind_back(resolve_size, true), layout_config.width);
    auto rows = di::visit(di::bind_back(resolve_size, false), layout_config.height);
    cols = di::clamp(cols, 1_u32, size.cols);
    rows = di::clamp(rows, 1_u32, size.rows);

    auto empty_rows = di::max(size.rows - rows, 0_u32);
    auto empty_cols = di::max(size.cols - cols, 0_u32);

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
