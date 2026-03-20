#pragma once

#include "config.h"
#include "di/container/path/prelude.h"
#include "di/container/vector/prelude.h"
#include "di/reflect/prelude.h"
#include "di/serialization/json_value.h"
#include "di/util/construct.h"
#include "di/vocab/optional/prelude.h"
#include "ttx/key.h"

namespace ttx::config_json::v1 {
struct Input {
    di::Optional<Key> prefix {};
    di::Optional<bool> disable_default_keybinds {};
    di::Optional<di::String> save_state_path {};

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<Input>) {
        return di::make_fields<"Input", "Configuration relating the input processing of ttx (primarily key bindings)">(
            di::field<"prefix", &Input::prefix,
                      "Prefix key to use. Almost all key bindings require using the prefix key to enter 'NORMAL' mode "
                      "first. Currently, the prefix key must be pressed in conjunction with the control key. So "
                      "specifying a prefix of 'A' means that to enter 'NORMAL' mode you must press 'ctrl+a'">,
            di::field<"disable_default_keybinds", &Input::disable_default_keybinds,
                      "Disable all default key bindings to allow full customization">,
            di::field<"save_state_path", &Input::save_state_path,
                      "Path to write save state files captured by ttx. These save state files can be replayed using "
                      "`ttx replay`. This functionality is useful for testing and reproducing bugs">);
    }
};

struct Layout {
    di::Optional<bool> hide_status_bar {};

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<Layout>) {
        return di::make_fields<
            "Layout",
            "Configuration relating to the layout of panes in ttx, including the status bar and pane spacing">(
            di::field<"hide_status_bar", &Layout::hide_status_bar,
                      "Hide the status bar for an extremely minimal layout">);
    }
};

struct Clipboard {
    di::Optional<ClipboardMode> mode {};

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<Clipboard>) {
        return di::make_fields<"Clipboard", "Configuration relating to the clipboard handling of ttx (OSC 52)">(
            di::field<"mode", &Clipboard::mode,
                      "Configure the way ttx interacts with the system clipboard when inner applications use OSC 52. "
                      "By default ttx will read/write from the system clipboard. This requires configuring your "
                      "terminal emulator to allow interacting with the system clipboard. If you do not want "
                      "applications to access your system clipboard, specify the mode as 'local'">);
    }
};

struct Session {
    di::Optional<bool> restore_layout {};
    di::Optional<bool> save_layout {};
    di::Optional<di::String> layout_name {};

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<Session>) {
        return di::make_fields<
            "Session", "Configuration relating the session management, such as automatically saving and restoring the "
                       "current layout">(
            di::field<"restore_layout", &Session::restore_layout,
                      "Automatically restore the previous layout on startup. This does not preserve commands but does "
                      "restore all sessions, tabs, and panes as well as their working directories">,
            di::field<"save_layout", &Session::save_layout,
                      "Automaticaly save the current layout to a file whenever updates occur. The layout is saved to "
                      "$XDG_DATA_HOME/ttx/layouts (~/.local/share/ttx/layouts). The layout file is meant for use the "
                      "`restore_layout` option">,
            di::field<"layout_name", &Session::layout_name,
                      "Layout name to use for save/restore. This defaults the name of the chosen profile">);
    }
};

struct Shell {
    di::Optional<di::Vector<di::String>> command {};

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<Shell>) {
        return di::make_fields<"Shell", "Configuration relating to the shell ttx starts in each pane">(
            di::field<"command", &Shell::command,
                      "The command to run inside is pane. This is a list of arguments passed directly to the OS. If "
                      "shell expansion is desired, use a command like this: [\"sh\", \"-c\", \"<COMMAND>\"]">);
    }
};

struct Terminfo {
    di::Optional<di::String> term {};
    di::Optional<bool> force_local_terminfo {};

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<Terminfo>) {
        return di::make_fields<"Terminfo", "Configuration relating to the terminfo ttx passes to inner applications">(
            di::field<"term", &Terminfo::term,
                      "Value of the TERM enviornment variable to pass to inner applications. ttx automaticaly sets up "
                      "the terminfo so the default value of 'xterm-ttx' will work locally. However, ttx's terminfo is "
                      "not automaticaly available when using ssh or sudo. If this is causing issues, set this option "
                      "to 'xterm-256color'">,
            di::field<
                "force_local_terminfo", &Terminfo::force_local_terminfo,
                "Compile ttx's terminfo on startup and set TERMINFO_DIR, even if the terminfo is already installed. "
                "This shouldn't be necessary but can be used if the system's terminfo is broken or for testing">);
    }
};

struct Config {
    di::String schema { "https://github.com/coletrammer/ttx/raw/refs/heads/main/meta/schema/config.json"_s };
    u32 version { 1 };
    di::Optional<di::Vector<di::String>> extends {};
    Input input {};
    Layout layout {};
    Clipboard clipboard {};
    Session session {};
    Shell shell {};
    Terminfo terminfo {};

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<Config>) {
        return di::make_fields<"Config", "JSON configuration for ttx">(
            di::field<"$schema", &Config::schema>,
            di::field<"version", &Config::version, "Version of the ttx config (should always be 1)">,
            di::field<"extends", &Config::extends,
                      "List of configuration files to extend. These can recursively extend more files. Priority is "
                      "given to the last time the configuration option is specified">,
            di::field<"input", &Config::input,
                      "Configuration relating the input processing of ttx (primarily key bindings)">,
            di::field<
                "layout", &Config::layout,
                "Configuration relating to the layout of panes in ttx, including the status bar and pane spacing">,
            di::field<"clipboard", &Config::clipboard,
                      "Configuration relating to the clipboard handling of ttx (OSC 52)">,
            di::field<"session", &Config::session,
                      "Configuration relating the session management, such as automatically saving and restoring the "
                      "current layout">,
            di::field<"shell", &Config::shell, "Configuration relating to the shell ttx starts in each pane">,
            di::field<"terminfo", &Config::terminfo,
                      "Configuration relating to the terminfo ttx passes to inner applications">);
    }
};

auto resolve_profile(di::TransparentStringView profile, Config&& cli_config = {}) -> di::Result<ttx::Config>;
auto to_config_json(ttx::Config&& config) -> Config;
auto json_schema() -> di::json::Object;
auto nix_options() -> di::String;
}
