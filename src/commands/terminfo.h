#pragma once

#include "di/cli/prelude.h"
#include "di/reflect/prelude.h"

namespace ttx {
enum class TerminfoFormat {
    Terminfo,
    Verbose,
};

constexpr static auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<TerminfoFormat>) {
    using enum TerminfoFormat;
    return di::make_enumerators<"TerminfoFormat">(
        di::enumerator<"terminfo", Terminfo, "Official terminfo format used by tools like tic">,
        di::enumerator<"verbose", Verbose,
                       "Verbose format with descriptions to understand the details of ttx's terminfo">);
}

struct Terminfo {
    TerminfoFormat format { TerminfoFormat::Terminfo };
    bool help { false };

    constexpr static auto get_cli_parser() {
        return di::cli_parser<Terminfo>("terminfo"_tsv, "Display the terminfo for ttx"_sv)
            .option<&Terminfo::format>('f', "format"_tsv, "Display format for the terminfo"_sv)
            .help();
    }
};

auto main(Terminfo& args) -> di::Result<>;
}
