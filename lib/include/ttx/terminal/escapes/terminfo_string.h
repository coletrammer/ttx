#pragma once

#include "di/reflect/prelude.h"
#include "ttx/escape_sequence_parser.h"

namespace ttx::terminal {
struct Capability;

/// @brief Terminal response string (for XTGETTCAP)
///
/// This is requested via DCS + q Pt ST, to get the terminal
/// terminfo response for a specific capability. This works
/// over ssh. The protocol is specified
/// [here](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h4-Device-Control-functions:DCS-plus-q-Pt-ST.F95).
struct TerminfoString {
    static auto hex(di::TransparentStringView bytes) -> di::String;
    static auto unhex(di::StringView hex_string) -> di::Optional<di::TransparentString>;

    di::Optional<di::TransparentString> name {};
    di::Optional<di::TransparentString> value {};

    auto valid() const { return name.has_value(); }

    static auto from_dcs(DCS const& dcs) -> di::Optional<TerminfoString>;
    static auto from_capability(Capability const& capability) -> TerminfoString;
    auto serialize() const -> di::String;

    auto operator==(TerminfoString const& other) const -> bool = default;

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<TerminfoString>) {
        return di::make_fields<"TerminfoString">(di::field<"name", &TerminfoString::name>,
                                                 di::field<"value", &TerminfoString::value>);
    }
};
}
