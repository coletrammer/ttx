#pragma once

#include "di/container/string/prelude.h"
#include "di/reflect/prelude.h"
#include "ttx/terminal/multi_cell_info.h"

namespace ttx::terminal {
/// @brief Represents text annotated using the text sizing protocol
///
/// This is a protocol invented by kitty, specified
/// [here](https://github.com/kovidgoyal/kitty/blob/master/docs/text-sizing-protocol.rst).
struct OSC66 {
    constexpr static auto max_text_size = usize(4096);

    MultiCellInfo info;  ///< Metadata associated with the text.
    di::StringView text; ///< Described text. Notice this is a reference and so does not own the underlying data.

    static auto parse(di::StringView data) -> di::Optional<OSC66>;

    auto serialize() const -> di::String;

    auto operator==(OSC66 const& other) const -> bool = default;

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<OSC66>) {
        return di::make_fields<"OSC66">(di::field<"info", &OSC66::info>, di::field<"text", &OSC66::text>);
    }
};
}
