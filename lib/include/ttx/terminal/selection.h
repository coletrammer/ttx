#pragma once

#include "di/container/algorithm/minmax.h"
#include "ttx/terminal/absolute_position.h"
#include "ttx/terminal/reflow_result.h"

namespace ttx::terminal {
/// @brief Represents the visual selection of a terminal
struct Selection {
    AbsolutePosition start; ///< The start of the selection
    AbsolutePosition end;   ///< The end of the selection (inclusive)

    /// @brief Normalize the selection so that start <= end
    constexpr auto normalize() const -> Selection {
        auto [s, e] = di::minmax({ start, end });
        return { s, e };
    }

    void apply_reflow_result(ReflowResult const& reflow_result) {
        start = reflow_result.map_position(start);
        end = reflow_result.map_position(end);
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
