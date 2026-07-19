#pragma once

#include "di/cli/prelude.h"
#include "run_client_base.h"

namespace ttx {
struct Replay : RunClientBase {
    bool help { false };

    constexpr static auto get_cli_parser() {
        return di::cli_parser<Replay>("replay"_tsv, "Start a Replay ttx instance"_sv)
            .option<&RunClientBase::prefix>('p', "prefix"_tsv, "Prefix key for key bindings"_sv, false, "KEY"_sv)
            .option<&RunClientBase::capture_command_output_path>('c', "capture-command-output-path"_tsv,
                                                                 "Capture command output to a file"_sv)
            .option<&RunClientBase::save_state_path>('S', "save-state-path"_tsv,
                                                     "Save state path when triggering saving a pane's state"_sv)
            .option<&RunClientBase::headless>('h', "headless"_tsv, "Headless mode"_sv)
            .argument<&RunClientBase::replay_paths>("REPLAY_FILES"_sv,
                                                    "Files to replay (each file gets its own pane)"_sv, true)
            .help();
    }
};

auto main(Replay& args) -> di::Result<>;
}
