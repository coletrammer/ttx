#pragma once

#include "di/container/string/string.h"
#include "di/types/integers.h"
#include "di/vocab/optional/optional_forward_declaration.h"
#include "ttx/features.h"
#include "ttx/terminal/color.h"
#include "ttx/terminal/palette.h"

namespace ttx::terminal {
/// @brief Represents a color palette query or change
///
/// We support both the kitty extension [OSC 21](https://sw.kovidgoyal.net/kitty/color-stack),
/// as well as the legacy xterm escape sequences. Because the semantics between the two
/// protocols are similiar enough, we parse both protocols into a common representation.
///
/// When we query the outer terminal using this protocol we prefer OSC 21 if available
/// and will otherwise use the legacy protocol, but importantly not all colors we support
/// are specifiable via the legacy protocols.
struct OSC21 {
    constexpr static auto palette = 4u;
    constexpr static auto special = 5u;
    constexpr static auto dynamic_start = 10u;
    constexpr static auto dynamic_foreground = 10u;
    constexpr static auto dynamic_background = 11u;
    constexpr static auto dynamic_cursor = 12u;
    constexpr static auto dynamic_selection_background = 17u;
    constexpr static auto dynamic_selection_foreground = 19u;
    constexpr static auto dynamic_end = 19u;
    constexpr static auto reset_offset = 100u;
    constexpr static auto kitty = 21u;

    constexpr static auto is_valid_osc_number(u32 number) {
        return number == palette || number == special || (number >= dynamic_start && number <= dynamic_end) ||
               number == kitty || number == palette + reset_offset || number == special + reset_offset ||
               (number >= dynamic_start + reset_offset && number <= dynamic_end + reset_offset);
    }

    struct Request {
        bool query { false };
        PaletteIndex palette { PaletteIndex::Unknown };
        Color color {};
        di::Optional<di::String> kitty_color_name {};

        auto is_reset() const -> bool { return !query && color.is_default(); }

        auto operator==(Request const&) const -> bool = default;

        constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<Request>) {
            return di::make_fields<"OSC21::Request">(
                di::field<"query", &Request::query>, di::field<"palette", &Request::palette>,
                di::field<"color", &Request::color>, di::field<"kitty_color_name", &Request::kitty_color_name>);
        }
    };

    di::Vector<Request> requests;

    static auto parse(di::StringView data) -> di::Optional<OSC21>;

    auto serialize(Feature features) const -> di::String;

    auto operator==(OSC21 const&) const -> bool = default;

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<OSC21>) {
        return di::make_fields<"OSC21">(di::field<"requests", &OSC21::requests>);
    }
};
}
