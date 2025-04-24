#include "ttx/terminal/escapes/osc_8.h"

#include "di/format/prelude.h"
#include "ttx/escape_sequence_parser.h"
#include "ttx/terminal/hyperlink.h"

namespace ttx::terminal {
auto OSC8::parse(di::StringView data) -> di::Optional<OSC8> {
    auto parts = data | di::split(U';') | di::to<di::Vector>();
    if (parts.size() != 2) {
        return {};
    }
    if (parts[0].empty() && parts[1].empty()) {
        return OSC8 {};
    }
    if (parts[1].size_bytes() > Hyperlink::max_uri_length) {
        return {};
    }

    auto result = OSC8 {};
    result.params = parts[0] | di::split(U':') |
                    di::transform([](di::StringView key_value) -> di::Tuple<di::String, di::String> {
                        auto equal = key_value.find(U'=');
                        if (!equal) {
                            return { key_value.to_owned(), ""_s };
                        }
                        return { key_value.substr(key_value.begin(), equal.begin()).to_owned(),
                                 key_value.substr(equal.end()).to_owned() };
                    }) |
                    di::to<di::TreeMap<di::String, di::String>>();
    result.uri = parts[1].to_owned();
    return result;
}

auto OSC8::from_hyperlink(di::Optional<Hyperlink const&> hyperlink) -> OSC8 {
    if (!hyperlink) {
        return {};
    }

    auto params = di::TreeMap<di::String, di::String> {};
    params.insert_or_assign("id"_s, di::clone(hyperlink.value().id));
    return { di::move(params), di::clone(hyperlink.value().uri) };
}

auto OSC8::serialize() const -> di::String {
    auto params_string = params | di::transform([](auto const& pair) -> di::String {
                             auto const& [key, value] = pair;
                             return *di::present("{}={}"_sv, key, value);
                         }) |
                         di::join_with(U':') | di::to<di::String>();
    return *di::present("\033]8;{};{}\033\\"_sv, params_string, uri);
}

auto OSC8::to_hyperlink(di::FunctionRef<di::String(di::Optional<di::StringView>)> make_id) const
    -> di::Optional<Hyperlink> {
    if (uri.empty()) {
        return {};
    }

    auto maybe_id = params.at("id"_sv).transform([](di::String const& id) -> di::StringView {
        if (id.size_bytes() >= Hyperlink::max_id_length) {
            // Truncate id to the first N bytes, while ensuring the string is valid UTF-8.
            for (auto offset : di::range(Hyperlink::max_id_length - 3, Hyperlink::max_id_length + 1) | di::reverse) {
                for (auto it : id.iterator_at_offset(offset)) {
                    return id.substr(id.begin(), it);
                }
            }
            di::unreachable();
        }
        return id.view();
    });
    return Hyperlink { .uri = di::clone(uri), .id = make_id(maybe_id) };
}
}
