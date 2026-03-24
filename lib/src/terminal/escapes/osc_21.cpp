#include "ttx/terminal/escapes/osc_21.h"

#include "di/format/prelude.h"
#include "di/parser/run_parser.h"
#include "di/util/construct.h"
#include "ttx/features.h"
#include "ttx/terminal/color.h"
#include "ttx/terminal/palette.h"

namespace ttx::terminal {
constexpr static auto kitty_color_names = di::Array {
    di::Tuple { "foreground"_sv, PaletteIndex::Foreground },
    di::Tuple { "background"_sv, PaletteIndex::Background },
    di::Tuple { "selection_foreground"_sv, PaletteIndex::SelectionForeground },
    di::Tuple { "selection_background"_sv, PaletteIndex::SelectionBackground },
    di::Tuple { "cursor"_sv, PaletteIndex::Cursor },
    di::Tuple { "cursor_text"_sv, PaletteIndex::CursorText },
};

static auto parse_color_hex(di::StringView color) -> di::Optional<Color> {
    auto result = di::run_parser(di::parser::integer<u64, di::parser::IntegerMode::Improved>(16), color);
    if (!result) {
        return {};
    }
    auto value = result.value();
    switch (color.size_bytes()) {
        case 3:
            return Color((u8(value >> 8) & 0xF) << 4, (u8(value >> 4) & 0xF) << 4, (u8(value) & 0xF) << 4);
        case 6:
            return Color(u8(value >> 16), u8(value >> 8), u8(value));
        case 9:
            return Color(u8(value >> 28), u8(value >> 16), u8(value >> 4));
        case 12:
            return Color(u8(value >> 40), u8(value >> 24), u8(value >> 8));
        default:
            return {};
    }
}

static auto parse_color_rgb(di::StringView color) -> di::Optional<Color> {
    auto parts = di::split(color, U'/') | di::to<di::Vector>();
    if (parts.size() != 3) {
        return {};
    }

    auto parse_part = [](di::StringView part) -> di::Optional<u8> {
        auto result = di::run_parser(di::parser::integer<u32, di::parser::IntegerMode::Improved>(16), part);
        if (!result) {
            return {};
        }
        auto value = result.value();
        switch (part.size_bytes()) {
            case 1:
                return u8((value << 4) + value);
            case 2:
                return u8(value);
            case 3:
                return u8(value >> 4);
            case 4:
                return u8(value >> 8);
            default:
                return {};
        }
    };

    return Color(TRY(parse_part(parts[0])), TRY(parse_part(parts[1])), TRY(parse_part(parts[2])));
}

static auto parse_color(di::StringView color) -> di::Optional<Color> {
    if (color.starts_with(U'#')) {
        return parse_color_hex(color.substr(di::next(color.begin())));
    }
    if (color.starts_with("rgb:"_sv)) {
        return parse_color_rgb(color.substr(di::next(color.begin(), 4)));
    }
    return Color::from_name(color).optional_value();
}

static auto parse_kitty(di::Span<di::StringView const> parts) -> di::Optional<OSC21> {
    auto result = OSC21 {};
    for (auto item : parts) {
        auto parts = di::split(item, U'=') | di::to<di::Vector>();
        if (parts.size() != 2) {
            continue;
        }
        auto request = OSC21::Request {};
        if (parts[1] == "?"_sv) {
            request.query = true;
        } else if (!parts[1].empty()) {
            auto c = parse_color(parts[1]);
            if (!c) {
                continue;
            }
            request.color = c.value();
        }
        auto palette_color = di::parse<u32>(parts[0]);
        if (!palette_color || palette_color.value() > u32(PaletteIndex::SpecialEnd)) {
            for (auto const& [name, palette] : kitty_color_names) {
                if (parts[0] == name) {
                    request.palette = palette;
                }
            }
            if (request.palette == PaletteIndex::Unknown) {
                request.kitty_color_name = parts[0].to_owned();
            }
        } else {
            request.palette = PaletteIndex(palette_color.value());
        }
        result.requests.push_back(di::move(request));
    }
    if (result.requests.empty()) {
        return {};
    }
    return result;
}

auto OSC21::parse(di::StringView data) -> di::Optional<OSC21> {
    if (data.empty()) {
        return {};
    }

    auto parts = data | di::split(U';') | di::to<di::Vector>();
    if (parts.empty()) {
        return {};
    }

    auto maybe_number = di::parse<u32>(parts[0]);
    if (!maybe_number) {
        return {};
    }

    auto result = OSC21 {};
    auto number = maybe_number.value();
    if (!OSC21::is_valid_osc_number(number)) {
        return {};
    }
    switch (number) {
        case palette:
        case special:
            for (auto [ns, cs] : parts | di::drop(1) | di::pairwise | di::stride(1)) {
                auto n = di::parse<u32>(ns);
                auto max = number == palette ? u32(PaletteIndex::SpecialEnd)
                                             : u32(PaletteIndex::SpecialEnd) - u32(PaletteIndex::SpecialBegin);
                if (!n.has_value() || n.value() > max) {
                    continue;
                }
                auto const palette_index =
                    PaletteIndex(n.value() + (number == palette ? 0_u32 : u32(PaletteIndex::SpecialBegin)));
                if (cs == "?"_sv) {
                    result.requests.push_back(Request {
                        .query = true,
                        .palette = palette_index,
                    });
                } else {
                    auto c = parse_color(cs);
                    if (!c) {
                        continue;
                    }
                    result.requests.push_back(Request {
                        .palette = palette_index,
                        .color = c.value(),
                    });
                }
            }
            if (result.requests.empty()) {
                return {};
            }
            return result;
        case palette + reset_offset:
        case special + reset_offset:
            for (auto ns : parts | di::drop(1)) {
                auto n = di::parse<u32>(ns);
                auto max = number - reset_offset == palette
                               ? u32(PaletteIndex::SpecialEnd)
                               : u32(PaletteIndex::SpecialEnd) - u32(PaletteIndex::SpecialBegin);
                if (!n.has_value() || n.value() > max) {
                    continue;
                }
                result.requests.push_back(Request {
                    .palette = PaletteIndex(
                        n.value() + (number - reset_offset == palette ? 0_u32 : u32(PaletteIndex::SpecialBegin))),
                });
            }
            if (result.requests.empty()) {
                return {};
            }
            return result;
        case kitty:
            return parse_kitty(parts | di::drop(1));
        default:
            break;
    }

    // OSC 110-119 reset dynamic color
    if (number > reset_offset) {
        // Xterm accepts a trailing ';' apparently.
        if (parts.size() > 2 || (parts.size() == 2 && !parts[1].empty())) {
            return {};
        }
        switch (number - reset_offset) {
            case dynamic_cursor:
                result.requests.push_back(Request { .palette = PaletteIndex::Cursor });
                break;
            case dynamic_selection_foreground:
                result.requests.push_back(Request { .palette = PaletteIndex::SelectionForeground });
                break;
            case dynamic_selection_background:
                result.requests.push_back(Request { .palette = PaletteIndex::SelectionBackground });
                break;
            case dynamic_background:
                result.requests.push_back(Request { .palette = PaletteIndex::Background });
                break;
            case dynamic_foreground:
                result.requests.push_back(Request { .palette = PaletteIndex::Foreground });
                break;
            default:
                return {};
        }
        return result;
    }

    // OSC 10-19 set/query dynamic color
    // Multiple colors can be specified which then apply to the next OSC number
    for (auto cs : parts | di::drop(1)) {
        auto _ = di::ScopeExit([&] {
            number++;
        });
        if (number > dynamic_end) {
            break;
        }
        auto request = Request {};
        if (cs == "?"_sv) {
            request.query = true;
        } else {
            auto c = parse_color(cs);
            if (!c) {
                continue;
            }
            request.color = c.value();
        }
        switch (number) {
            case dynamic_cursor:
                request.palette = PaletteIndex::Cursor;
                break;
            case dynamic_selection_foreground:
                request.palette = PaletteIndex::SelectionForeground;
                break;
            case dynamic_selection_background:
                request.palette = PaletteIndex::SelectionBackground;
                break;
            case dynamic_background:
                request.palette = PaletteIndex::Background;
                break;
            case dynamic_foreground:
                request.palette = PaletteIndex::Foreground;
                break;
            default:
                continue;
        }
        result.requests.push_back(di::move(request));
    }
    if (result.requests.empty()) {
        return {};
    }
    return result;
}

static auto serialize_color(Color color, bool kitty = false) -> di::String {
    if (color.is_dynamic()) {
        return "dynamic"_s;
    }
    if (!color.is_custom()) {
        return ""_s;
    }
    if (kitty) {
        return di::format("rgb:{:02x}/{:02x}/{:02x}"_sv, color.r, color.g, color.b);
    }

    // For some reason, the legacy format uses 16 bit colors instead of 8 bit colors.
    return di::format("rgb:{:04x}/{:04x}/{:04x}"_sv, (color.r << 8) + color.r, (color.g << 8) + color.g,
                      (color.b << 8) + color.b);
}

static auto kitty_name(PaletteIndex index) -> di::String {
    if (index <= PaletteIndex::SpecialEnd) {
        return di::to_string(u16(index));
    }
    for (auto const& [name, palette] : kitty_color_names) {
        if (palette == index) {
            return name.to_owned();
        }
    }
    return "?"_s;
}

static auto serialize_kitty(OSC21::Request const& request) -> di::String {
    auto palette_name = kitty_name(request.palette);
    auto const& name = request.kitty_color_name.has_value() ? request.kitty_color_name.value() : palette_name;
    return di::format("{}={}"_sv, name, request.query ? "?"_s : serialize_color(request.color, true));
}

static auto serialize_legacy(OSC21::Request const& request) -> di::String {
    if (request.palette <= PaletteIndex::StaticEnd) {
        if (request.is_reset()) {
            return di::format("{};{}"_sv, OSC21::reset_offset + OSC21::palette, u16(request.palette));
        }
        return di::format("{};{};{}"_sv, OSC21::palette, u16(request.palette),
                          request.query ? "?"_s : serialize_color(request.color));
    }
    if (request.palette <= PaletteIndex::SpecialEnd) {
        if (request.is_reset()) {
            return di::format("{};{}"_sv, OSC21::reset_offset + OSC21::special,
                              u16(request.palette) - u16(PaletteIndex::SpecialBegin));
        }
        return di::format("{};{};{}"_sv, OSC21::special, u16(request.palette) - u16(PaletteIndex::SpecialBegin),
                          request.query ? "?"_s : serialize_color(request.color));
    }
    auto number = 0_u32;
    switch (request.palette) {
        case PaletteIndex::Foreground:
            number = OSC21::dynamic_foreground;
            break;
        case PaletteIndex::Background:
            number = OSC21::dynamic_background;
            break;
        case PaletteIndex::Cursor:
            number = OSC21::dynamic_cursor;
            break;
        case PaletteIndex::SelectionForeground:
            number = OSC21::dynamic_selection_foreground;
            break;
        case PaletteIndex::SelectionBackground:
            number = OSC21::dynamic_selection_background;
            break;
        default:
            return {};
    }

    if (request.is_reset()) {
        return di::format("{}"_sv, number + OSC21::reset_offset);
    }
    return di::format("{};{}"_sv, number, request.query ? "?"_s : serialize_color(request.color));
}

auto OSC21::serialize(Feature features) const -> di::String {
    if (!!(features & Feature::DynamicPaletteKitty)) {
        if (requests.empty()) {
            return {};
        }
        return di::format("\033]21;{}\33\\"_sv,
                          requests | di::transform(serialize_kitty) | di::join_with(U';') | di::to<di::String>());
    }

    auto result = ""_s;
    for (auto const& request : requests) {
        auto value = serialize_legacy(request);
        if (value.empty()) {
            continue;
        }
        result.append(di::format("\033]{}\033\\"_sv, value));
    }
    return result;
}
}
