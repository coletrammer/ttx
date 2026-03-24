#pragma once

#include <di/function/value.h>
#include <di/parser/basic/match_zero_or_more.h>

#include "di/reflect/prelude.h"
#include "di/types/integers.h"

namespace ttx::terminal {
struct Color {
    using IsAtom = void;

    enum class Type : u8 {
        Default, ///< Color is the default (unset SGR)
        Palette, ///< Color is a palette color (256 colors are available)
        Custom,  ///< Color is true color (r, b, g fully specified)
        Dynamic, ///< Color is dynamic (e.g. for selection use reverse video)
    };

    enum Palette : u8 {
        Black,
        Red,
        Green,
        Yellow,
        Blue,
        Magenta,
        Cyan,
        LightGrey,
        DarkGrey,
        LightRed,
        LightGreen,
        LightYellow,
        LightBlue,
        LightMagenta,
        LightCyan,
        White,
    };

    static auto from_name(di::StringView color) -> di::Result<Color>;
    static auto from_string(di::StringView color) -> di::Result<Color>;

    Color() = default;
    constexpr Color(Palette c) : type(Type::Palette), r(c) {}
    constexpr Color(u8 r, u8 g, u8 b) : type(Type::Custom), r(r), g(g), b(b) {}
    constexpr explicit Color(Type t) : type(t) {}

    Type type = Type::Default;
    u8 r = 0;
    u8 g = 0;
    u8 b = 0;

    auto is_default() const { return type == Type::Default; }
    auto is_palette() const { return type == Type::Palette; }
    auto is_custom() const { return type == Type::Custom; }
    auto is_dynamic() const { return type == Type::Dynamic; }

    auto value_or(Color other) const { return is_default() ? other : *this; }

    auto to_string() const -> di::String;

    auto operator==(Color const& other) const -> bool = default;
    auto operator<=>(Color const& other) const = default;

    constexpr friend auto tag_invoke(di::Tag<di::parser::create_parser_in_place>, di::InPlaceType<Color>) {
        return di::parser::match_zero_or_more(di::function::value(true))
                   << []<typename Context>(Context& context,
                                           di::concepts::CopyConstructible auto results) -> di::Result<Color> {
            using Enc = di::meta::Encoding<Context>;
            auto encoding = context.encoding();

            auto const input = di::container::string::StringViewImpl<Enc> {
                di::encoding::unicode_code_point_unwrap(encoding, results.begin()),
                di::encoding::unicode_code_point_unwrap(encoding, results.end())
            };

            return Color::from_string(input);
        };
    }
};

constexpr auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<Color::Type>) {
    using enum Color::Type;
    return di::make_enumerators<"Color::Type">(di::enumerator<"Default", Default>, di::enumerator<"Palette", Palette>,
                                               di::enumerator<"Custom", Custom>, di::enumerator<"Dynamic", Dynamic>);
}

constexpr auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<Color::Palette>) {
    using enum Color::Palette;
    return di::make_enumerators<"Color::Palette">(
        di::enumerator<"Black", Black>, di::enumerator<"Red", Red>, di::enumerator<"Green", Green>,
        di::enumerator<"Brown", Yellow>, di::enumerator<"Blue", Blue>, di::enumerator<"Magenta", Magenta>,
        di::enumerator<"Cyan", Cyan>, di::enumerator<"LightGrey", LightGrey>, di::enumerator<"DarkGrey", DarkGrey>,
        di::enumerator<"LightRed", LightRed>, di::enumerator<"LightGreen", LightGreen>,
        di::enumerator<"Yellow", LightYellow>, di::enumerator<"LightBlue", LightBlue>,
        di::enumerator<"LightMagenta", LightMagenta>, di::enumerator<"LightCyan", LightCyan>,
        di::enumerator<"White", White>);
}
}
