#pragma once

#include <di/types/byte.h>
#include <di/util/to_underlying.h>
#include <di/vocab/array/array.h>

#include "ttx/terminal/color.h"

namespace ttx::terminal {
///< @brief Extended palette index which includes dynamic colors
///
/// The first 256 indices correspond to traditional "palette" colors
/// (Color::Palette), with the first 16 being traditional ansi colors.
/// This type also includes indices into the "special" color palette,
/// which is unused by ttx but could be used in the future. The "dynamic"
/// colors are used by ttx and are dynamically configurable.
enum class PaletteIndex : u16 {
    StaticBegin = 0,
    StaticEnd = 255,

    SpecialBegin,
    SpecialBold = SpecialBegin,
    SpecialUnderline,
    SpecialBlink,
    SpecialReverse,
    SpecialItalic,
    SpecialEnd = SpecialItalic,

    Foreground,
    Background,
    Cursor,
    CursorText,
    SelectionBackground,
    SelectionForeground,

    Count,
    Unknown = Count,
};

constexpr static auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<PaletteIndex>) {
    using enum PaletteIndex;
    return di::make_enumerators<"PaletteIndex">(
        di::enumerator<"SpecialBold", SpecialBold>, di::enumerator<"SpecialUnderline", SpecialUnderline>,
        di::enumerator<"SpecialBlink", SpecialBlink>, di::enumerator<"SpecialReverse", SpecialReverse>,
        di::enumerator<"SpecialItalic", SpecialItalic>, di::enumerator<"Foreground", Foreground>,
        di::enumerator<"Background", Background>, di::enumerator<"Cursor", Cursor>,
        di::enumerator<"CursorText", CursorText>, di::enumerator<"SelectionBackground", SelectionBackground>,
        di::enumerator<"SelectionForeground", SelectionForeground>, di::enumerator<"Unknown", Unknown>);
}

///< @brief Represents the controllable palette colors for a terminal
///
/// These can be queried or set on a per-terminal basis via OSC 4,5,10-19
/// or via OSC 21 (kitty color protocol).
///
/// When an individual color is the default (empty) color, we chosen color
/// is controlled by the global color theme (if present), followed by the actual
/// default color in the outer terminal.
class Palette {
public:
    auto operator==(Palette const&) const -> bool = default;

    auto get(PaletteIndex index) const -> Color { return m_colors[di::to_underlying(index)]; }
    void set(PaletteIndex index, Color value) {
        if (!value.is_default()) {
            m_modified = true;
        }
        m_colors[di::to_underlying(index)] = value;
    }

    auto resolve(Color color) const -> Color {
        if (color.is_palette()) {
            return get(PaletteIndex(color.r)).value_or(color);
        }
        return color;
    }

    auto resolve_foreground(Color color) const -> Color {
        color = resolve(color);
        if (color.is_default()) {
            color = get(PaletteIndex::Foreground);
        }
        return color;
    }

    auto resolve_background(Color color) const -> Color {
        color = resolve(color);
        if (color.is_default()) {
            color = get(PaletteIndex::Background);
        }
        return color;
    }

    static auto supports_dynamic(PaletteIndex index) -> bool {
        switch (index) {
            case PaletteIndex::Cursor:
            case PaletteIndex::CursorText:
            case PaletteIndex::SelectionForeground:
            case PaletteIndex::SelectionBackground:
                return true;
            default:
                return false;
        }
    }

    void reset() {
        m_colors = {};
        m_modified = false;
    }

    auto modified() const -> bool { return m_modified; }
    void maybe_clear_modifed() { m_modified = di::any_of(m_colors, di::not_fn(&Color::is_default)); }

private:
    di::Array<Color, di::to_underlying(PaletteIndex::Count)> m_colors;
    bool m_modified { false };
};
}
