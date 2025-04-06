#pragma once

#include "di/container/string/prelude.h"
#include "di/util/clone.h"

namespace ttx::terminal {
struct Hyperlink;

namespace detail {
    struct HyperlinkOps {
        using Key = di::String;

        constexpr static auto get_key(Hyperlink const& hyperlink) -> di::String const&;
    };
}

/// @brief Represents a hyperlink specified via OSC 6.
///
/// [Specification](https://gist.github.com/egmontkob/eb114294efbcd5adb1944c9f3cb5feda).
struct Hyperlink {
    /// Default operation class for IdMap<Hyperlink>.
    using DefaultOps = detail::HyperlinkOps;

    /// The max URI for a hyperlink should be 2083 per the
    /// [spec](https://gist.github.com/egmontkob/eb114294efbcd5adb1944c9f3cb5feda#length-limits):
    constexpr static auto max_uri_length = 2083zu;

    /// The max ID length is chosen so that so that ttx can insert a prefix
    /// to the id without overflowing the limit of 250 in VTE.
    constexpr static auto max_id_length = 230u;

    di::String uri; ///< URI for the hyperlink
    di::String id;  ///< ID of hyperlink, for linking cells together.

    auto clone() const -> Hyperlink {
        return {
            di::clone(uri),
            di::clone(id),
        };
    }

    auto operator==(Hyperlink const&) const -> bool = default;
    auto operator<=>(Hyperlink const&) const = default;
};

namespace detail {
    constexpr auto HyperlinkOps::get_key(Hyperlink const& hyperlink) -> di::String const& {
        return hyperlink.id;
    }
}
}
