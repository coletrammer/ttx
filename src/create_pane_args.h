#pragma once

#include "di/container/path/prelude.h"
#include "di/container/string/prelude.h"
#include "di/reflect/prelude.h"
#include "ttx/terminal/palette.h"

namespace ttx {
struct CreatePaneArgs {
    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<CreatePaneArgs>) {
        return di::make_fields<"CreatePaneArgs">(
            di::field<"command", &CreatePaneArgs::command>,
            di::field<"capture_command_output_path", &CreatePaneArgs::capture_command_output_path>,
            di::field<"replay_path", &CreatePaneArgs::replay_path>,
            di::field<"save_state_path", &CreatePaneArgs::save_state_path>,
            di::field<"pipe_input", &CreatePaneArgs::pipe_input>, di::field<"cwd", &CreatePaneArgs::cwd>,
            di::field<"terminfo_dir", &CreatePaneArgs::terminfo_dir>, di::field<"term", &CreatePaneArgs::term>,
            di::field<"global_palette", &CreatePaneArgs::global_palette>,
            di::field<"local_palette", &CreatePaneArgs::local_palette>,
            di::field<"theme_mode", &CreatePaneArgs::theme_mode>,
            di::field<"pipe_output", &CreatePaneArgs::pipe_output>,
            di::field<"pipe_extra_output", &CreatePaneArgs::pipe_extra_output>);
    }

    auto with_cwd(di::Optional<di::Path> cwd) const& -> CreatePaneArgs {
        auto result = di::clone(*this);
        result.cwd = di::move(cwd);
        return result;
    }

    di::Vector<di::TransparentString> command {};
    di::Optional<di::Path> capture_command_output_path {};
    di::Optional<di::Path> replay_path {};
    di::Optional<di::Path> save_state_path {};
    di::Optional<di::String> pipe_input {};
    di::Optional<di::Path> cwd {};
    di::Optional<di::Path> terminfo_dir {};
    di::TransparentString term { "xterm-ttx"_ts };
    terminal::Palette global_palette {};
    terminal::Palette local_palette {};
    terminal::ThemeMode theme_mode { terminal::ThemeMode::Dark };
    bool pipe_output { false };
    bool pipe_extra_output { false }; ///< Create a pipe on fd 3 and read from it
};
}
