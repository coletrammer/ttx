#pragma once

#include "di/cli/prelude.h"

namespace ttx {
struct Completions {
    di::cli::Shell shell { di::cli::Shell::Bash };
    bool help { false };

    constexpr static auto get_cli_parser() {
        return di::cli_parser<Completions>("completions"_tsv, "Write shell completions for ttx to stdout"_sv)
            .argument<&Completions::shell>("SHELL"_sv, "Shell to generate completions for"_sv, true)
            .help();
    }
};

auto main(Completions& args) -> di::Result<>;
}
