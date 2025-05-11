#pragma once

#include "di/container/algorithm/minmax.h"
#include "di/reflect/prelude.h"
#include "di/types/integers.h"

namespace ttx::terminal {
/// @brief Represents a coordinate of a visual selection
///
/// The row coordinate if measured in absolute units, and so
/// can refer to rows in the scroll back.
struct SelectionPoint {
    u64 row { 0 }; ///< The absolute row referenced by the selection
    u32 col { 0 }; ///< The column referenced by the selection

    auto operator==(SelectionPoint const&) const -> bool = default;
    auto operator<=>(SelectionPoint const&) const = default;

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<SelectionPoint>) {
        return di::make_fields<"SelectionPoint">(di::field<"row", &SelectionPoint::row>,
                                                 di::field<"col", &SelectionPoint::col>);
    }
};

/// @brief Represents the visual selection of a terminal
struct Selection {
    SelectionPoint start; ///< The start of the selection
    SelectionPoint end;   ///< The end of the selection (inclusive)

    /// @brief Normalize the selection so that start <= end
    constexpr auto normalize() const -> Selection {
        auto [s, e] = di::minmax({ start, end });
        return { s, e };
    }

    constexpr auto operator==(Selection const& other) const -> bool {
        auto a = this->normalize();
        auto b = other.normalize();
        return a.start == b.start && a.end == other.end;
    }

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<Selection>) {
        return di::make_fields<"Selection">(di::field<"start", &Selection::start>, di::field<"end", &Selection::end>);
    }
};
}
