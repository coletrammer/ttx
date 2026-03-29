#pragma once

#include "di/reflect/prelude.h"

namespace ttx::terminal {
/// @brief Represents a request to set the current window title
///
/// Currently we only support UTF-8 window titles but xterm has extensions
/// to configure different formats for the window title. The parse() function
/// could handle this in the future.
struct OSC2 {
    di::String window_title;

    static auto parse(di::TransparentStringView data) -> di::Optional<OSC2>;

    auto clone() const -> OSC2 { return { window_title.clone() }; }

    auto serialize() const -> di::String;

    auto operator==(OSC2 const& other) const -> bool = default;

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<OSC2>) {
        return di::make_fields<"OSC2">(di::field<"window_title", &OSC2::window_title>);
    }
};
}
