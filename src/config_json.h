#pragma once

#include "config.h"
#include "di/container/path/prelude.h"
#include "di/container/vector/prelude.h"
#include "di/reflect/prelude.h"
#include "di/serialization/json_value.h"
#include "di/util/construct.h"
#include "di/vocab/optional/prelude.h"
#include "theme.h"
#include "ttx/key.h"
#include "ttx/terminal/color.h"
#include "ttx/terminal/palette.h"

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

struct Colors {
    di::Optional<di::Vector<terminal::Color>> palette {};
    di::Optional<terminal::Color> foreground {};
    di::Optional<terminal::Color> background {};
    di::Optional<terminal::Color> selection_background {};
    di::Optional<terminal::Color> selection_foreground {};
    di::Optional<terminal::Color> cursor {};
    di::Optional<terminal::Color> cursor_text {};

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<Colors>) {
        return di::make_fields<
            "Colors", "The colors used for by terminals running inside ttx. This includes the default values for the "
                      "color palette, cursor colors, and selection colors. When not specified ttx will use the default "
                      "colors used by the outer terminal">(
            di::field<"palette", &Colors::palette,
                      "The terminal palette colors to use as an array. Up to 256 "
                      "colors can be specified but typically only the "
                      "first 16 are, which correspond the typical ANSI colors">,
            di::field<"foreground", &Colors::foreground, "The default foreground color to use in the terminal">,
            di::field<"background", &Colors::background, "The default background color to use in the terminal">,
            di::field<"selection_background", &Colors::selection_background,
                      "The background color to use when highlighting selection text. A value of dynamic will cause the "
                      "foreground and background to be swapped for selected cells">,
            di::field<"selection_foreground", &Colors::selection_foreground,
                      "The foreground color to use when highlighting selection text. A value of dynamic will cause the "
                      "foreground and foreground to be swapped for selected cells">,
            di::field<"cursor", &Colors::cursor,
                      "The default color to use when displaying the cursor. A value of dynamic indicates the default "
                      "foreground color will be used">,
            di::field<"cursor_text", &Colors::cursor_text,
                      "The default color to use when displaying text under the cursor. A value of dynamic indicates "
                      "the default background color will be used">);
    }
};

struct StatusBarColors {
    di::Optional<terminal::Color> badge_text_color {};
    di::Optional<terminal::Color> label_text_color {};
    di::Optional<terminal::Color> background_color {};
    di::Optional<terminal::Color> label_background_color {};

    di::Optional<terminal::Color> active_tab_badge_color {};
    di::Optional<terminal::Color> inactive_tab_badge_color {};
    di::Optional<terminal::Color> session_badge_background_color {};
    di::Optional<terminal::Color> host_badge_background_color {};

    di::Optional<terminal::Color> switch_mode_color {};
    di::Optional<terminal::Color> insert_mode_color {};
    di::Optional<terminal::Color> normal_mode_color {};
    di::Optional<terminal::Color> resize_mode_color {};

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<StatusBarColors>) {
        return di::make_fields<"StatusBarColors", "Configuration for the ttx status bar colors">(
            di::field<"badge_text_color", &StatusBarColors::badge_text_color,
                      "The text color to use for colored badges (the left part of status bar components). A value of "
                      "dynamic means use the default background color as the text color">,
            di::field<"label_text_color", &StatusBarColors::label_text_color,
                      "The text color to use for the text label of tabs">,
            di::field<"background_color", &StatusBarColors::background_color,
                      "The default background color to use for the status bar">,
            di::field<"label_background_color", &StatusBarColors::label_background_color,
                      "The background color to use for tab labels">,
            di::field<"active_tab_badge_color", &StatusBarColors::active_tab_badge_color,
                      "The color to use to signify a tab is active">,
            di::field<"inactive_tab_badge_color", &StatusBarColors::inactive_tab_badge_color,
                      "The color to use to signify a tab is inactive">,
            di::field<"session_badge_background_color", &StatusBarColors::session_badge_background_color,
                      "The background color to use for the session name badge">,
            di::field<"host_badge_background_color", &StatusBarColors::host_badge_background_color,
                      "The background color to use for the host name badge">,
            di::field<"switch_mode_color", &StatusBarColors::switch_mode_color,
                      "The color to use to signify you are in SWITCH mode">,
            di::field<"insert_mode_color", &StatusBarColors::insert_mode_color,
                      "The color to use to signify you are in INSERT mode">,
            di::field<"normal_mode_color", &StatusBarColors::normal_mode_color,
                      "The color to use to signify you are in NORMAL mode">,
            di::field<"resize_mode_color", &StatusBarColors::resize_mode_color,
                      "The color to use to signify you are in RESIZE mode">);
    }
};

