#include "attach.h"

#include "client.h"
#include "config_json.h"
#include "di/container/string/conversion.h"
#include "di/vocab/error/string_error.h"
#include "ttx/features.h"

namespace ttx {
auto main(Attach& args) -> di::Result<> {
    auto const replay_mode = !args.replay_paths.empty();
    if (args.headless || replay_mode) {
        args.profile = ""_tsv;
    }

    if (args.profile.ends_with('/')) {
        return di::Unexpected(di::format_error("--profile cannot be a directory"_sv));
    }
    auto features = FeatureResult { .features = Feature::All };
    if (!args.headless) {
        features = TRY(detect_features(dius::std_in).transform_error([](di::Error error) {
            return di::format_error("Failed to detect terminal features: {}"_sv, error);
        }));
    }
    auto config_from_args = config_json::v1::Config {
        .theme = {
            .name = args.theme.transform(di::to_utf8_string_lossy),
        },
        .input = {
            .prefix = args.prefix,
            .save_state_path = args.save_state_path.transform([](di::PathView path) { return di::to_utf8_string_lossy(path.data()); }),
        },
        .clipboard = {
            .mode = args.clipboard_mode,
        },
        .session = {
            .restore_layout = args.disable_layout_restore ? di::Optional(false) : di::nullopt,
            .save_layout = args.disable_layout_save ? di::Optional(false) : di::nullopt,
            .layout_name = args.layout_name.transform(di::to_utf8_string_lossy),
        },
        .shell = {
            .command = !args.command.empty () ? di::Optional(args.command | di::transform(di::to_utf8_string_lossy) | di::to<di::Vector>()) : di::nullopt,
        },
        .status_bar = {
            .hide = args.hide_status_bar,
        },
        .terminfo = {
            .term = args.term.transform(di::to_utf8_string_lossy),
            .force_local_terminfo = args.force_local_terminfo ? di::Optional(true) : di::nullopt,
        },
    };
    auto config = TRY(config_json::v1::resolve_profile(args.profile, features.theme_mode, features.palette,
                                                       di::clone(config_from_args))
                          .transform_error([&](auto&& error) {
                              return di::format_error("Failed to resolve profile '{}': {}"_sv, args.profile, error);
                          }));

    // Setup - log to file.
    [[maybe_unused]] auto& log = dius::std_err = TRY(dius::open_sync("/tmp/ttx.log"_pv, dius::OpenMode::WriteClobber));

    return run_client(di::move(config), features.features);
}
}
