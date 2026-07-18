#pragma once

#include "di/cli/prelude.h"
#include "new_base.h"

namespace ttx {
struct Replay : NewBase {
    bool help { false };

    constexpr static auto get_cli_parser() {
        return di::cli_parser<Replay>("replay"_tsv, "Start a Replay ttx instance"_sv)
            .option<&NewBase::prefix>('p', "prefix"_tsv, "Prefix key for key bindings"_sv, false, "KEY"_sv)
            .option<&NewBase::capture_command_output_path>('c', "capture-command-output-path"_tsv,
                                                           "Capture command output to a file"_sv)
            .option<&NewBase::save_state_path>('S', "save-state-path"_tsv,
                                               "Save state path when triggering saving a pane's state"_sv)
            .option<&NewBase::headless>('h', "headless"_tsv, "Headless mode"_sv)
            .argument<&NewBase::replay_paths>("REPLAY_FILES"_sv, "Files to replay (each file gets its own pane)"_sv,
                                              true)
            .help();
    }
};

auto main(Replay& args) -> di::Result<>;
}
