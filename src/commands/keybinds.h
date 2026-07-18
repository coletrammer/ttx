#pragma once

#include "di/cli/prelude.h"

namespace ttx {
struct Keybinds {
    di::TransparentStringView profile { "main"_tsv };
    bool replay_mode { false };
    bool help { false };

    constexpr static auto get_cli_parser() {
        return di::cli_parser<Keybinds>("keybinds"_tsv, "List active keybinds for ttx"_sv)
            .option<&Keybinds::profile>('p', "profile"_tsv,
                                        "Profile name to use for this session (empty string loads no configuration)"_sv)
            .option<&Keybinds::profile>({}, "replay-mode"_tsv,
                                        "Show keybinds assuming ttx is replaying an captured input file"_sv)
            .help();
    }
};

auto main(Keybinds& args) -> di::Result<>;
}
