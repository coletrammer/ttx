#include "ttx/terminal/escapes/osc_7.h"

#include "di/format/prelude.h"
#include "di/serialization/percent_encoded.h"
#include "di/util/construct.h"
#include "ttx/escape_sequence_parser.h"

namespace ttx::terminal {
auto OSC7::parse(di::TransparentStringView data) -> di::Optional<OSC7> {
    if (!data.starts_with(file_scheme) && !data.starts_with(kitty_scheme)) {
        return {};
    }

    auto needs_percent_decode = data.starts_with(file_scheme);
    data = data.substr(di::next(data.find('/').end()));

    auto hostname_end = data.find('/').begin();
    if (hostname_end == data.end()) {
        // This means we were missing the third / for the hostname.
        return {};
    }

    auto hostname = data.substr(data.begin(), hostname_end);
    data = data.substr(hostname_end);

    if (!needs_percent_decode) {
        return OSC7(hostname.to_owned(), data.to_owned());
    }

    auto decode = [](di::TransparentStringView view) -> di::Optional<di::TransparentString> {
        auto result = di::parse<di::PercentEncoded<>>(view);
        if (!result) {
            return {};
        }
        return di::move(result).value().underlying_string();
    };

    auto decoded_hostname = decode(hostname);
    auto decoded_path = decode(data);
    if (!decoded_hostname || !decoded_path) {
        return {};
    }

    return OSC7(di::move(decoded_hostname).value(), di::move(decoded_path).value());
}

auto OSC7::serialize() const -> di::String {
    // When serializing, use the file URI type and percent encode everything. This ensures
    // our output is valid UTF-8 even if the path isn't. To prevent percent encoding the / character,
    // we need to first split on '/'.
    auto encoded_path_parts = path.data() | di::split('/') | di::transform(di::PercentEncodedView::from_raw_data) |
                              di::transform(di::to_string) | di::join_with(U'/') | di::transform(di::construct<c8>) |
                              di::to<di::String>(di::encoding::assume_valid);
    return *di::present("\033]7;{}{}{}\033\\"_sv, file_scheme, di::PercentEncodedView::from_raw_data(hostname),
                        encoded_path_parts);
}
}
