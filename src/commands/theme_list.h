#pragma once

#include "di/cli/prelude.h"
#include "theme.h"

namespace ttx {
struct ThemeList {
    ThemeSource source { ThemeSource::All };
    bool help { false };

    constexpr static auto get_cli_parser() {
        return di::cli_parser<ThemeList>("list"_tsv, "List the names of the available themes"_sv)
            .argument<&ThemeList::source>("SOURCE"_sv, "Show themes specific to this source"_sv)
            .help();
    }
};

auto main(ThemeList& args) -> di::Result<>;
}
