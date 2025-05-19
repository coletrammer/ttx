#include "ttx/terminal/escapes/terminfo_string.h"

#include "di/format/prelude.h"
#include "di/util/construct.h"
#include "dius/print.h"
#include "ttx/terminal/capability.h"

namespace ttx::terminal {
auto TerminfoString::hex(di::TransparentStringView bytes) -> di::String {
    auto result = ""_s;
    for (auto byte : bytes) {
        result += *di::present("{:02X}"_sv, u8(byte));
    }
    return result;
}

auto TerminfoString::unhex(di::StringView hex_string) -> di::Optional<di::TransparentString> {
    if (hex_string.empty() || hex_string.size_bytes() % 2 != 0) {
        return {};
    }
    auto valid = '0'_mc - '9'_mc || 'a'_mc - 'f'_mc || 'A'_mc - 'F'_mc;
    auto v = [](u8 value) {
        if (value >= '0' && value <= '9') {
            return value - '0';
        }
        if (value >= 'a' && value <= 'f') {
            return value - 'a' + 10;
        }
        return value - 'A' + 10;
    };

    auto result = ""_ts;
    for (auto slice : hex_string.span() | di::chunk(2)) {
        if (!valid(slice[0]) || !valid(slice[1])) {
            return {};
        }
        result.push_back(char(u8((v(slice[0]) << 4) + v(slice[1]))));
    }
    return result;
}

auto TerminfoString::from_dcs(DCS const& dcs) -> di::Optional<TerminfoString> {
    if (dcs.intermediate != "+r"_sv) {
        return {};
    }
    if (dcs.params.size() != 1) {
        return {};
    }
    if (dcs.params.get(0) > 1) {
        return {};
    }
    if (dcs.params.get(0) == 0) {
        if (dcs.data.empty()) {
            return TerminfoString {};
        }
        return {};
    }

    auto equal = dcs.data.find(U'=');
    auto maybe_name = unhex(dcs.data.substr(dcs.data.begin(), equal.begin()));
    if (!maybe_name) {
        return {};
    }
    auto value = di::Optional<di::TransparentString> {};
    if (equal) {
        auto maybe_value = unhex(dcs.data.substr(equal.end()));
        if (!maybe_value) {
            return {};
        }
        value = di::move(maybe_value);
    }
    return TerminfoString { di::move(maybe_name), di::move(value) };
}

auto TerminfoString::from_capability(Capability const& capability) -> TerminfoString {
    auto name = capability.short_name.span() | di::transform(di::construct<char>) | di::to<di::TransparentString>();
    auto value = di::visit(di::overload(
                               [](di::Void) -> di::Optional<di::TransparentString> {
                                   return {};
                               },
                               [](u32 value) -> di::Optional<di::TransparentString> {
                                   return di::to_string(value) | di::transform(di::construct<char>) |
                                          di::to<di::TransparentString>();
                               },
                               [](di::TransparentStringView value) -> di::Optional<di::TransparentString> {
                                   // If the string does contain parameters, no unescaping is needed.
                                   if (value.contains(U'%')) {
                                       return value.span() | di::to<di::TransparentString>();
                                   }
                                   // In this case, we must replace \E with byte \x1b and also replace ^X with a byte.
                                   auto result = ""_ts;
                                   auto pending_backslash = false;
                                   auto pending_control = false;
                                   for (auto byte : value.span()) {
                                       if (byte == '\\' && !pending_backslash) {
                                           pending_backslash = true;
                                           pending_control = false;
                                       } else if (byte == '^' && !pending_control) {
                                           pending_control = true;
                                           pending_backslash = false;
                                       } else if (pending_backslash) {
                                           pending_backslash = false;
                                           if (byte == 'E') {
                                               result[result.size() - 1] = '\x1b';
                                               continue;
                                           }
                                       } else if (pending_control) {
                                           pending_control = false;
                                           if (byte == '?') {
                                               result[result.size() - 1] = 127;
                                           } else {
                                               result[result.size() - 1] = char(byte - 64);
                                           }
                                           continue;
                                       }
                                       result.push_back(byte);
                                   }
                                   return result;
                               }),
                           capability.value);
    return { di::move(name), di::move(value) };
}

auto TerminfoString::serialize() const -> di::String {
    if (!valid()) {
        return "\033P0+r\033\\"_s;
    }
    auto lhs = hex(name.value());
    if (!value) {
        return *di::present("\033P1+r{}\033\\"_sv, lhs);
    }
    return *di::present("\033P1+r{}={}\033\\"_sv, lhs, hex(value.value()));
}
}
