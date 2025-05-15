#pragma once

#include "di/reflect/prelude.h"
#include "di/util/bitwise_enum.h"
#include "dius/sync_file.h"

namespace ttx {
enum class Feature : u64 {
    None = 0,
    SyncronizedOutput = 1 << 0,       ///< Allow using DEC mode 2026 to synchronize screen updates
    TextSizingWidth = 1 << 1,         ///< Supports using the text sizing protocol to specify explicit text width.
    TextSizingFull = 1 << 2,          ///< Supports using the text sizing protocol to multi-height cells.
    ThemeDetection = 1 << 3,          ///< Supports light/dark mode detection via mode 2031.
    InBandSizeReports = 1 << 4,       ///< Supports in-band size reports via mode 2048.
    GraphemeClusteringMode = 1 << 5,  ///< Supports grapheme clustering mode via mode 2027.
    KittyKeyProtocol = 1 << 6,        ///< Supports kitty key protocol.
    Undercurl = 1 << 7,               ///< Supports undercurl (fancy underline) and underline colors.
    BasicGraphemeClustering = 1 << 8, ///< Supports grapheme clustering, but may not match the kitty spec
    FullGraphemeClustering = 1 << 9,  ///< Grapheme clustering behavior matches kitty spec
    TextSizingPresentation = 1 << 10, ///< Supports text-sizing with scale=1 but fractional scale and alignment.
    All = u64(-1),
};

DI_DEFINE_ENUM_BITWISE_OPERATIONS(Feature)

constexpr auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<Feature>) {
    using enum Feature;
    return di::make_enumerators<"Feature">(
        di::enumerator<"None", None>, di::enumerator<"SyncronizedOutput", SyncronizedOutput>,
        di::enumerator<"TextSizingWidth", TextSizingWidth>, di::enumerator<"TextSizingFull", TextSizingFull>,
        di::enumerator<"ThemeDetection", ThemeDetection>, di::enumerator<"InBandSizeReports", InBandSizeReports>,
        di::enumerator<"GraphemeClusteringMode", GraphemeClusteringMode>,
        di::enumerator<"KittyKeyProtocol", KittyKeyProtocol>, di::enumerator<"Undercurl", Undercurl>,
        di::enumerator<"BasicGraphemeClustering", BasicGraphemeClustering>,
        di::enumerator<"FullGraphemeClustering", FullGraphemeClustering>,
        di::enumerator<"TextSizingPresentation", TextSizingPresentation>);
}

auto detect_features(dius::SyncFile& terminal) -> di::Result<Feature>;
}
