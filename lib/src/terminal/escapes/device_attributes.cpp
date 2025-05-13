#include "ttx/terminal/escapes/device_attributes.h"

#include "di/format/prelude.h"

namespace ttx::terminal {
auto PrimaryDeviceAttributes::from_csi(const CSI& csi) -> di::Optional<PrimaryDeviceAttributes> {
    if (csi.intermediate != "?"_sv || csi.terminator != U'c') {
        return {};
    }
    auto result = PrimaryDeviceAttributes {};
    for (auto i : di::range(csi.params.size())) {
        result.attributes.push_back(csi.params.get(i));
    }
    return result;
}

auto PrimaryDeviceAttributes::serialize() const -> di::String {
    auto attributes_string = attributes | di::transform(di::to_string) | di::join_with(U';') | di::to<di::String>();
    return *di::present("\033[?{}c"_sv, attributes_string);
}
}
