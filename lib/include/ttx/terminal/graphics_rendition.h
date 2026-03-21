#pragma once

#include "di/container/string/prelude.h"
#include "di/reflect/prelude.h"
#include "di/types/integers.h"
#include "di/vocab/optional/prelude.h"
#include "di/vocab/span/prelude.h"
#include "ttx/features.h"
#include "ttx/params.h"
#include "ttx/terminal/color.h"

namespace ttx::terminal {
enum class BlinkMode : u8 {
    None,
    Normal,
    Rapid,
};

constexpr auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<BlinkMode>) {
    using enum BlinkMode;
    return di::make_enumerators<"BlinkMode">(di::enumerator<"None", None>, di::enumerator<"Normal", Normal>,
                                             di::enumerator<"Rapid", Rapid>);
}

enum class FontWeight : u8 {
    None,
    Bold,
    Dim,
};

constexpr auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<FontWeight>) {
    using enum FontWeight;
    return di::make_enumerators<"FontWeight">(di::enumerator<"None", None>, di::enumerator<"Bold", Bold>,
                                              di::enumerator<"Dim", Dim>);
}

enum class UnderlineMode : u8 {
    None = 0,
    Normal = 1,
    Double = 2,
    Curly = 3,
    Dotted = 4,
    Dashed = 5,
};

constexpr auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<UnderlineMode>) {
    using enum UnderlineMode;
    return di::make_enumerators<"UnderlineMode">(di::enumerator<"None", None>, di::enumerator<"Normal", Normal>,
                                                 di::enumerator<"Double", Double>, di::enumerator<"Curly", Curly>,
                                                 di::enumerator<"Dotted", Dotted>, di::enumerator<"Dashed", Dashed>);
}

struct GraphicsRendition {
    Color fg {};
    Color bg {};
    Color underline_color {};

    FontWeight font_weight { FontWeight::None };
    BlinkMode blink_mode { BlinkMode::None };
    UnderlineMode underline_mode { UnderlineMode::None };
    bool italic { false };
    bool overline { false };
    bool inverted { false };
    bool invisible { false };
    bool strike_through { false };

    static auto from_csi_params(Params const& params) {
        auto result = GraphicsRendition {};
        result.update_with_csi_params(params);
        return result;
    }
    void update_with_csi_params(Params const& params);

    auto as_csi_params(Feature features = Feature::None, di::Optional<GraphicsRendition const&> prev = {}) const
        -> di::Vector<Params>;

    auto operator==(GraphicsRendition const& other) const -> bool = default;
    auto operator<=>(GraphicsRendition const& other) const = default;

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<GraphicsRendition>) {
        return di::make_fields<"GraphicsRendition">(
            di::field<"fg", &GraphicsRendition::fg>, di::field<"bg", &GraphicsRendition::bg>,
            di::field<"underline_color", &GraphicsRendition::underline_color>,
            di::field<"font_weight", &GraphicsRendition::font_weight>,
            di::field<"blink_mode", &GraphicsRendition::blink_mode>,
            di::field<"underline_mode", &GraphicsRendition::underline_mode>,
            di::field<"italic", &GraphicsRendition::italic>, di::field<"overline", &GraphicsRendition::overline>,
            di::field<"inverted", &GraphicsRendition::inverted>, di::field<"invisible", &GraphicsRendition::invisible>,
            di::field<"strike_through", &GraphicsRendition::strike_through>);
    }
};
}
