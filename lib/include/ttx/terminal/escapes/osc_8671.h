#pragma once

#include "di/reflect/prelude.h"
#include "ttx/terminal/navigation_direction.h"

namespace ttx::terminal {
/// @brief Type of OSC 8671 message
enum class SeamlessNavigationRequestType {
    Supported,
    Register,
    Unregister,
    Navigate,
    Acknowledge,
    Enter,
};

constexpr auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<SeamlessNavigationRequestType>) {
    using enum SeamlessNavigationRequestType;
    return di::make_enumerators<"SeamlessNaviagationRequestType">(
        di::enumerator<"Supported", Supported>, di::enumerator<"Register", Register>,
        di::enumerator<"Unregister", Unregister>, di::enumerator<"Navigate", Navigate>,
        di::enumerator<"Acknowledge", Acknowledge>, di::enumerator<"Enter", Enter>);
}

/// @brief Represents a seamless pane navigation request
struct OSC8671 {
    constexpr static auto max_id_byte_size = 36_usize;

    SeamlessNavigationRequestType type { SeamlessNavigationRequestType::Supported };
    di::Optional<NavigateDirection> direction {};
    di::Optional<di::String> id {};
    di::Optional<di::Tuple<u32, u32>> range {};
    NavigateWrapMode wrap_mode { NavigateWrapMode::Disallow };
    bool hide_cursor_on_enter { false };

    static auto parse(di::StringView data) -> di::Optional<OSC8671>;

    auto serialize() const -> di::String;

    auto operator==(OSC8671 const& other) const -> bool = default;

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<OSC8671>) {
        return di::make_fields<"OSC8671">(di::field<"type", &OSC8671::type>,
                                          di::field<"direction", &OSC8671::direction>, di::field<"id", &OSC8671::id>,
                                          di::field<"range", &OSC8671::range>,
                                          di::field<"wrap_mode", &OSC8671::wrap_mode>,
                                          di::field<"hide_cursor_on_enter", &OSC8671::hide_cursor_on_enter>);
    }
};
}
