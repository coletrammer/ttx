#pragma once

#include "di/reflect/prelude.h"
#include "di/util/bitwise_enum.h"
#include "dius/sync_file.h"

namespace ttx {
enum class Feature {
    None = 0,
    SyncronizedOutput = 1 << 0, ///< Allow using DEC mode 2026 to synchronize screen updates
    TextSizingWidth = 1 << 1,   ///< Supports using the text sizing protocol to specify explicit text width.
    TextSizingFull = 1 << 2,    ///< Supports using the text sizing protocol to multi-height cells.
    All = SyncronizedOutput
};

DI_DEFINE_ENUM_BITWISE_OPERATIONS(Feature)

constexpr auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<Feature>) {
    using enum Feature;
    return di::make_enumerators<"Feature">(
        di::enumerator<"None", None>, di::enumerator<"SyncronizedOutput", SyncronizedOutput>,
        di::enumerator<"TextSizingWidth", TextSizingWidth>, di::enumerator<"TextSizingFull", TextSizingFull>);
}

auto detect_features(dius::SyncFile& terminal) -> di::Result<Feature>;
}
