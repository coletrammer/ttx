#pragma once

#include "di/reflect/prelude.h"
#include "di/util/bitwise_enum.h"
#include "dius/sync_file.h"
#include "ttx/terminal/palette.h"

namespace ttx {
enum class Feature : u64 {
    None = 0,
    SyncronizedOutput = 1 << 0,         ///< Allow using DEC mode 2026 to synchronize screen updates.
    TextSizingWidth = 1 << 1,           ///< Supports using the text sizing protocol to specify explicit text width.
    TextSizingFull = 1 << 2,            ///< Supports using the text sizing protocol to multi-height cells.
    ThemeDetection = 1 << 3,            ///< Supports light/dark mode detection via mode 2031.
    InBandSizeReports = 1 << 4,         ///< Supports in-band size reports via mode 2048.
    GraphemeClusteringMode = 1 << 5,    ///< Supports grapheme clustering mode via mode 2027.
    KittyKeyProtocol = 1 << 6,          ///< Supports kitty key protocol.
    Undercurl = 1 << 7,                 ///< Supports undercurl (fancy underline) and underline colors.
    BasicGraphemeClustering = 1 << 8,   ///< Supports grapheme clustering, but may not match the kitty spec.
    FullGraphemeClustering = 1 << 9,    ///< Grapheme clustering behavior matches kitty spec.
    TextSizingPresentation = 1 << 10,   ///< Supports text-sizing with scale=1 but fractional scale and alignment.
    Clipboard = 1 << 11,                ///< Supports setting the clipboard (OSC 52).
    DynamicPalette = 1 << 12,           ///< Supports changing the color palette dynamically.
    BackgroundCharacterErase = 1 << 13, ///< Clearing the screen sets the current SGR background color.
    SeamlessNavigation = 1 << 14,       ///< Supports seamless navigation protocol (OSC 8671).
    DynamicPaletteKitty = 1 << 15,      ///< Supports kitty color protocol (OSC 21).
    All = u64(-1),
};

DI_DEFINE_ENUM_BITWISE_OPERATIONS(Feature)

constexpr auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<Feature>) {
    using enum Feature;
    return di::make_enumerators<"Feature">(
        di::enumerator<"None", None>,
        di::enumerator<"SyncronizedOutput", SyncronizedOutput,
                       "Allow using DEC private mode 2026 to synchronize screen updates to prevent tearing">,
        di::enumerator<"TextSizingWidth", TextSizingWidth,
                       "Supports using kitty text sizing protocol (OSC 66) to specify explicit widths">,
        di::enumerator<"TextSizingFull", TextSizingFull,
                       "Supports using kitty text sizing protocol (OSC 66) to specify multi-height cells">,
        di::enumerator<"ThemeDetection", ThemeDetection,
                       "Supports light/dark mode detection via DEC private mode 2031">,
        di::enumerator<"InBandSizeReports", InBandSizeReports,
                       "Supported in-band size reports via DEC private mode 2048">,
        di::enumerator<"GraphemeClusteringMode", GraphemeClusteringMode,
                       "Reports some support for grapheme clustering DEC private mode 2038">,
        di::enumerator<"KittyKeyProtocol", KittyKeyProtocol, "Supports kitty key reporting protocol">,
        di::enumerator<"Undercurl", Undercurl, "Supports undercurl (fancy underline) and underline colors">,
        di::enumerator<"BasicGraphemeClustering", BasicGraphemeClustering, "Properly handles multi-code point emojis">,
        di::enumerator<"FullGraphemeClustering", FullGraphemeClustering,
                       "Grapheme clustering behavior matches the kitty specification on extreme edge cases">,
        di::enumerator<
            "TextSizingPresentation", TextSizingPresentation,
            "Supports using kitty text sizing protocl (OSC 66) to specifiy fractional scaling and cell alignment">,
        di::enumerator<"Clipboard", Clipboard, "Supports settings the clipboard via OSC 52">,
        di::enumerator<"DynamicPalette", DynamicPalette, "Supports setting the color palette dynamically via OSC 4">,
        di::enumerator<"BackgroundCharacterErase", BackgroundCharacterErase,
                       "Clearing the screen will set the background color of cells">,
        di::enumerator<"SeamlessNavigation", SeamlessNavigation,
                       "Suppports the seamless navigation protocol (OSC 8671)">,
        di::enumerator<"DynamicPaletteKitty", DynamicPaletteKitty,
                       "Supports setting the color palette via kitty color protocol (OSC 21)">);
}

struct FeatureResult {
    Feature features { Feature::None };
    terminal::Palette palette {};
    terminal::ThemeMode theme_mode { terminal::ThemeMode::Dark };
};

auto detect_features(dius::SyncFile& terminal) -> di::Result<FeatureResult>;
}
