#pragma once

#include "di/reflect/prelude.h"

namespace ttx::terminal {
/// @brief Represents a application current working directory report
///
/// This is not specified anywhere, because the spec failed to reach
/// consensus. The format for the escape is OSC 7 ; data ST, where data
/// is a URI. We support the following schemes: file and kitty-shell-cwd.
/// The only difference between these two formats is that the kitty-shell-cwd
/// URI is not percent escaped encoded, making it easy to use from a
/// shell script.
struct OSC7 {
    constexpr static auto file_scheme = "file://"_tsv;
    constexpr static auto kitty_scheme = "kitty-shell-cwd://"_tsv;

    di::TransparentString hostname;
    di::Path path;

    static auto parse(di::TransparentStringView data) -> di::Optional<OSC7>;

    auto clone() const -> OSC7 { return { hostname.clone(), path.clone() }; }

    auto serialize() const -> di::String;

    auto operator==(OSC7 const& other) const -> bool = default;

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<OSC7>) {
        return di::make_fields<"OSC7">(di::field<"hostname", &OSC7::hostname>, di::field<"path", &OSC7::path>);
    }
};
}
