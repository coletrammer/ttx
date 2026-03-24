#include "di/container/view/cartesian_product.h"
#include "di/container/view/join_with.h"
#include "di/test/prelude.h"
#include "ttx/features.h"
#include "ttx/params.h"
#include "ttx/terminal/graphics_rendition.h"

namespace graphics_rendition {
using namespace ttx::terminal;

static void parse() {
    auto rendition = GraphicsRendition::from_csi_params({ { 3 }, { 2 }, { 4 }, { 5 }, { 7 }, { 8 }, { 9 }, { 53 } });
    ASSERT_EQ(auto(rendition.font_weight), FontWeight::Dim);
    ASSERT(auto(rendition.italic));
    ASSERT_EQ(auto(rendition.underline_mode), UnderlineMode::Normal);
    ASSERT_EQ(auto(rendition.blink_mode), BlinkMode::Normal);
    ASSERT(auto(rendition.inverted));
    ASSERT(auto(rendition.invisible));
    ASSERT(auto(rendition.strike_through));
    ASSERT(auto(rendition.overline));

    rendition.update_with_csi_params({ { 22 }, { 23 }, { 24 }, { 25 }, { 27 }, { 28 }, { 29 }, { 55 } });
    ASSERT_EQ(auto(rendition.font_weight), FontWeight::None);
    ASSERT(!auto(rendition.italic));
    ASSERT_EQ(auto(rendition.underline_mode), UnderlineMode::None);
    ASSERT_EQ(auto(rendition.blink_mode), BlinkMode::None);
    ASSERT(!auto(rendition.inverted));
    ASSERT(!auto(rendition.invisible));
    ASSERT(!auto(rendition.strike_through));
    ASSERT(!auto(rendition.overline));

    rendition.update_with_csi_params({ { 21 }, { 6 } });
    ASSERT_EQ(auto(rendition.underline_mode), UnderlineMode::Double);
    ASSERT_EQ(auto(rendition.blink_mode), BlinkMode::Rapid);

    rendition.update_with_csi_params({ { 4, 3 }, { 6 } });
    ASSERT_EQ(auto(rendition.underline_mode), UnderlineMode::Curly);

    rendition.update_with_csi_params({ { 33 }, { 44 } });
    ASSERT_EQ(auto(rendition.fg.r), Color::Palette::Yellow);
    ASSERT_EQ(auto(rendition.bg.r), Color::Palette::Blue);

    rendition.update_with_csi_params({ { 93 }, { 104 } });
    ASSERT_EQ(auto(rendition.fg.r), Color::Palette::LightYellow);
    ASSERT_EQ(auto(rendition.bg.r), Color::Palette::LightBlue);

    // legacy 256 color
    rendition.update_with_csi_params({ { 38 }, { 2 }, { 1 }, { 2 }, { 3 } });
    ASSERT_EQ(auto(rendition.fg), Color(1, 2, 3));

    // 256 color without color space
    rendition.update_with_csi_params({ { 48, 2, 1, 2, 3 } });
    ASSERT_EQ(auto(rendition.bg), Color(1, 2, 3));

    // 256 color with color space
    rendition.update_with_csi_params({ { 48, 2, 0, 2, 3, 4 } });
    ASSERT_EQ(auto(rendition.bg), Color(2, 3, 4));

    // index color
    rendition.update_with_csi_params({ { 58, 5, 5 } });
    ASSERT_EQ(auto(rendition.underline_color), Color(Color::Palette::Magenta));

    // legacy indexed color
    rendition.update_with_csi_params({ { 38 }, { 5 }, { 9 } });
    ASSERT_EQ(auto(rendition.fg), Color::Palette::LightRed);

    // clear
    rendition.update_with_csi_params({ { 39 }, { 49 }, { 59 } });
    ASSERT_EQ(auto(rendition.fg.type), Color::Type::Default);
    ASSERT_EQ(auto(rendition.bg.type), Color::Type::Default);
    ASSERT_EQ(auto(rendition.underline_color.type), Color::Type::Default);
}

static auto combine_csi_params(di::Vector<ttx::Params> params_list) -> ttx::Params {
    // NOTE: The reference for parsing escape sequences limits the number of parameters at 16.
    // Therefore we split SGR parameters into multiple escape sequences if necessary. This
    // validates that each item in the list doesn't container too many parameters.

    auto strings = di::Vector<di::String> {};
    for (auto const& params : params_list) {
        auto count = 0_usize;
        for (auto i : di::range(params.size())) {
            auto subparams = params.subparams(i);
            count += subparams.size();
        }
        ASSERT_LT_EQ(count, 16_usize);

        strings.push_back(params.to_string());
    }

    auto string = strings | di::join_with(U';') | di::to<di::String>();
    return ttx::Params::from_string(string);
}

static void as_csi_params() {
    auto rendition = GraphicsRendition {};
    rendition.blink_mode = BlinkMode::Rapid;
    rendition.italic = true;
    rendition.font_weight = FontWeight::Bold;
    rendition.underline_mode = UnderlineMode::Curly;
    rendition.fg = Color(2, 45, 67);
    rendition.bg = Color(3, 88, 99);
    rendition.underline_color = Color(22, 35, 87);

    // Without undercurl, we use normal underline and legacy graphics codes
    auto actual = combine_csi_params(rendition.as_csi_params(ttx::Feature::None));
    auto expected = ttx::Params {
        { 0 },  { 1 },  { 3 },  { 6 }, { 4 }, { 38 }, { 2 },  { 2 },
        { 45 }, { 67 }, { 48 }, { 2 }, { 3 }, { 88 }, { 99 }, { 58, 2, {}, 22, 35, 87 },
    };
    ASSERT_EQ(actual, expected);

    // With undercurl, we use the modern graphics forms
    actual = combine_csi_params(rendition.as_csi_params(ttx::Feature::Undercurl));
    expected = ttx::Params {
        { 0 },
        { 1 },
        { 3 },
        { 6 },
        { 4, 3 },
        { 38, 2, {}, 2, 45, 67 },
        { 48, 2, {}, 3, 88, 99 },
        { 58, 2, {}, 22, 35, 87 },
    };
    ASSERT_EQ(actual, expected);
}

static void roundtrip() {
    auto colors = di::Array {
        Color(),
        Color(Color::Palette::Blue),
        Color(Color::Palette::LightCyan),
        Color(Color::Palette(255)),
        Color(Color(123, 255, 99)),
    };
    auto font_weights = di::Array { FontWeight::None, FontWeight::Bold, FontWeight::Dim };
    auto blink_modes = di::Array { BlinkMode::None, BlinkMode::Normal, BlinkMode::Rapid };
    auto underline_modes = di::Array { UnderlineMode::None,   UnderlineMode::Normal, UnderlineMode::Curly,
                                       UnderlineMode::Dashed, UnderlineMode::Dotted, UnderlineMode::Double };
    auto italics = di::Array { false, true };
    auto overlines = di::Array { false, true };
    auto inverteds = di::Array { false, true };
    auto invisibles = di::Array { false, true };
    auto strike_throughs = di::Array { false, true };
    auto features = di::Array { ttx::Feature::None, ttx::Feature::Undercurl };
    auto use_prevs = di::Array { false, true };

    auto prev = di::Optional<GraphicsRendition> {};
    for (auto [use_prev, fg, bg, underline_color, font_weight, blink_mode, underline_mode, italic, overline, inverted,
               invisible, strike_through, feature] :
         di::cartesian_product(use_prevs, colors, colors, colors, font_weights, blink_modes, underline_modes, italics,
                               overlines, inverteds, invisibles, strike_throughs, features)) {
        auto expected = GraphicsRendition {};
        expected.fg = fg;
        expected.bg = bg;
        expected.underline_color = underline_color;
        expected.font_weight = font_weight;
        expected.underline_mode = underline_mode;
        expected.blink_mode = blink_mode;
        expected.italic = italic;
        expected.overline = overline;
        expected.inverted = inverted;
        expected.invisible = invisible;
        expected.strike_through = strike_through;

        auto actual = use_prev ? prev.value_or(GraphicsRendition {}) : GraphicsRendition {};
        if (!(feature & ttx::Feature::Undercurl)) {
            if (expected.underline_mode != UnderlineMode::Normal && expected.underline_mode != UnderlineMode::None &&
                expected.underline_mode != UnderlineMode::Double && expected.underline_mode != actual.underline_mode) {
                expected.underline_mode = UnderlineMode::Normal;
            }
        }

        auto params =
            combine_csi_params(expected.as_csi_params(feature, use_prev ? prev.transform(di::cref) : di::nullopt));
        if (!use_prev || !params.empty()) {
            actual.update_with_csi_params(params);
        }
        ASSERT_EQ(expected, actual);

        prev = actual;
    }
}

TEST(graphics_rendition, parse)
TEST(graphics_rendition, as_csi_params)
TEST(graphics_rendition, roundtrip)
}
