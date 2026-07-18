#pragma once

#include "config_docs.h"
#include "config_nix.h"
#include "config_schema.h"
#include "config_show.h"
#include "di/cli/prelude.h"

namespace ttx {
struct ConfigCommand {
    di::Variant<di::Void, ConfigShow, ConfigDocs, ConfigNix, ConfigSchema> subcommand;
    bool help { false };

    constexpr static auto get_cli_parser() {
        return di::cli_parser<ConfigCommand>("config"_tsv, "Show information about ttx's configuration"_sv)
            .subcommands<&ConfigCommand::subcommand>()
            .help();
    }
};

auto main(ConfigCommand& args) -> di::Result<>;
}
