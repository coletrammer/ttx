#include "ttx/terminal/escapes/mode.h"

#include "di/format/prelude.h"

namespace ttx::terminal {
auto ModeQueryReply::from_csi(const CSI& csi) -> di::Optional<ModeQueryReply> {
    if (csi.terminator != 'y' || (csi.intermediate != "$"_sv && csi.intermediate != "?$"_sv)) {
        return {};
    }
    if (csi.params.size() != 2) {
        return {};
    }

    auto support = ModeSupport(csi.params.get(1));
    if (!di::valid_enum_value(support)) {
        return {};
    }

    auto is_dec = csi.intermediate == "?$"_sv;
    auto mode = csi.params.get(0);
    return ModeQueryReply {
        support,
        is_dec ? DecMode(mode) : DecMode::None,
        !is_dec ? AnsiMode(mode) : AnsiMode::None,
    };
}

auto ModeQueryReply::serialize() const -> di::String {
    auto is_dec = dec_mode != DecMode::None;
    return *di::present("\033[{}{};{}$y"_sv, is_dec ? "?"_sv : ""_sv, is_dec ? u32(dec_mode) : u32(ansi_mode),
                        u32(support));
}
}
