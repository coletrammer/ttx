#pragma once

#include "di/container/string/prelude.h"
#include "di/reflect/prelude.h"
#include "ttx/escape_sequence_parser.h"
#include "ttx/terminal/escapes/terminfo_string.h"

namespace ttx::terminal {
/// @brief Represents a Termcap capability
struct Capability {
    di::StringView long_name;             ///< Human understandable name for diagnostic print-out
    di::TransparentStringView short_name; ///< Short name stored in terminfo file
    di::Variant<di::Void, u32, di::TransparentStringView> value = {}; ///< Void means a boolean capabillity
    di::StringView description;                                       ///< Description for diagnostic print-out
    bool enabled = true; ///< Allows for marking entries as not yet enabled, but will be once supported.

    auto serialize() const -> di::String;

    auto operator==(Capability const&) const -> bool = default;
    auto operator<=>(Capability const&) const = default;

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<Capability>) {
        return di::make_fields<"Capability">(
            di::field<"long_name", &Capability::long_name>, di::field<"short_name", &Capability::short_name>,
            di::field<"value", &Capability::value>, di::field<"description", &Capability::description>);
    }
};

/// @brief Represents a terminfo entry
struct Terminfo {
    di::Span<di::TransparentStringView const> names;
    di::Span<Capability const> capabilities;

    auto serialize() const -> di::String;

    auto operator==(Terminfo const&) const -> bool = default;
    auto operator<=>(Terminfo const&) const = default;

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<Terminfo>) {
        return di::make_fields<"Terminfo">(di::field<"names", &Terminfo::names>,
                                           di::field<"capabilities", &Terminfo::capabilities>);
    }
};

auto get_ttx_terminfo() -> Terminfo const&;
auto lookup_terminfo_string(di::StringView hex_name) -> TerminfoString;
}