struct StatusBar {
    di::Optional<bool> hide {};
    StatusBarColors colors {};

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<StatusBar>) {
        return di::make_fields<"StatusBar", "Configuration for the ttx status bar, including colors and layout">(
            di::field<"hide", &StatusBar::hide,
                      "Hide the status bar. This is useful for minimal layouts or for testing">,
            di::field<"colors", &StatusBar::colors, "Configure the colors used by the status bar">);
    }
};

struct FzfColors {
    di::Optional<terminal::Color> fg {};
    di::Optional<terminal::Color> fgplus {};
    di::Optional<terminal::Color> bg {};
    di::Optional<terminal::Color> bgplus {};
    di::Optional<terminal::Color> border {};
    di::Optional<terminal::Color> header {};
    di::Optional<terminal::Color> hl {};
    di::Optional<terminal::Color> hlplus {};
    di::Optional<terminal::Color> info {};
    di::Optional<terminal::Color> label {};
    di::Optional<terminal::Color> marker {};
    di::Optional<terminal::Color> pointer {};
    di::Optional<terminal::Color> preview_border {};
    di::Optional<terminal::Color> prompt {};
    di::Optional<terminal::Color> selected_bg {};
    di::Optional<terminal::Color> separator {};
    di::Optional<terminal::Color> spinner {};

    auto operator==(FzfColors const&) const -> bool = default;

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<FzfColors>) {
        return di::make_fields<
            "FzfColors",
            "theme for the fzf popups used in the ttx UI. The names of the fields match directly the "
            "cooresponding fzf color name, documented [here](https://man.archlinux.org/man/fzf.1.en#GLOBAL_STYLE), "
            "although only a subset of keys are available">(
            di::field<"fg", &FzfColors::fg, "Text color of unselected entries">,
            di::field<"fg+", &FzfColors::fgplus, "Text color of selected entries">,
            di::field<"bg", &FzfColors::bg, "Background color of unselected entries">,
            di::field<"bg+", &FzfColors::bgplus, "Background color of selected entries">,
            di::field<"border", &FzfColors::border, "Outer border color">,
            di::field<"header", &FzfColors::header, "Text color of the fzf header">,
            di::field<"hl", &FzfColors::hl, "Text color to show the matched parts of an entry">,
            di::field<"hl+", &FzfColors::hlplus, "Text color to show the matched parts of selected entries">,
            di::field<"info", &FzfColors::info, "Text color for info (located on the right of the prompt line)">,
            di::field<"label", &FzfColors::label, "Text color for text placed in an fzf border">,
            di::field<"marker", &FzfColors::marker, "Text color of multi-select indicator">,
            di::field<"pointer", &FzfColors::pointer, "Text color of current entry indicator">,
            di::field<"preview-border", &FzfColors::preview_border, "Border color for the preview window">,
            di::field<"prompt", &FzfColors::prompt, "Text color of the query prompt">,
            di::field<"selected-bg", &FzfColors::selected_bg, "Background color of selected entries">,
            di::field<"separator", &FzfColors::separator, "Color of line separator below the query text">,
            di::field<"spinner", &FzfColors::spinner, "Color of the loading spinner">);
    }
};

struct Fzf {
    FzfColors colors {};

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<Fzf>) {
        return di::make_fields<"Fzf", "Configure the fzf popups used by the ttx UI">(
            di::field<"colors", &Fzf::colors, "Configure the theme used by fzf popups">);
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
                      "shell expansion is desired, use a command like this: [\"sh\", \"-c\", \"<COMMAND>\"]. By "
                      "default '$SHELL' is started with no additional arguments">);
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

