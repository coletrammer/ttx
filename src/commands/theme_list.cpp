#include "theme_list.h"

#include "config_json.h"
#include "di/vocab/error/string_error.h"
#include "dius/print.h"
#include "ttx/features.h"

namespace ttx {
auto main(ThemeList& args) -> di::Result<> {
    auto features = TRY(detect_features(dius::std_in).transform_error([](di::Error error) {
        return di::format_error("Failed to detect terminal features: {}"_sv, error);
    }));
    auto themes = TRY(config_json::v1::list_themes(args.source, features.palette));

    dius::println("{: <50}{: <10}{: <20}"_sv, di::Styled("Name"_sv, di::FormatEffect::Bold),
                  di::Styled("Mode"_sv, di::FormatEffect::Bold), di::Styled("Source"_sv, di::FormatEffect::Bold));
    dius::println("{:=<80}"_sv, ""_sv);
    for (auto const& [name, source, mode, _] : themes) {
        dius::println("{: <50}{: <10}{: <20}"_sv, di::Styled(name, di::FormatEffect::Bold), di::to_string(mode),
                      di::to_string(source));
    }
    return {};
}
}
