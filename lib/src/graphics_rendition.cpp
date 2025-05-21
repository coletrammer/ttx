#include "ttx/graphics_rendition.h"

#include "di/container/vector/vector.h"
#include "di/parser/integral.h"
#include "di/vocab/tuple/prelude.h"
#include "ttx/features.h"
#include "ttx/params.h"

namespace ttx {
// Parse complex color. This routine handles the following colors in the following form:
//   38;2;R;G;B   -- legacy form used for backwards compatability.
//   38:2:R:G:B   -- normal form with subparameters but without color space.
//   38:2:X:R:G:B -- subparameters with color space argument, which is ignored.
//   38;5;I       -- lagecy index form used for backwards compatability.
//   38:5:I       -- index form, specifices an index into color palette. Palette indices must be less than 256.
// This returns both the number of parameters consumed as well as the parsed color.
static auto parse_complex_color(Params const& params, usize start_index) -> di::Tuple<usize, Color> {
    auto subparams = params.subparams(start_index);
    if (subparams.size() == 1) {
        // If there are no subparameters, use the legacy form.
        switch (params.get(start_index + 1)) {
            case 2:
                if (params.size() - start_index < 5) {
                    return { 1, {} };
                }
                return { 5,
                         Color(params.get(start_index + 2), params.get(start_index + 3), params.get(start_index + 4)) };
            case 5:
                if (params.size() - start_index < 3) {
                    return { 1, {} };
                }
                if (params.get(start_index + 2) >= 256) {
                    return { 3, Color() };
                }
                return { 3, Color(Color::Palette(params.get(start_index + 2))) };
            default:
                return { 1, {} };
        }
        return { 5, Color(params.get(start_index + 2), params.get(start_index + 3), params.get(start_index + 4)) };
    }

    // Regular form with subparams
    switch (subparams.get(1)) {
        case 2:
            if (subparams.size() != 5 && subparams.size() != 6) {
                break;
            }
            return { 1, Color(subparams.get(subparams.size() - 3), subparams.get(subparams.size() - 2),
                              subparams.get(subparams.size() - 1)) };
        case 5:
            if (subparams.get(2) >= 256) {
                return { 1, Color() };
            }
            return { 1, Color(Color::Palette(subparams.get(2))) };
        default:
            break;
    }
    return { 1, {} };
}

// Parse any color specifer. This handles all cases for fg, bg, and underline_color.
static auto parse_color(Params const& params, usize start_index) -> di::Tuple<usize, Color> {
    auto command = params.get(start_index);

    // Complex colors
    if (command == 38 || command == 48 || command == 58) {
        return parse_complex_color(params, start_index);
    }

    // High colors
    auto palette_index = command % 10;
    if (command >= 90) {
        return { 1, Color(Color::Palette(Color::Palette::DarkGrey + palette_index)) };
    }

    // Reset
    if (palette_index == 9) {
        return { 1, {} };
    }

    // Palette color
    return { 1, Color(Color::Palette(Color::Palette::Black + palette_index)) };
}

// Select Graphics Rendition - https://vt100.net/docs/vt510-rm/SGR.html
//   Modern extensions like underline and true color can be found here:
//     https://wezfurlong.org/wezterm/escape-sequences.html#graphic-rendition-sgr
void GraphicsRendition::update_with_csi_params(Params const& params) {
    // No params = reset.
    if (params.empty()) {
        *this = {};
        return;
    }

    for (auto i = 0_usize; i == 0 || i < params.size(); i++) {
        switch (params.get(i, 0)) {
            case 0:
                *this = {};
                break;
            case 1:
                font_weight = FontWeight::Bold;
                break;
            case 2:
                font_weight = FontWeight::Dim;
                break;
            case 3:
                italic = true;
                break;
            case 4: {
                auto subparam = params.get_subparam(i, 1, 1);
                switch (subparam) {
                    case 0:
                        underline_mode = UnderlineMode::None;
                        break;
                    case 1:
                        underline_mode = UnderlineMode::Normal;
                        break;
                    case 2:
                        underline_mode = UnderlineMode::Double;
                        break;
                    case 3:
                        underline_mode = UnderlineMode::Curly;
                        break;
                    case 4:
                        underline_mode = UnderlineMode::Dotted;
                        break;
                    case 5:
                        underline_mode = UnderlineMode::Dashed;
                        break;
                    default:
                        break;
                }
                break;
            }
            case 5:
                blink_mode = BlinkMode::Normal;
                break;
            case 6:
                blink_mode = BlinkMode::Rapid;
                break;
            case 7:
                inverted = true;
                break;
            case 8:
                invisible = true;
                break;
            case 9:
                strike_through = true;
                break;
            case 21:
                underline_mode = UnderlineMode::Double;
                break;
            case 22:
                font_weight = FontWeight::None;
                break;
            case 23:
                italic = false;
                break;
            case 24:
                underline_mode = UnderlineMode::None;
                break;
            case 25:
                blink_mode = BlinkMode::None;
                break;
            case 27:
                inverted = false;
                break;
            case 28:
                invisible = false;
                break;
            case 29:
                strike_through = false;
                break;
            case 30:
            case 31:
            case 32:
            case 33:
            case 34:
            case 35:
            case 36:
            case 37:
            case 38:
            case 39:
            case 90:
            case 91:
            case 92:
            case 93:
            case 94:
            case 95:
            case 96:
            case 97: {
                auto [n, c] = parse_color(params, i);
                i += n - 1;
                fg = c;
                break;
            }
            case 40:
            case 41:
            case 42:
            case 43:
            case 44:
            case 45:
            case 46:
            case 47:
            case 48:
            case 49:
            case 100:
            case 101:
            case 102:
            case 103:
            case 104:
            case 105:
            case 106:
            case 107: {
                auto [n, c] = parse_color(params, i);
                i += n - 1;
                bg = c;
                break;
            }
            case 53:
                overline = true;
                break;
            case 55:
                overline = false;
                break;
            case 58:
            case 59: {
                auto [n, c] = parse_color(params, i);
                i += n - 1;
                underline_color = c;
                break;
            }
            default:
                break;
        }
    }
}

enum class ColorType {
    Fg,
    Bg,
    Underine,
};

static auto color_to_params(Color c, ColorType type, bool use_legacy) -> Params {
    if (c.type == Color::Type::Custom) {
        auto code = type == ColorType::Fg ? 38u : type == ColorType::Bg ? 48u : 58u;
        if (type == ColorType::Underine || !use_legacy) {
            // Underline color isn't constrained by backwards compatability, so use the new form:
            //   code:2::r:g:b
            return { { code, 2, {}, c.r, c.g, c.b } };
        }
        // For compatability, use the old escape sequence form for fg and bg.
        //   code; 2; r; g; b
        return { { code }, { 2 }, { c.r }, { c.g }, { c.b } };
    }
    if (c.type == Color::Type::Default) {
        auto code = type == ColorType::Fg ? 39u : type == ColorType::Bg ? 49u : 59u;
        return { { code } };
    }
    if (type == ColorType::Underine) {
        // Use palette index.
        return { { 58, 5, u32(c.r - Color::Palette::Black) } };
    }
    if (c.r <= Color::Palette::LightGrey) {
        auto base_value = type == ColorType::Fg ? 30u : 40u;
        return { { base_value + c.r - Color::Palette::Black } };
    }
    if (c.r <= Color::Palette::White) {
        auto base_value = type == ColorType::Fg ? 90u : 100u;
        return { { base_value + c.r - Color::Palette::DarkGrey } };
    }
    // Indexed color
    auto base_value = type == ColorType::Fg ? 38u : 48u;
    if (use_legacy) {
        return { { base_value }, { 5 }, { c.r } };
    }
    return { { base_value, 5, c.r } };
}

auto GraphicsRendition::as_csi_params(Feature features, di::Optional<GraphicsRendition const&> prev) const
    -> di::Vector<Params> {
    // Start by clearing all attributes.
    auto result = di::Vector<Params>();
    auto& basic = result.emplace_back();
    if (!prev) {
        basic.add_param(0);
    }

    auto compare = prev.transform([](auto&& x) {
                           return x;
                       })
                       .value_or(GraphicsRendition {});
    if (compare.font_weight != font_weight) {
        switch (font_weight) {
            case FontWeight::Bold:
                basic.add_param(1);
                break;
            case FontWeight::Dim:
                basic.add_param(2);
                break;
            case FontWeight::None:
                basic.add_param(22);
                break;
        }
    }
    if (compare.italic != italic) {
        if (italic) {
            basic.add_param(3);
        } else {
            basic.add_param(23);
        }
    }
    if (compare.blink_mode != blink_mode) {
        switch (blink_mode) {
            case BlinkMode::Normal:
                basic.add_param(5);
                break;
            case BlinkMode::Rapid:
                basic.add_param(6);
                break;
            case BlinkMode::None:
                basic.add_param(25);
                break;
        }
    }
    if (compare.inverted != inverted) {
        if (inverted) {
            basic.add_param(7);
        } else {
            basic.add_param(27);
        }
    }
    if (compare.invisible != invisible) {
        if (invisible) {
            basic.add_param(8);
        } else {
            basic.add_param(28);
        }
    }
    if (compare.strike_through != strike_through) {
        if (strike_through) {
            basic.add_param(9);
        } else {
            basic.add_param(29);
        }
    }
    if (compare.overline != overline) {
        if (overline) {
            basic.add_param(53);
        } else {
            basic.add_param(55);
        }
    }

    if (compare.underline_mode != underline_mode) {
        switch (underline_mode) {
            case UnderlineMode::Normal:
                basic.add_param(4);
                break;
            case UnderlineMode::Double:
                basic.add_param(21);
                break;
            case UnderlineMode::Curly: {
                // For compatability, include the subparameters for specifying the underline type
                // in its own SGR escape. This way we won't mix an escape sequence with both
                // parameters and subparameters.
                if (!!(features & Feature::Undercurl)) {
                    auto params = Params {};
                    params.add_subparams({ 4, 3 });
                    result.push_back(di::move(params));
                } else {
                    basic.add_param(4);
                }
                break;
            }
            case UnderlineMode::Dotted: {
                if (!!(features & Feature::Undercurl)) {
                    auto params = Params {};
                    params.add_subparams({ 4, 4 });
                    result.push_back(di::move(params));
                } else {
                    basic.add_param(4);
                }
                break;
            }
            case UnderlineMode::Dashed: {
                if (!!(features & Feature::Undercurl)) {
                    auto params = Params {};
                    params.add_subparams({ 4, 5 });
                    result.push_back(di::move(params));
                } else {
                    basic.add_param(4);
                }
                break;
            }
            case UnderlineMode::None:
                basic.add_param(24);
                break;
        }
    }

    // To ensure we don't exceed to maximum number of parameters allowed (16), split each color
    // spec into its own set of parameters.
    //
    // Also, use legacy true color sequences (with ';' instead of ':') unless the terminal is new
    // enough to support undercurl.
    auto use_legacy = !(features & Feature::Undercurl);
    if (compare.fg != fg) {
        result.push_back(color_to_params(fg, ColorType::Fg, use_legacy));
    }
    if (compare.bg != bg) {
        result.push_back(color_to_params(bg, ColorType::Bg, use_legacy));
    }
    if (compare.underline_color != underline_color) {
        result.push_back(color_to_params(underline_color, ColorType::Underine, use_legacy));
    }
    di::erase_if(result, &Params::empty);
    return result;
}
}
