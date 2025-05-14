#pragma once

#include "di/reflect/prelude.h"
#include "ttx/escape_sequence_parser.h"
#include "ttx/key_event_io.h"

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

/// @brief Kitty key protocol status report
///
/// This is requested via CSI ? u, and indicates the status of the kitty key progressive
/// enhancements. This is mainly used to detect support for the protocol. Ths format is
/// specified [here](https://sw.kovidgoyal.net/kitty/keyboard-protocol/#progressive-enhancement).
struct KittyKeyReport {
    KeyReportingFlags flags = KeyReportingFlags::None;

    static auto from_csi(CSI const& csi) -> di::Optional<KittyKeyReport>;
    auto serialize() const -> di::String;

    auto operator==(KittyKeyReport const& other) const -> bool = default;

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<KittyKeyReport>) {
        return di::make_fields<"KittyKeyReport">(di::field<"flags", &KittyKeyReport::flags>);
    }
};

/// @brief Request status string response
///
/// This is requested by DCS $ q Pt ST (DECRQSS), where Pt is the type
/// of request to make. Commonly, Pt=m which indicates a request for the
/// current graphics rendition. This is specified
/// [here](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h4-Device-Control-functions:DCS-$-q-Pt-ST.DF5).
struct StatusStringResponse {
    di::Optional<di::String> response;

    static auto from_dcs(DCS const& dcs) -> di::Optional<StatusStringResponse>;
    auto serialize() const -> di::String;

    auto operator==(StatusStringResponse const& other) const -> bool = default;

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<StatusStringResponse>) {
        return di::make_fields<"StatusStringResponse">(di::field<"response", &StatusStringResponse::response>);
    }
};
}
