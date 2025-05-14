#pragma once

#include "di/reflect/prelude.h"
#include "ttx/escape_sequence_parser.h"

namespace ttx::terminal {
/// @brief ANSI terminal modes
///
/// For now, these are left as a placeholder as none are supported.
enum class AnsiMode {
    None = 0,
};

constexpr auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<AnsiMode>) {
    using enum AnsiMode;
    return di::make_enumerators<"AnsiMode">(di::enumerator<"None", None>);
}

/// @brief DEC private modes
///
/// Unless otherwise specified, these modes are documented by xterm
/// [here](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h4-Functions-using-CSI-_-ordered-by-the-final-character-lparen-s-rparen:CSI-?-Pm-h.1D0E).
enum class DecMode {
    None = 0,

    /// @brief Enable application cursor keys mode
    ///
    /// This controls whether the arrow keys are reported using CSI
    /// or SS3 sequences when there are no modifiers.
    CursorKeysMode = 1,

    /// @brief Override the terminal column size to 80 or 132 columns
    Select80Or132ColumnMode = 3,

    /// @brief Reverse video mode
    ///
    /// In ttx, this mode is equivalent to toggling the "inverted" graphics
    /// rendition on every cell.
    ReverseVideo = 5,

    /// @brief Cursor origin mode
    ///
    /// This controls the interpretation of absolute cursor positions when
    /// setting the cursor. When origin mode is enabled, these positions are
    /// relative to the top-left scroll margin. Additionally, the cursor
    /// cannot move outside of the scroll region.
    OriginMode = 6,

    /// @brief X10 (legacy) mouse mode
    ///
    /// This mode corresponds to both the X10 mosue reporting
    /// mode and X10 mouse event encoding protocol.
    X10Mouse = 9,

    /// @brief Cursor enable (toggle cursor visibility)
    CursorEnable = 25,

    /// @brief Allow selecting 80 or 132 column mode
    Allow80Or132ColumnMode = 40,

    /// @brief VT 200 mouse events - presses only
    VT200Mouse = 1000,

    /// @brief Cell motion mouse tracking - motion when held only
    CellMotionMouseTracking = 1002,

    /// @brief All motion mouse tracking - all mouse events
    AllMotionMouseTracking = 1003,

    /// @brief Focus event mode (enable/disable)
    FocusEvent = 1004,

    /// @brief UTF-8 mouse encoding
    UTF8Mouse = 1005,

    /// @brief SGR mouse encoding
    ///
    /// This is the most commonly used mouse encoding.
    SGRMouse = 1006,

    /// @brief Translate scrolling into up/down presses
    ///
    /// This mode is useful for applications like less which
    /// support scrolling but have no reason to process other
    /// mouse events.
    AlternateScroll = 1007,

    /// @brief URXVT mouse encoding
    URXVTMouse = 1015,

    /// @brief SGR mouse pixel encoding
    SGRPixelMouse = 1016,

    /// @brief Use alternate screen buffer
    ///
    /// xterm specifies additionally modes for the alternate screen
    /// buffer, but they aren't needed. This mode additionally saves
    /// and restores the cursor which gives it better behavior.
    AlternateScreenBuffer = 1049,

    /// @brief Denote paste events with CSI 200 ~ and CSI 201 ~
    BrackedPaste = 2004,

    /// @brief Synchronize screen render with application
    ///
    /// This prevents screen tearing by allowing applications
    /// to control when the terminal actually renders the screen.
    /// Typically, this is enabled at the start of drawing the
    /// screen and enabled once drawing is finished. This is
    /// specified [here](https://gist.github.com/christianparpart/d8a62cc1ab659194337d73e399004036).
    SynchronizedOutput = 2026,

    /// @brief Perform grapheme clustering on inputs
    ///
    /// Older terminals naively computed the width of individual
    /// text by simply summing the result of the `wcwidth()` function,
    /// which operates on indivual code points. Terminals may additionally
    /// do other things.
    ///
    /// For terminals which support this mode, combining characters
    /// (within a grapheme cluster, regardless of width) will not advance
    /// the cursor. Additionally, variation selector 16 forces a grapheme
    /// to have width 2. This behavior is specified
    /// [here](https://github.com/contour-terminal/terminal-unicode-core).
    ///
    /// Depending on the terminal, this mode will either be hard-wired to 1
    /// (contour, wezterm), or configurable (ghostty, foot). Additionally,
    /// terminals like kitty do not report supporting this mode but do support
    /// grapheme clustering by default.
    GraphemeClustering = 2027,

