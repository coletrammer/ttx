#include "ttx/terminal/escapes/device_status.h"

#include "di/format/prelude.h"

namespace ttx::terminal {
auto OperatingStatusReport::from_csi(const CSI& csi) -> di::Optional<OperatingStatusReport> {
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

auto CursorPositionReport::from_csi(const CSI& csi) -> di::Optional<CursorPositionReport> {
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
}
