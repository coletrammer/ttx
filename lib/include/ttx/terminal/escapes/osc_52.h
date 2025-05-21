#pragma once

#include "di/reflect/prelude.h"
#include "di/serialization/base64.h"

namespace ttx::terminal {
/// @brief Represents the type of selection being modifed by an OSC 52 sequence
enum class SelectionType : u8 {
    Clipboard, ///< Standard user clipboard (default)
    Selection, ///< Primary selection clipboard
    _0,        ///< Numbered buffers, never forwarded to the outer terminal
    _1,
    _2,
    _3,
    _4,
    _5,
    _6,
    _7,
};

constexpr auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<SelectionType>) {
    using enum SelectionType;
    return di::make_enumerators<"SelectionType">(
        di::enumerator<"Clipboard", Clipboard>, di::enumerator<"Selection", Selection>, di::enumerator<"0", _0>,
        di::enumerator<"1", _1>, di::enumerator<"2", _2>, di::enumerator<"3", _3>, di::enumerator<"4", _4>,
        di::enumerator<"5", _5>, di::enumerator<"6", _6>, di::enumerator<"7", _7>);
}

/// @brief Represents an OSC 52 sequence, which allows for modifying or querying the clipboard
///
/// Although xterm originally specified 10 different selection types (c,p,q,s,0-7), modern terminals
/// only support 'c' and 'p' (clipboard and primary selection), with 's' being mapped directly to 'p'.
/// 'c' is the primary clipboard users expect to interact with, and is the default. Note this differs
/// from xterm which says the default clipboard is "s 0".
///
/// Some terminals will ignore requests to the other selection types (kitty), while others map all other
/// types to 'c' (ghostty). Some terminals also only implementing writing the clipboard, not reading
/// (wezterm). Additionally terminals may have a user-provided setting to opt-out of clipboard functionality.
/// In this case, the terminal should always return an empty selection but we have handle our requests getting
/// ignored.
///
/// Therefore, when parsing OSC 52 messages, we must map the selection types appropriately instead of just
/// passing them through. Also, we will reserve the numbered buffers for selections which are local to the
/// ttx instance (handled fully internally). This will prove useful later, and is easy implement, because
/// we need to have a internal clipboard anyway to handle terminals which don't support OSC 52 at all.
///
/// OSC 52 is specified
/// [here](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h4-Operating-System-Commands:OSC-Ps;Pt-ST:Ps-=-5-2.101B).
struct OSC52 {
    di::StaticVector<SelectionType, di::Constexpr<10zu>> selections {};
    di::Base64<> data;
    bool query { false };

    static auto parse(di::StringView data) -> di::Optional<OSC52>;

    auto serialize() const -> di::String;

    auto operator==(OSC52 const& other) const -> bool = default;

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<OSC52>) {
        return di::make_fields<"OSC52">(di::field<"selections", &OSC52::selections>, di::field<"data", &OSC52::data>,
                                        di::field<"query", &OSC52::query>);
    }
};
}
