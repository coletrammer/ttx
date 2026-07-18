#include "theme_show_local_palette.h"

#include "config_json.h"
#include "di/vocab/error/string_error.h"
#include "dius/print.h"
#include "ttx/features.h"

namespace ttx {
auto main(ThemeShowLocalPalette&) -> di::Result<> {
    auto features = TRY(detect_features(dius::std_in).transform_error([](di::Error error) {
        return di::format_error("Failed to detect terminal features: {}"_sv, error);
    }));
    if (!(features.features & Feature::DynamicPalette)) {
        return di::Unexpected(di::format_error("Terminal does not support getting the local color palette"_sv));
    }
    auto theme = config_json::v1::config_from_palette(features.palette);
    auto string = to_json_string_without_empty_objects(theme);
    dius::println("{}"_sv, string);
    return {};
}
}
