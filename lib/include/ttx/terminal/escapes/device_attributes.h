#pragma once

#include "di/reflect/prelude.h"
#include "ttx/escape_sequence_parser.h"

namespace ttx::terminal {
/// @brief Terminal primary device attributes
///
/// These are queried via the DA1 esacpe sequence, documented
/// [here](https://vt100.net/docs/vt510-rm/DA1.html).
///
/// This implementation treats the device attributes as opaque
/// values, because the format varies between terminals.
struct PrimaryDeviceAttributes {
    di::Vector<u32> attributes;

    static auto from_csi(CSI const& csi) -> di::Optional<PrimaryDeviceAttributes>;
    auto serialize() const -> di::String;

    auto operator==(PrimaryDeviceAttributes const& other) const -> bool = default;

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<PrimaryDeviceAttributes>) {
        return di::make_fields<"PrimaryDeviceAttributes">(
            di::field<"attributes", &PrimaryDeviceAttributes::attributes>);
    }
};
}
