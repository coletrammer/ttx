#include "theme_apply_local_palette.h"

#include "config_json.h"
#include "di/vocab/error/string_error.h"
#include "dius/print.h"
#include "ttx/features.h"
#include "ttx/terminal/escapes/osc_21.h"

namespace ttx {
auto main(ThemeApplyLocalPalette& args) -> di::Result<> {
    auto features = TRY(detect_features(dius::std_in).transform_error([](di::Error error) {
        return di::format_error("Failed to detect terminal features: {}"_sv, error);
    }));
    auto theme_json = TRY(config_json::v1::resolve_theme(args.name, features.palette));
    auto theme = config_json::v1::convert_to_config(di::move(theme_json));
    if (!(features.features & Feature::DynamicPalette) && !(features.features & Feature::DynamicPaletteKitty)) {
        return di::Unexpected(di::format_error("Terminal does not support setting the local color palette"_sv));
    }
    auto string = ""_s;
    for (auto index_number : di::range(u32(terminal::PaletteIndex::Count))) {
        auto const index = terminal::PaletteIndex(index_number);
        auto osc21 = terminal::OSC21();
        osc21.requests.push_back(terminal::OSC21::Request {
            .palette = index,
            .color = theme.colors.get(index),
        });
        string += osc21.serialize(features.features);
    }
    dius::print("{}"_sv, string);
    (void) dius::std_out.flush();
    return {};
}
}
