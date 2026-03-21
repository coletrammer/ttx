#pragma once

#include "di/reflect/prelude.h"
#include "di/types/integers.h"

namespace ttx::terminal {
struct Color {
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
        Brown,
        Blue,
        Magenta,
        Cyan,
        LightGrey,
        DarkGrey,
        LightRed,
        LightGreen,
        Yellow,
        LightBlue,
        LightMagenta,
        LightCyan,
        White,
    };

    Color() = default;
    constexpr Color(Palette c) : type(Type::Palette), r(c) {}
    constexpr Color(u8 r, u8 g, u8 b) : type(Type::Custom), r(r), g(g), b(b) {}

    Type type = Type::Default;
    u8 r = 0;
    u8 g = 0;
    u8 b = 0;

    auto operator==(Color const& other) const -> bool = default;
    auto operator<=>(Color const& other) const = default;

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<Color>) {
        return di::make_fields<"Color">(di::field<"type", &Color::type>, di::field<"r", &Color::r>,
                                        di::field<"g", &Color::g>, di::field<"b", &Color::b>);
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
        di::enumerator<"Brown", Brown>, di::enumerator<"Blue", Blue>, di::enumerator<"Magenta", Magenta>,
        di::enumerator<"Cyan", Cyan>, di::enumerator<"LightGrey", LightGrey>, di::enumerator<"DarkGrey", DarkGrey>,
        di::enumerator<"LightRed", LightRed>, di::enumerator<"LightGreen", LightGreen>,
        di::enumerator<"Yellow", Yellow>, di::enumerator<"LightBlue", LightBlue>,
        di::enumerator<"LightMagenta", LightMagenta>, di::enumerator<"LightCyan", LightCyan>,
        di::enumerator<"White", White>);
}
}
