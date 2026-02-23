#include "ttx/terminal/escapes/osc_8671.h"

#include "di/assert/prelude.h"
#include "di/format/prelude.h"
#include "ttx/terminal/navigation_direction.h"

namespace ttx::terminal {
constexpr auto type_map = di::Array {
    di::Tuple { "supported"_sv, SeamlessNavigationRequestType::Supported },
    di::Tuple { "register"_sv, SeamlessNavigationRequestType::Register },
    di::Tuple { "unregister"_sv, SeamlessNavigationRequestType::Unregister },
    di::Tuple { "navigate"_sv, SeamlessNavigationRequestType::Navigate },
    di::Tuple { "acknowledge"_sv, SeamlessNavigationRequestType::Acknowledge },
    di::Tuple { "enter"_sv, SeamlessNavigationRequestType::Enter },
};

constexpr auto direction_map = di::Array {
    di::Tuple { "left"_sv, NavigateDirection::Left },
    di::Tuple { "right"_sv, NavigateDirection::Right },
    di::Tuple { "down"_sv, NavigateDirection::Down },
    di::Tuple { "up"_sv, NavigateDirection::Up },
};

auto OSC8671::parse(di::StringView data) -> di::Optional<OSC8671> {
    if (data.empty()) {
        return {};
    }

    auto parts = data | di::split(U';') | di::to<di::Vector>();
    if (parts.size() > 2) {
        return {};
    }

    auto type = di::Optional<SeamlessNavigationRequestType> {};
    auto id = di::Optional<di::String> {};
    auto wrap_mode = di::Optional<NavigateWrapMode> {};
    auto range = di::Optional<di::Tuple<u32, u32>> {};
    auto hide_cursor_on_enter = di::Optional<bool> {};
    for (auto part : parts[0] | di::split(U':')) {
        auto equal = part.find(U'=');
        if (!equal) {
            return {};
        }
        auto key = part.substr(part.begin(), equal.begin());
        auto value_string = part.substr(equal.end());
        if (key == "w"_sv) {
            if (value_string == "true"_sv) {
                wrap_mode = NavigateWrapMode::Allow;
            } else if (value_string == "false"_sv) {
                wrap_mode = NavigateWrapMode::Disallow;
            } else {
                return {};
            }
        } else if (key == "h"_sv) {
            if (value_string == "true"_sv) {
                hide_cursor_on_enter = true;
            } else if (value_string == "false"_sv) {
                hide_cursor_on_enter = false;
            } else {
                return {};
            }
        } else if (key == "t"_sv) {
            auto const* it = di::find(type_map, value_string, [](auto const& x) {
                return di::get<0>(x);
            });
            if (it == type_map.end()) {
                return {};
            }
            type = di::get<1>(*it);
        } else if (key == "id"_sv) {
            if (value_string.size_bytes() > max_id_byte_size) {
                return {};
            }
            id = value_string.to_owned();
        } else if (key == "r"_sv) {
            auto vs = value_string | di::split(U',') | di::to<di::Vector>();
            if (vs.size() != 2) {
                return {};
            }
            auto s = di::parse<u32>(vs[0]);
            auto e = di::parse<u32>(vs[1]);
            if (!s || !e) {
                return {};
            }
            if (*s < 1 || *e < 1) {
                return {};
            }
            if (*e < *s) {
                return {};
            }
            range = di::Tuple { *s, *e };
        } else {
            return {};
        }
    }

    if (!type) {
        return {};
    }
    if (type.value() != SeamlessNavigationRequestType::Enter &&
        type.value() != SeamlessNavigationRequestType::Navigate && range.has_value()) {
        return {};
    }
    if (type.value() != SeamlessNavigationRequestType::Register && hide_cursor_on_enter.has_value()) {
        return {};
    }
    if (type.value() != SeamlessNavigationRequestType::Navigate &&
        type.value() != SeamlessNavigationRequestType::Acknowledge && wrap_mode.has_value()) {
        return {};
    }

    auto direction = di::Optional<NavigateDirection> {};
    if (type.value() == SeamlessNavigationRequestType::Navigate ||
        type.value() == SeamlessNavigationRequestType::Acknowledge ||
        type.value() == SeamlessNavigationRequestType::Enter) {
        if (parts.size() < 2) {
            return {};
        }
        auto const* it = di::find(direction_map, parts[1], [](auto const& x) {
            return di::get<0>(x);
        });
        if (it == direction_map.end()) {
            return {};
        }
        direction = di::get<1>(*it);
    } else if (parts.size() > 1) {
        return {};
    }

    return OSC8671 {
        .type = type.value(),
        .direction = direction,
        .id = di::move(id),
        .range = range,
        .wrap_mode = wrap_mode.value_or(NavigateWrapMode::Disallow),
        .hide_cursor_on_enter = hide_cursor_on_enter.value_or(false),
    };
}

auto OSC8671::serialize() const -> di::String {
    auto result = "\033]8671;t="_s;
    auto type_string = di::get<0>(*di::find(type_map, type, [](auto const& x) {
        return di::get<1>(x);
    }));
    result.append(type_string);
    if (wrap_mode == NavigateWrapMode::Allow) {
        result.append(":w=true"_sv);
    }
    if (hide_cursor_on_enter) {
        result.append(":h=true"_sv);
    }
    if (id) {
        result.append(":id="_sv);
        result.append(id.value());
    }
    if (range) {
        result.append(di::format(":r={},{}"_sv, di::get<0>(*range), di::get<1>(*range)));
    }
    if (direction) {
        result.push_back(U';');
        auto direction_string = di::get<0>(*di::find(direction_map, direction.value(), [](auto const& x) {
            return di::get<1>(x);
        }));
        result.append(direction_string);
    }
    result.append("\033\\"_sv);
    return result;
}
}
