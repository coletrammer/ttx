#pragma once

#include "di/cli/prelude.h"
#include "new_base.h"

namespace ttx {
struct New : RunClientBase {
    bool help { false };

    constexpr static auto get_cli_parser() {
        return di::cli_parser<New>("new"_tsv, "Start a new ttx instance"_sv)
            .option<&RunClientBase::prefix>({}, "prefix"_tsv, "Override config prefix key for key bindings"_sv, false,
                                            "KEY"_sv)
            .option<&RunClientBase::disable_layout_restore>({}, "disable-layout-restore"_tsv,
                                                            "Disable restoring from saved layout"_sv)
            .option<&RunClientBase::disable_layout_save>({}, "disable-layout-restore"_tsv,
                                                         "Disable continuously saving the current layout"_sv)
            .option<&RunClientBase::layout_name>({}, "layout-name"_tsv,
                                                 "Layout name for save/restore (default: profile name)"_sv)
            .option<&RunClientBase::profile>(
                'p', "profile"_tsv, "Profile name to use for this session (empty string loads no configuration)"_sv)
            .option<&RunClientBase::theme>({}, "theme"_tsv, "Theme to use (overrides configuration if specified)"_sv)
            .option<&RunClientBase::hide_status_bar>('s', "hide-status-bar"_tsv, "Hide the status bar"_sv)
            .option<&RunClientBase::capture_command_output_path>('c', "capture-command-output-path"_tsv,
                                                                 "Capture command output to a file"_sv)
            .option<&RunClientBase::save_state_path>('S', "save-state-path"_tsv,
                                                     "Save state path when triggering saving a pane's state"_sv)
            .option<&RunClientBase::headless>('h', "headless"_tsv, "Headless mode"_sv)
            .option<&RunClientBase::term>('t', "term"_tsv, "Override config TERM environment variable"_sv)
            .option<&RunClientBase::clipboard_mode>({}, "clipboard"_tsv, "Set the clipboard mode"_sv, false, "MODE"_sv)
            .option<&RunClientBase::force_local_terminfo>(
                {}, "force-local-terminfo"_tsv,
                "Always try and compile built-in terminfo, and set TERMINFO env variable"_sv)
            .argument<&RunClientBase::command>("COMMAND"_sv, "Program to run in terminal (default: $SHELL)"_sv, false,
                                               di::cli::ValueType::CommandWithArgs)
            .help();
    }
};

auto main(New& args) -> di::Result<>;
}
