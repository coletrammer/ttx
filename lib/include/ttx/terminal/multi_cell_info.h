#pragma once

#include "di/reflect/prelude.h"
#include "di/types/prelude.h"

namespace ttx::terminal {
/// @brief Shared information for cells linked via text sizing protocol (OSC 66) or double width characters
struct MultiCellInfo {
    u8 scale { 1 }; ///< Vertiacl scale for the cell. This is applied multiplicatively to the width.
    u8 width { 0 }; ///< Width in cells. When specified as 0 via OSC 66, the width is inferred.
    u8 fractional_scale_numerator { 0 };   ///< Fractional scale numerator
    u8 fractional_scale_denominator { 0 }; ///< Fractional scale denominator
    u8 vertical_alignment { 0 };           ///< Vertical fractional scale alignment
    u8 horizontal_alignment { 0 };         ///< Horizontal fractional scale alignment

    constexpr auto compute_width() const -> u8 { return scale * width; }

    auto operator==(MultiCellInfo const&) const -> bool = default;
    auto operator<=>(MultiCellInfo const&) const = default;

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<MultiCellInfo>) {
        return di::make_fields<"Hyperlink">(
            di::field<"scale", &MultiCellInfo::scale>, di::field<"width", &MultiCellInfo::width>,
            di::field<"fractional_scale_numerator", &MultiCellInfo::fractional_scale_numerator>,
            di::field<"fractional_scale_denominator", &MultiCellInfo::fractional_scale_denominator>,
            di::field<"vertical_alignment", &MultiCellInfo::vertical_alignment>,
            di::field<"horizontal_alignment", &MultiCellInfo::horizontal_alignment>);
    }
};

// Strictly speaking, this isn't a multicell. However, its a reasonable default value.
constexpr inline auto narrow_multi_cell_info = MultiCellInfo { 1, 1 };

constexpr inline auto wide_multi_cell_info = MultiCellInfo { 1, 2 };
}