    /// @brief Enable automatic reports of users dark/light theme preference
    ///
    /// This allows an application to subscribe to updates when the user changes
    /// their theme preference. The terminal will send special device status
    /// report messages whenever this changes. This is specified
    /// [here](https://contour-terminal.org/vt-extensions/color-palette-update-notifications/).
    ThemeDetection = 2031,

    /// @brief Enable automatic reports of the current terminal size
    ///
    /// This lets applications bypass the SIGWINCH mechanism and instead
    /// directly get size reports whenver the size changes. This is
    /// specified [here](https://gist.github.com/rockorager/e695fb2924d36b2bcf1fff4a3704bd83).
    InBandSizeReports = 2048,
};

constexpr auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<DecMode>) {
    using enum DecMode;
    return di::make_enumerators<"DecMode">(
        di::enumerator<"None", None>, di::enumerator<"CursorKeysMode", CursorKeysMode>,
        di::enumerator<"Select80Or132ColumnMode", Select80Or132ColumnMode>,
        di::enumerator<"ReverseVideo", ReverseVideo>, di::enumerator<"OriginMode", OriginMode>,
        di::enumerator<"X10Mouse", X10Mouse>, di::enumerator<"CursorEnable", CursorEnable>,
        di::enumerator<"Allow80Or132ColumnMode", Allow80Or132ColumnMode>, di::enumerator<"VT200Mouse", VT200Mouse>,
        di::enumerator<"CellMotionMouseTracking", CellMotionMouseTracking>,
        di::enumerator<"AllMotionMouseTracking", AllMotionMouseTracking>, di::enumerator<"FocusEvent", FocusEvent>,
        di::enumerator<"UTF8Mouse", UTF8Mouse>, di::enumerator<"SGRMouse", SGRMouse>,
        di::enumerator<"AlternateScroll", AlternateScroll>, di::enumerator<"URXVTMouse", URXVTMouse>,
        di::enumerator<"SGRPixelMouse", SGRPixelMouse>, di::enumerator<"AlternateScreenBuffer", AlternateScreenBuffer>,
        di::enumerator<"BrackedPaste", BrackedPaste>, di::enumerator<"SynchronizedOutput", SynchronizedOutput>,
        di::enumerator<"GraphemeClustering", GraphemeClustering>, di::enumerator<"ThemeDetection", ThemeDetection>,
        di::enumerator<"InBandSizeReports", InBandSizeReports>);
}

enum class ModeSupport {
    Unknown = 0,     ///< Terminal doesn't know this mode.
    Set = 1,         ///< Mode is currently set.
    Unset = 2,       ///< Mode is currently unset.
    AlwaysSet = 3,   ///< Mode is set, and cannot be modified.
    AlwaysUnset = 4, ///< Mode is unset, and cannot be modified.
};

constexpr auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<ModeSupport>) {
    using enum ModeSupport;
    return di::make_enumerators<"ModeSupport">(di::enumerator<"Unknown", Unknown>, di::enumerator<"Set", Set>,
                                               di::enumerator<"Unset", Unset>, di::enumerator<"AlwaysSet", AlwaysSet>,
                                               di::enumerator<"AlwaysUnset", AlwaysUnset>);
}

/// @brief Terminal DEC request query mode reply
///
/// This is the terminal response of a DECRQM query,
/// documented [here](https://vt100.net/docs/vt510-rm/DECRQM.html).
struct ModeQueryReply {
    ModeSupport support = ModeSupport::Unknown; ///< Support the terminal offers for the mode.
    DecMode dec_mode = DecMode::None;           ///< DEC mode queried.
    AnsiMode ansi_mode = AnsiMode::None;        ///< ANSI mode queried.

    static auto from_csi(CSI const& csi) -> di::Optional<ModeQueryReply>;
    auto serialize() const -> di::String;

    auto operator==(ModeQueryReply const& other) const -> bool = default;

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<ModeQueryReply>) {
        return di::make_fields<"ModeQueryReply">(di::field<"support", &ModeQueryReply::support>,
                                                 di::field<"dec_mode", &ModeQueryReply::dec_mode>,
                                                 di::field<"ansi_mode", &ModeQueryReply::ansi_mode>);
    }
};
}
