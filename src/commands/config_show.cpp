#include "config_show.h"

#include "config_json.h"
#include "di/container/string/conversion.h"
#include "di/vocab/error/string_error.h"
#include "dius/print.h"
#include "ttx/features.h"

namespace ttx {
auto main(ConfigShow& args) -> di::Result<> {
    if (args.profile.ends_with('/')) {
        return di::Unexpected(di::format_error("--profile cannot be a directory"_sv));
    }

    auto features = TRY(detect_features(dius::std_in).transform_error([](di::Error error) {
        return di::format_error("Failed to detect terminal features: {}"_sv, error);
    }));
    auto config_from_args = config_json::v1::Config {
        .theme = {
            .name = args.theme.transform(di::to_utf8_string_lossy),
        },
    };
    auto config = TRY(config_json::v1::resolve_profile_to_json(args.profile, features.theme_mode, features.palette,
                                                               di::move(config_from_args), args.resolve_theme)
                          .transform_error([&](auto&& error) {
                              return di::format_error("Failed to resolve profile '{}': {}"_sv, args.profile, error);
                          }));
    auto string = to_json_string_without_empty_objects(config_json::v1::config_with_defaults(di::move(config)));
    dius::println("{}"_sv, string);
    return {};
}
}
