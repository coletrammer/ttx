#include "ttx/terminal/palette.h"

#include "di/format/prelude.h"

namespace ttx::terminal {
auto Palette::to_string() const -> di::String {
    return di::to_string(m_colors);
}
}
