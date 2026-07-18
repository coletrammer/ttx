#pragma once

#include "commands/attach.h"
#include "commands/completions.h"
#include "commands/config_command.h"
#include "commands/features.h"
#include "commands/keybinds.h"
#include "commands/new.h"
#include "commands/replay.h"
#include "commands/server_command.h"
#include "commands/terminfo.h"
#include "commands/theme_command.h"
#include "di/cli/prelude.h"

namespace ttx {
struct Args {
    di::Variant<New, Attach, ConfigCommand, ThemeCommand, Completions, Replay, Keybinds, Features, Terminfo,
                ServerCommand>
        subcommand;
    bool help { false };

    constexpr static auto get_cli_parser() {
        return di::cli_parser<Args>("ttx"_tsv, "Terminal multiplexer"_sv).subcommands<&Args::subcommand>().help();
    }
};

auto main(Args& args) -> di::Result<>;
}
