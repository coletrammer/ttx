#include "theme_show.h"

#include "config_json.h"
#include "di/vocab/error/string_error.h"
#include "dius/print.h"
#include "ttx/features.h"

namespace ttx {
auto main(ThemeShow& args) -> di::Result<> {
    auto features = TRY(detect_features(dius::std_in).transform_error([](di::Error error) {
        return di::format_error("Failed to detect terminal features: {}"_sv, error);
    }));
    auto theme = TRY(config_json::v1::resolve_theme(args.name, features.palette));
    auto string = to_json_string_without_empty_objects(theme);
    dius::println("{}"_sv, string);
    return {};
}
}
