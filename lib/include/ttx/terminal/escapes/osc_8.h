#pragma once

#include "di/container/tree/tree_map.h"
#include "di/function/container/function_ref.h"
#include "di/reflect/prelude.h"
#include "ttx/escape_sequence_parser.h"
#include "ttx/terminal/hyperlink.h"

namespace ttx::terminal {
/// @brief Represents a terminal hyperlink escape sequence
///
/// This is specified [here](https://gist.github.com/egmontkob/eb114294efbcd5adb1944c9f3cb5feda).
struct OSC8 {
    di::TreeMap<di::String, di::String> params;
    di::String uri;

    static auto parse(di::StringView data) -> di::Optional<OSC8>;
    static auto from_hyperlink(di::Optional<Hyperlink const&> hyperlink) -> OSC8;

    auto serialize() const -> di::String;
    auto to_hyperlink(di::FunctionRef<di::String(di::Optional<di::StringView>)> make_id) const
        -> di::Optional<Hyperlink>;

    auto operator==(OSC8 const& other) const -> bool = default;

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<OSC8>) {
        return di::make_fields<"OSC8">(di::field<"params", &OSC8::params>, di::field<"uri", &OSC8::uri>);
    }
};
}
