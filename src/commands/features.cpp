#include "features.h"

#include "di/vocab/error/string_error.h"
#include "dius/print.h"
#include "ttx/features.h"

namespace ttx {
auto main(Features& args) -> di::Result<> {
    auto features = TRY(detect_features(dius::std_in).transform_error([](di::Error error) {
                        return di::format_error("Failed to detect terminal features: {}"_sv, error);
                    })).features;

    dius::println("{: <30}{: <20}{: <90}"_sv, di::Styled("Feature"_sv, di::FormatEffect::Bold),
                  di::Styled("Is Supported"_sv, di::FormatEffect::Bold),
                  di::Styled("Description"_sv, di::FormatEffect::Bold));
    dius::println("{:=<150}"_sv, ""_sv);
    di::tuple_for_each(
        [&]<typename E>(E) {
            if (E::value == Feature::None) {
                return;
            }

            auto name = di::container::fixed_string_to_utf8_string_view<E::name>();
            auto description = di::container::fixed_string_to_utf8_string_view<E::description>();
            auto supported = !!(features & E::value);
            if (supported || !args.only_show_supported) {
                dius::println("{: <30}{: <20}{: <90}"_sv, di::Styled(name, di::FormatEffect::Bold),
                              supported ? di::Styled("supported"_sv, di::FormatColor::Green)
                                        : di::Styled("unsupported"_sv, di::FormatColor::Red),
                              description);
            }
        },
        di::reflect(features));
    return {};
}
}
