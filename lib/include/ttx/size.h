#pragma once

#include "di/types/prelude.h"
#include "dius/tty.h"

namespace ttx {
struct Size {
    u32 rows { 0 };
    u32 cols { 0 };
    u32 xpixels { 0 };
    u32 ypixels { 0 };

    static auto from_window_size(dius::tty::WindowSize const& window_size) -> Size {
        return { window_size.rows, window_size.cols, window_size.pixel_width, window_size.pixel_height };
    }
    auto as_window_size() const -> dius::tty::WindowSize { return { rows, cols, xpixels, ypixels }; }

    auto rows_shrinked(u32 r) const -> Size {
        if (r >= rows) {
            return { 0, cols, xpixels, 0 };
        }
        return { rows - r, cols, xpixels, ypixels - (r * ypixels / rows) };
    }

    auto cols_shrinked(u32 c) const -> Size {
        if (c >= cols) {
            return { rows, 0, 0, ypixels };
        }
        return { rows, cols - c, xpixels - (c * xpixels / cols), ypixels };
    }

    auto operator==(Size const&) const -> bool = default;
    auto operator<=>(Size const&) const = default;

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<Size>) {
        return di::make_fields<"Size">(di::field<"rows", &Size::rows>, di::field<"cols", &Size::cols>,
                                       di::field<"xpixels", &Size::xpixels>, di::field<"ypixels", &Size::ypixels>);
    }
};
}
