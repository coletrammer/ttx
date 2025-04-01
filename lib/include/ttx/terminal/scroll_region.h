#pragma once

#include "di/reflect/prelude.h"
#include "di/types/prelude.h"

namespace ttx::terminal {
/// @brief Represents the scrolling region for a terminal.
///
/// The vertical scroll region is set via DECSTBM, while the horizontal margin
/// is set via DECSLRM. Currently, only vertical margins are supported, since
/// they are significantly more common. By default, the scroll region is the
/// entire screen.
///
/// The scroll region affects how commands like insert lines and delete lines
/// function, as well as scrolling induced via text auto-wrapping or cursor
/// movement (C0 reverse index and line feed). Lines outside the scroll region
/// are not affected by scrolling.
struct ScrollRegion {
    u32 start_row { 0 }; ///< Represents the first row in the scroll region.
    u32 end_row { 0 };   ///< Represents the last row in the scroll region (exclusive!).

    // Delete the default constructor because the default scroll region depends on the
    // terminal size.
    ScrollRegion() = delete;

    constexpr ScrollRegion(u32 start_row, u32 end_row) : start_row(start_row), end_row(end_row) {}

    auto operator==(ScrollRegion const&) const -> bool = default;
    auto operator<=>(ScrollRegion const&) const = default;

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<ScrollRegion>) {
        return di::make_fields<"ScrollRegion">(di::field<"start_row", &ScrollRegion::start_row>,
                                               di::field<"end_row", &ScrollRegion::end_row>);
    }
};
}
