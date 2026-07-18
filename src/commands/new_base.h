#pragma once

#include "di/container/path/prelude.h"
#include "di/vocab/optional/prelude.h"
#include "ttx/clipboard.h"
#include "ttx/key.h"

namespace ttx {
struct NewBase {
    di::Optional<Key> prefix;
    bool hide_status_bar { false };
    bool headless { false };
    di::Optional<di::PathView> save_state_path;
    di::Optional<di::PathView> capture_command_output_path;

    // New
    bool disable_layout_restore { false };
    bool disable_layout_save { false };
    di::Optional<di::TransparentStringView> layout_name;
    di::Optional<di::TransparentStringView> term;
    di::Optional<ClipboardMode> clipboard_mode;
    bool force_local_terminfo { false };
    di::Vector<di::TransparentStringView> command;
    di::TransparentStringView profile { "main"_tsv };
    di::Optional<di::TransparentStringView> theme;

    // Replay
    di::Vector<di::PathView> replay_paths;
};

auto main(NewBase& args) -> di::Result<>;
}
