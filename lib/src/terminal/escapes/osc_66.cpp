#include "ttx/terminal/escapes/osc_66.h"

#include "di/assert/prelude.h"
#include "di/format/prelude.h"
#include "ttx/terminal/multi_cell_info.h"

namespace ttx::terminal {
struct Key {
    di::StringView key;
    u8 MultiCellInfo::* pointer;
    u8 min_value = 0;
    u8 max_value = 0;
};

constexpr auto keys = di::Array {
    Key("s"_sv, &MultiCellInfo::scale, 1, 7),
    Key("w"_sv, &MultiCellInfo::width, 0, 7),
    Key("n"_sv, &MultiCellInfo::fractional_scale_numerator, 0, 15),
    Key("d"_sv, &MultiCellInfo::fractional_scale_denominator, 0, 15),
    Key("v"_sv, &MultiCellInfo::vertical_alignment, 0, 2),
    Key("h"_sv, &MultiCellInfo::horizontal_alignment, 0, 2),
};

auto OSC66::parse(di::StringView data) -> di::Optional<OSC66> {
    if (data.empty()) {
        return {};
    }

    auto parts = data | di::split(U';') | di::to<di::Vector>();
    if (parts.size() != 2) {
        return {};
    }

    auto result = OSC66 { {}, *parts.back() };
    if (result.text.empty() || result.text.size_bytes() > OSC66::max_text_size) {
        return {};
    }
    for (auto part : parts[0] | di::split(U':')) {
        auto equal = part.find(U'=');
        if (!equal) {
            return {};
        }
        auto key = part.substr(part.begin(), equal.begin());
        auto value_string = part.substr(equal.end());
        auto value = di::parse<u8>(value_string);
        if (!value) {
            return {};
        }
        auto v = value.value();
        auto found = false;
        for (auto const& k : keys) {
            if (k.key == key) {
                if (v < k.min_value || v > k.max_value) {
                    return {};
                }
                result.info.*k.pointer = v;
                found = true;
                break;
            }
        }
        if (!found) {
            return {};
        }
    }
    if (result.info.fractional_scale_denominator != 0 &&
        result.info.fractional_scale_denominator <= result.info.fractional_scale_numerator) {
        return {};
    }
    return result;
}

auto OSC66::serialize() const -> di::String {
    auto defaults = MultiCellInfo();
    auto key_values = keys | di::transform([&](Key const& key) -> di::String {
                          if (info.*key.pointer == defaults.*key.pointer) {
                              return {};
                          }
                          return *di::present("{}={}"_sv, key.key, info.*key.pointer);
                      }) |
                      di::filter(di::not_fn(di::empty)) | di::join_with(U':') | di::to<di::String>();
    return *di::present("\033]66;{};{}\033\\"_sv, key_values, text);
}
}
