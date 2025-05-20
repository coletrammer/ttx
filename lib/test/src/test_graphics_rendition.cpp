#include "di/container/view/cartesian_product.h"
#include "di/container/view/join_with.h"
#include "di/test/prelude.h"
#include "ttx/graphics_rendition.h"
#include "ttx/params.h"

namespace graphics_rendition {
static void parse() {
    auto rendition =
        ttx::GraphicsRendition::from_csi_params({ { 3 }, { 2 }, { 4 }, { 5 }, { 7 }, { 8 }, { 9 }, { 53 } });
    ASSERT_EQ(auto(rendition.font_weight), ttx::FontWeight::Dim);
    ASSERT(auto(rendition.italic));
    ASSERT_EQ(auto(rendition.underline_mode), ttx::UnderlineMode::Normal);
    ASSERT_EQ(auto(rendition.blink_mode), ttx::BlinkMode::Normal);
    ASSERT(auto(rendition.inverted));
    ASSERT(auto(rendition.invisible));
    ASSERT(auto(rendition.strike_through));
    ASSERT(auto(rendition.overline));

    rendition.update_with_csi_params({ { 22 }, { 23 }, { 24 }, { 25 }, { 27 }, { 28 }, { 29 }, { 55 } });
    ASSERT_EQ(auto(rendition.font_weight), ttx::FontWeight::None);
    ASSERT(!auto(rendition.italic));
    ASSERT_EQ(auto(rendition.underline_mode), ttx::UnderlineMode::None);
    ASSERT_EQ(auto(rendition.blink_mode), ttx::BlinkMode::None);
    ASSERT(!auto(rendition.inverted));
    ASSERT(!auto(rendition.invisible));
    ASSERT(!auto(rendition.strike_through));
    ASSERT(!auto(rendition.overline));

    rendition.update_with_csi_params({ { 21 }, { 6 } });
    ASSERT_EQ(auto(rendition.underline_mode), ttx::UnderlineMode::Double);
    ASSERT_EQ(auto(rendition.blink_mode), ttx::BlinkMode::Rapid);

    rendition.update_with_csi_params({ { 4, 3 }, { 6 } });
    ASSERT_EQ(auto(rendition.underline_mode), ttx::UnderlineMode::Curly);

    rendition.update_with_csi_params({ { 33 }, { 44 } });
    ASSERT_EQ(auto(rendition.fg.r), ttx::Color::Palette::Brown);
    ASSERT_EQ(auto(rendition.bg.r), ttx::Color::Palette::Blue);

    rendition.update_with_csi_params({ { 93 }, { 104 } });
    ASSERT_EQ(auto(rendition.fg.r), ttx::Color::Palette::Yellow);
    ASSERT_EQ(auto(rendition.bg.r), ttx::Color::Palette::LightBlue);

    // legacy 256 color
    rendition.update_with_csi_params({ { 38 }, { 2 }, { 1 }, { 2 }, { 3 } });
    ASSERT_EQ(auto(rendition.fg), ttx::Color(1, 2, 3));

    // 256 color without color space
    rendition.update_with_csi_params({ { 48, 2, 1, 2, 3 } });
    ASSERT_EQ(auto(rendition.bg), ttx::Color(1, 2, 3));

    // 256 color with color space
    rendition.update_with_csi_params({ { 48, 2, 0, 2, 3, 4 } });
    ASSERT_EQ(auto(rendition.bg), ttx::Color(2, 3, 4));

    // index color
    rendition.update_with_csi_params({ { 58, 5, 5 } });
    ASSERT_EQ(auto(rendition.underline_color), ttx::Color(ttx::Color::Palette::Magenta));

    // legacy indexed color
    rendition.update_with_csi_params({ { 38 }, { 5 }, { 9 } });
    ASSERT_EQ(auto(rendition.fg), ttx::Color::Palette::LightRed);

    // clear
    rendition.update_with_csi_params({ { 39 }, { 49 }, { 59 } });
    ASSERT_EQ(auto(rendition.fg.type), ttx::Color::Type::Default);
    ASSERT_EQ(auto(rendition.bg.type), ttx::Color::Type::Default);
    ASSERT_EQ(auto(rendition.underline_color.type), ttx::Color::Type::Default);
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
    auto rendition = ttx::GraphicsRendition {};
    rendition.blink_mode = ttx::BlinkMode::Rapid;
    rendition.italic = true;
    rendition.font_weight = ttx::FontWeight::Bold;
    rendition.fg = ttx::Color(2, 45, 67);
    rendition.bg = ttx::Color(3, 88, 99);
    rendition.underline_color = ttx::Color(22, 35, 87);

    auto actual = combine_csi_params(rendition.as_csi_params());
    auto expected = ttx::Params {
        { 0 }, { 1 }, { 3 },  { 6 },  { 38 },
        { 2 }, { 2 }, { 45 }, { 67 }, { 48 },
        { 2 }, { 3 }, { 88 }, { 99 }, { 58, 2, {}, 22, 35, 87 },
    };
    ASSERT_EQ(actual, expected);
}

static void roundtrip() {
    auto colors = di::Array {
        ttx::Color(),
        ttx::Color(ttx::Color::Palette::Blue),
        ttx::Color(ttx::Color::Palette::LightCyan),
        ttx::Color(ttx::Color::Palette(255)),
        ttx::Color(ttx::Color(123, 255, 99)),
    };
    auto font_weights = di::Array { ttx::FontWeight::None, ttx::FontWeight::Bold, ttx::FontWeight::Dim };
    auto blink_modes = di::Array { ttx::BlinkMode::None, ttx::BlinkMode::Normal, ttx::BlinkMode::Rapid };
    auto underline_modes =
        di::Array { ttx::UnderlineMode::Normal, ttx::UnderlineMode::Normal, ttx::UnderlineMode::Curly,
                    ttx::UnderlineMode::Dashed, ttx::UnderlineMode::Dotted, ttx::UnderlineMode::Double };
    auto italics = di::Array { false, true };
    auto overlines = di::Array { false, true };
    auto inverteds = di::Array { false, true };
    auto invisibles = di::Array { false, true };
    auto strike_throughs = di::Array { false, true };

    for (auto [fg, bg, underline_color, font_weight, blink_mode, underline_mode, italic, overline, inverted, invisible,
               strike_through] :
         di::cartesian_product(colors, colors, colors, font_weights, blink_modes, underline_modes, italics, overlines,
                               inverteds, invisibles, strike_throughs)) {
        auto expected = ttx::GraphicsRendition {};
        expected.fg = fg;
        expected.bg = bg;
        expected.underline_color = underline_color;
        expected.font_weight = font_weight;
        expected.font_weight = font_weight;
        expected.italic = italic;
        expected.overline = overline;
        expected.inverted = inverted;
        expected.invisible = invisible;
        expected.strike_through = strike_through;

        auto actual = ttx::GraphicsRendition::from_csi_params(combine_csi_params(expected.as_csi_params()));
        ASSERT_EQ(expected, actual);
    }
}

TEST(graphics_rendition, parse)
TEST(graphics_rendition, as_csi_params)
TEST(graphics_rendition, roundtrip)
}
