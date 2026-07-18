#pragma once

#include "di/cli/prelude.h"
#include "ttx/features.h"

namespace ttx {
struct Features {
    bool help { false };
    bool only_show_supported { false };

    constexpr static auto get_cli_parser() {
        return di::cli_parser<Features>("features"_tsv, "List features detected in the current terminal"_sv)
            .option<&Features::only_show_supported>({}, "only-show-supported"_tsv,
                                                    "Only log information feature which are detected"_sv)
            .help();
    }
};

auto main(Features& args) -> di::Result<>;
}
