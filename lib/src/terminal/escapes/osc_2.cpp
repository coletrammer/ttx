#include "ttx/terminal/escapes/osc_2.h"

#include "di/container/string/conversion.h"
#include "di/format/prelude.h"

namespace ttx::terminal {
auto OSC2::parse(di::TransparentStringView data) -> di::Optional<OSC2> {
    return OSC2(di::to_utf8_string_lossy(data));
}

auto OSC2::serialize() const -> di::String {
    return di::format("\033]2;{}\033\\"_sv, window_title);
}
}
