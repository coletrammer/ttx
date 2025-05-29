#pragma once

#include "di/reflect/prelude.h"
#include "ttx/escape_sequence_parser.h"
#include "ttx/size.h"

namespace ttx::terminal {
/// @brief Text area pixel size report
///
/// This is requested via CSI 14 t, as specified
/// [here](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h4-Functions-using-CSI-_-ordered-by-the-final-character-lparen-s-rparen:CSI-Ps;Ps;Ps-t.1EB0).
struct TextAreaPixelSizeReport {
    u32 xpixels { 0 };
    u32 ypixels { 0 };

    static auto from_csi(CSI const& csi) -> di::Optional<TextAreaPixelSizeReport>;
    auto serialize() const -> di::String;

    auto operator==(TextAreaPixelSizeReport const& other) const -> bool = default;

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<TextAreaPixelSizeReport>) {
        return di::make_fields<"TextAreaPixelSizeReport">(di::field<"xpixels", &TextAreaPixelSizeReport::xpixels>,
                                                          di::field<"ypixels", &TextAreaPixelSizeReport::ypixels>);
    }
};

/// @brief Cell pixel size report
///
/// This is requested via CSI 16 t, as specified
/// [here](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h4-Functions-using-CSI-_-ordered-by-the-final-character-lparen-s-rparen:CSI-Ps;Ps;Ps-t.1EB0).
struct CellPixelSizeReport {
    u32 xpixels { 0 };
    u32 ypixels { 0 };

    static auto from_csi(CSI const& csi) -> di::Optional<CellPixelSizeReport>;
    auto serialize() const -> di::String;

    auto operator==(CellPixelSizeReport const& other) const -> bool = default;

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<CellPixelSizeReport>) {
        return di::make_fields<"CellPixelSizeReport">(di::field<"xpixels", &CellPixelSizeReport::xpixels>,
                                                      di::field<"ypixels", &CellPixelSizeReport::ypixels>);
    }
};

/// @brief Text area size report
///
/// This is requested via CSI 18 t, as specified
/// [here](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h4-Functions-using-CSI-_-ordered-by-the-final-character-lparen-s-rparen:CSI-Ps;Ps;Ps-t.1EB0).
struct TextAreaSizeReport {
    u32 cols { 0 };
    u32 rows { 0 };

    static auto from_csi(CSI const& csi) -> di::Optional<TextAreaSizeReport>;
    auto serialize() const -> di::String;

    auto operator==(TextAreaSizeReport const& other) const -> bool = default;

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<TextAreaSizeReport>) {
        return di::make_fields<"TextAreaSizeReport">(di::field<"cols", &TextAreaSizeReport::cols>,
                                                     di::field<"rows", &TextAreaSizeReport::rows>);
    }
};

/// @brief In-band size report
///
/// This is requested by DEC private mode 2024, as specified
/// [here](https://gist.github.com/rockorager/e695fb2924d36b2bcf1fff4a3704bd83).
struct InBandSizeReport {
    Size size;

    static auto from_csi(CSI const& csi) -> di::Optional<InBandSizeReport>;
    auto serialize() const -> di::String;

    auto operator==(InBandSizeReport const& other) const -> bool = default;

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<InBandSizeReport>) {
        return di::make_fields<"InBandSizeReport">(di::field<"size", &InBandSizeReport::size>);
    }
};
}
