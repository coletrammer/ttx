#include "ttx/terminal/escapes/device_status.h"

#include "di/format/prelude.h"
#include "dius/print.h"
#include "ttx/key_event_io.h"

namespace ttx::terminal {
auto OperatingStatusReport::from_csi(CSI const& csi) -> di::Optional<OperatingStatusReport> {
    if (!csi.intermediate.empty() || csi.terminator != 'n') {
        return {};
    }
    if (csi.params.size() != 1) {
        return {};
    }

    auto val = csi.params.get(0);
    if (val != 0 && val != 3) {
        return {};
    }

    return OperatingStatusReport { .malfunction = val == 3 };
}

auto OperatingStatusReport::serialize() const -> di::String {
    return *di::present("\033[{}n"_sv, malfunction ? 3 : 0);
}

auto CursorPositionReport::from_csi(CSI const& csi) -> di::Optional<CursorPositionReport> {
    if (!csi.intermediate.empty() || csi.terminator != 'R') {
        return {};
    }
    if (csi.params.size() != 2) {
        return {};
    }

    auto row = csi.params.get(0);
    auto col = csi.params.get(1);
    if (row == 0 || col == 0) {
        return {};
    }

    return CursorPositionReport { row - 1, col - 1 };
}

auto CursorPositionReport::serialize() const -> di::String {
    return *di::present("\033[{};{}R"_sv, row + 1, col + 1);
}

auto KittyKeyReport::from_csi(CSI const& csi) -> di::Optional<KittyKeyReport> {
    if (csi.intermediate != "?"_sv || csi.terminator != 'u') {
        return {};
    }
    if (csi.params.size() != 1) {
        return {};
    }

    auto flags = KeyReportingFlags(csi.params.get(0));
    if (!!(flags & ~KeyReportingFlags::All)) {
        return {};
    }
    return KittyKeyReport { flags };
}

auto KittyKeyReport::serialize() const -> di::String {
    return *di::present("\033[?{}u"_sv, u32(flags));
}

auto StatusStringResponse::from_dcs(DCS const& dcs) -> di::Optional<StatusStringResponse> {
    if (dcs.intermediate != "$r"_sv) {
        return {};
    }
    if (dcs.params.size() != 1) {
        return {};
    }
    if (dcs.params.get(0) > 2) {
        return {};
    }
    if (dcs.params.get(0) == 0) {
        if (dcs.data.empty()) {
            return StatusStringResponse {};
        }
        return {};
    }
    return StatusStringResponse(dcs.data.clone());
}

auto StatusStringResponse::serialize() const -> di::String {
    return *di::present("\033P{}$r{}\033\\"_sv, u32(response.has_value()),
                        response.transform(&di::String::view).value_or(""_sv));
}
}
