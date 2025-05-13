#pragma once

#include "di/reflect/prelude.h"
#include "ttx/escape_sequence_parser.h"

namespace ttx::terminal {
/// @brief Operating status report
///
/// This is requrested by DSR 5, as specified
/// [here](https://vt100.net/docs/vt510-rm/DSR-OS.html).
struct OperatingStatusReport {
    bool malfunction { false };

    static auto from_csi(CSI const& csi) -> di::Optional<OperatingStatusReport>;
    auto serialize() const -> di::String;

    auto operator==(OperatingStatusReport const& other) const -> bool = default;

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<OperatingStatusReport>) {
        return di::make_fields<"OperatingStatusReport">(di::field<"malfunction", &OperatingStatusReport::malfunction>);
    }
};

/// @brief Cursor position report
///
/// This is requested by DSR 6, returning the current position of the cursor.
/// This can be used by application to determine how the terminal handles
/// certain unicode strings, as well as escape sequences like OSC 66. The
/// serialized row and column values are 1-indexed, but stored as 0-indexed
/// values.
///
/// This is specified [here](https://vt100.net/docs/vt510-rm/DSR-CPR.html).
struct CursorPositionReport {
    u32 row { 0 };
    u32 col { 0 };

    static auto from_csi(CSI const& csi) -> di::Optional<CursorPositionReport>;
    auto serialize() const -> di::String;

    auto operator==(CursorPositionReport const& other) const -> bool = default;

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<CursorPositionReport>) {
        return di::make_fields<"CursorPositionReport">(di::field<"row", &CursorPositionReport::row>,
                                                       di::field<"col", &CursorPositionReport::col>);
    }
};
}
