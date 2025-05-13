#pragma once

#include "di/reflect/prelude.h"
#include "di/util/bitwise_enum.h"
#include "dius/sync_file.h"

namespace ttx::terminal {
enum class Feature {
    None = 0,
    SyncronizedOutput = 1 << 0, ///< Allow using DEC mode 2026 to synchronize screen updates
    All = 0,
};

DI_DEFINE_ENUM_BITWISE_OPERATIONS(Feature)

constexpr auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<Feature>) {
    using enum Feature;
    return di::make_enumerators<"Feature">(di::enumerator<"None", None>,
                                           di::enumerator<"SyncronizedOutput", SyncronizedOutput>);
}

auto detect_features()
}
