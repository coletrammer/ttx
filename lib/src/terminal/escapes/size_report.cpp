#include "ttx/terminal/escapes/size_report.h"

#include "di/format/prelude.h"
#include "dius/print.h"
#include "ttx/key_event_io.h"

namespace ttx::terminal {
auto TextAreaPixelSizeReport::from_csi(CSI const& csi) -> di::Optional<TextAreaPixelSizeReport> {
    if (!csi.intermediate.empty() || csi.terminator != 't') {
        return {};
    }
    if (csi.params.size() != 3) {
        return {};
    }

    auto val = csi.params.get(0);
    if (val != 4) {
        return {};
    }

    return TextAreaPixelSizeReport {
        .xpixels = csi.params.get(2),
        .ypixels = csi.params.get(1),
    };
}

auto TextAreaPixelSizeReport::serialize() const -> di::String {
    return *di::present("\033[4;{};{}t"_sv, ypixels, xpixels);
}

auto CellPixelSizeReport::from_csi(CSI const& csi) -> di::Optional<CellPixelSizeReport> {
    if (!csi.intermediate.empty() || csi.terminator != 't') {
        return {};
    }
    if (csi.params.size() != 3) {
        return {};
    }

    auto val = csi.params.get(0);
    if (val != 6) {
        return {};
    }

    return CellPixelSizeReport {
        .xpixels = csi.params.get(2),
        .ypixels = csi.params.get(1),
    };
}

auto CellPixelSizeReport::serialize() const -> di::String {
    return *di::present("\033[6;{};{}t"_sv, ypixels, xpixels);
}

auto TextAreaSizeReport::from_csi(CSI const& csi) -> di::Optional<TextAreaSizeReport> {
    if (!csi.intermediate.empty() || csi.terminator != 't') {
        return {};
    }
    if (csi.params.size() != 3) {
        return {};
    }

    auto val = csi.params.get(0);
    if (val != 8) {
        return {};
    }

    return TextAreaSizeReport {
        .cols = csi.params.get(2),
        .rows = csi.params.get(1),
    };
}

auto TextAreaSizeReport::serialize() const -> di::String {
    return *di::present("\033[8;{};{}t"_sv, rows, cols);
}

auto InBandSizeReport::from_csi(CSI const& csi) -> di::Optional<InBandSizeReport> {
    if (!csi.intermediate.empty() || csi.terminator != 't') {
        return {};
    }
    if (csi.params.size() != 5) {
        return {};
    }

    auto val = csi.params.get(0);
    if (val != 48) {
        return {};
    }

    return InBandSizeReport {
        .size =
            Size {
                .rows = csi.params.get(1),
                .cols = csi.params.get(2),
                .xpixels = csi.params.get(4),
                .ypixels = csi.params.get(3),
            },
    };
}

auto InBandSizeReport::serialize() const -> di::String {
    return *di::present("\033[48;{};{};{};{}t"_sv, size.rows, size.cols, size.ypixels, size.xpixels);
}
}
