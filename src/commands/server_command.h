#pragma once

#include "commands/server_start.h"
#include "di/cli/prelude.h"

namespace ttx {
struct ServerCommand {
    di::Variant<di::Void, ServerStart> subcommand;
    bool help { false };

    constexpr static auto get_cli_parser() {
        return di::cli_parser<ServerCommand>("server"_tsv, "Manage ttx server instances"_sv)
            .subcommands<&ServerCommand::subcommand>()
            .help();
    }
};

auto main(ServerCommand& args) -> di::Result<>;
}