struct Theme {
    di::Optional<di::String> name {};
    di::Optional<di::String> dark {};
    di::Optional<di::String> light {};

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<Theme>) {
        return di::make_fields<"Theme",
                               "Configure the theme ttx will use. You can specify dark/light preference aware "
                               "themes by specifying both the 'dark' and 'light' fields instead of the 'name' field">(
            di::field<
                "name", &Theme::name,
                "The named theme to use. When not set, ttx will try to auto-detect your terminal's color "
                "scheme against the list available themes. When auto detection fails, the standard 16 ANSI colors "
                "will be used, with the theme name set to 'ansi'. You can disable theme detection by explicitly "
                "setting the theme to a specific theme or to 'ansi'. When providing a named theme, "
                "ttx first searches the directory `$XDG_CONFIG_HOME/ttx/themes` for a JSON configuration file "
                "matching the name. The file's name should be the theme's name followed by `.json`. Custom "
                "themes use the same JSON schema as normal configuration files, and can include any property (but "
                "cannot extend other files). When no custom theme is found, ttx searches its set of built-in themes. "
                "The built-in themes are taken from the [iTerm2 Color Schemes "
                "repository](https://github.com/mbadolato/iTerm2-Color-Schemes). Configuration defined in a theme "
                "has lower precedence than any setting defined in a configuration file. If you want settings to be "
                "modifable by the theme you select you cannot specify the option your main configuration file">,
            di::field<"dark", &Theme::dark,
                      "Theme to use when the color preference is 'dark'. When not specified the named theme will "
                      "be used">,
            di::field<
                "light", &Theme::light,
                "Theme to use when the color preference is 'light'. When not specified the named theme will be used">);
    }
};

struct Config {
    di::String schema { "https://github.com/coletrammer/ttx/raw/refs/heads/main/meta/schema/config.json"_s };
    u32 version { 1 };
    di::Optional<di::Vector<di::String>> extends {};
    Theme theme {};
    Input input {};
    Colors colors {};
    Clipboard clipboard {};
    Session session {};
    Shell shell {};
    Fzf fzf {};
    StatusBar status_bar {};
    Terminfo terminfo {};

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<Config>) {
        return di::make_fields<"Config", "Top-level JSON configuration for ttx">(
            di::field<"$schema", &Config::schema>,
            di::field<"version", &Config::version, "Version of the ttx config (should always be 1)">,
            di::field<"extends", &Config::extends,
                      "List of configuration files to extend. These can recursively extend more files. Priority is "
                      "given to the last time the configuration option is specified">,
            di::field<"theme", &Config::theme, "Configure the theme used by ttx">,
            di::field<"input", &Config::input,
                      "Configuration relating the input processing of ttx (primarily key bindings)">,
            di::field<"colors", &Config::colors, "Terminal colors to use (main color palette)">,
            di::field<"clipboard", &Config::clipboard,
                      "Configuration relating to the clipboard handling of ttx (OSC 52)">,
            di::field<"session", &Config::session,
                      "Configuration relating the session management, such as automatically saving and restoring the "
                      "current layout">,
            di::field<"shell", &Config::shell, "Configuration relating to the shell ttx starts in each pane">,
            di::field<"fzf", &Config::fzf, "Configuration for ttx built-in fzf popups">,
            di::field<"status_bar", &Config::status_bar, "Configuration for the status bar">,
            di::field<"terminfo", &Config::terminfo,
                      "Configuration relating to the terminfo ttx passes to inner applications">);
    }
};

auto list_custom_themes() -> di::Result<di::Vector<ListedTheme>>;
auto resolve_custom_theme(di::TransparentStringView name) -> di::Result<Config>;
auto resolve_profile_to_json(di::TransparentStringView profile, terminal::ThemeMode theme_preference,
                             terminal::Palette const& palette, Config&& cli_config = {}, bool resolve_theme = true)
    -> di::Result<Config>;
auto convert_to_config(Config&& config) -> ttx::Config;
auto resolve_profile(di::TransparentStringView profile, terminal::ThemeMode theme_preference,
                     terminal::Palette const& outer_terminal_palette, Config&& cli_config = {},
                     bool resolve_theme = true) -> di::Result<ttx::Config>;
auto config_with_defaults(Config&& config) -> Config;
auto resolve_theme(di::TransparentStringView name, terminal::Palette const& outer_terminal_palette)
    -> di::Result<config_json::v1::Config>;
auto config_from_palette(terminal::Palette const& palette) -> config_json::v1::Config;
void strip_empty_objects(di::json::Object& object);
auto list_themes(ThemeSource source, terminal::Palette const& outer_terminal_palette)
    -> di::Result<di::Vector<ListedTheme>>;
auto iterm2_themes() -> di::TreeMap<di::TransparentString, config_json::v1::Config> const&;
auto built_in_themes() -> di::TreeMap<di::TransparentString, config_json::v1::Config> const&;
auto json_schema() -> di::json::Object;
auto nix_options() -> di::String;
auto markdown_docs() -> di::String;
}
