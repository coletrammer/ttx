#include "theme_detect_dark_light_mode.h"

#include "di/vocab/error/string_error.h"
#include "dius/print.h"
#include "ttx/features.h"

namespace ttx {
auto main(ThemeDetectDarkLightMode&) -> di::Result<> {
    auto features = TRY(detect_features(dius::std_in).transform_error([](di::Error error) {
        return di::format_error("Failed to detect terminal features: {}"_sv, error);
    }));
    if (!(features.features & Feature::ThemeDetection)) {
        return di::Unexpected(di::format_error("Terminal does not support theme detection"_sv));
    }
    dius::println("{}"_sv, features.theme_mode);
    return {};
}
}
