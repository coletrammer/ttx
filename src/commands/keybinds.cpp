#include "keybinds.h"

#include "config_json.h"
#include "di/vocab/error/string_error.h"
#include "dius/print.h"
#include "key_bind.h"

namespace ttx {
auto main(Keybinds& args) -> di::Result<> {
    auto config = TRY(config_json::v1::resolve_profile(args.profile, {}, {}).transform_error([&](auto&& error) {
        return di::format_error("Failed to resolve profile '{}': {}"_sv, args.profile, error);
    }));
    // TODO: key binds
    // auto key_binds = make_key_binds(config.input, args.replay_mode);
    // for (auto const& bind : key_binds) {
    //     dius::println("{}"_sv, bind);
    // }
    return {};
}
}
