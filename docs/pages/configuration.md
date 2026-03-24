# Configuration

ttx configuration is done via JSON configuration files stored in `$XDG_CONFIG_HOME/ttx`.
Each individual configuration file created can be loaded as a "profile" by passing `--profile=$PROFILE` to ttx new.
The default profile is "main" so your first configuration file should be `$XDG_CONFIG_HOME/ttx/main.json`.

To share configuration across multiple profiles use the "extends" field in the configuration file.
This lets you override or combine as many other configuration files as you want into the final loaded configuration.
To see the configuration file after handling "extends", use `ttx config show --profile "$PROFILE"`

The configuration file is fully specified by a JSON schema file.
When defining your configuration, you can include `"$schema": "https://github.com/coletrammer/ttx/raw/refs/heads/main/meta/schema/config.json"`
to get auto-completions in your text editor.

You can also start your configuration from the defaults by running:

```sh
mkdir -p ~/.config/ttx
ttx config show > ~/.config/ttx/main.json
```

## Nix Home Manager

When using home manager, use the settings field to define your configuration files.
The top-level key is the profile name, followed by the exact same keys as the JSON configuration file.
You can also define your configuration in JSON and load it directly into nix. You will still get the benefits of build time validation of the configuration file:

```nix
programs.ttx = {
  enable = true;
  settings = {
    main = builtins.fromJSON (builtins.readFile ./ttx-main.json);
    super = builtins.fromJSON (builtins.readFile ./ttx-super.json);
  };
};
```

## Configuration Format

Each table below corresponds to a JSON object in the ttx configuration. The first object is the top-level
config. Each field has a corresponding type, which for objects will link to the documentation for that JSON
sub-object.

### Config

Top-level JSON configuration for ttx.

| Field      | Type                    | Default | Description                                                                                                                                               |
| ---------- | ----------------------- | ------- | --------------------------------------------------------------------------------------------------------------------------------------------------------- |
| extends    | list of string          | []      | List of configuration files to extend. These can recursively extend more files. Priority is given to the last time the configuration option is specified. |
| theme      | [Theme](#Theme)         | {}      | Configure the theme used by ttx.                                                                                                                          |
| input      | [Input](#Input)         | {}      | Configuration relating the input processing of ttx (primarily key bindings).                                                                              |
| colors     | [Colors](#Colors)       | {}      | Terminal colors to use (main color palette).                                                                                                              |
| clipboard  | [Clipboard](#Clipboard) | {}      | Configuration relating to the clipboard handling of ttx (OSC 52).                                                                                         |
| session    | [Session](#Session)     | {}      | Configuration relating the session management, such as automatically saving and restoring the current layout.                                             |
| shell      | [Shell](#Shell)         | {}      | Configuration relating to the shell ttx starts in each pane.                                                                                              |
| fzf        | [Fzf](#Fzf)             | {}      | Configuration for ttx built-in fzf popups.                                                                                                                |
| status_bar | [StatusBar](#StatusBar) | {}      | Configuration for the status bar.                                                                                                                         |
| terminfo   | [Terminfo](#Terminfo)   | {}      | Configuration relating to the terminfo ttx passes to inner applications.                                                                                  |

### Theme

Configure the theme ttx will use.

| Field | Type   | Default | Description                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 |
| ----- | ------ | ------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| name  | string | "ansi"  | The named theme to use. When not set, ttx will try to auto-detect your terminal's color scheme against the list built-in theme, unless theme auto detection is disabled via config or `--detect-theme=disabled`. When auto detection fails, the standard 16 ANS colors will be used, with the theme name set to 'ansi'. When providing a named theme, ttx first searches the directory `$XDG_CONFIG_HOME/ttx/themes` for a JSON configuration file matching the name. The file's name should be the theme's name followed by `.json`. Custom themes use the same JSON schema as normal configuration files, and can include any property. However, built-in themes only configure colors. When no custom theme is found, ttx searches its set of built-in schemes. The built-in themes are taken from the [tinted terminal repository](https://github.com/tinted-theming/tinted-terminal). Configuration defined in a theme has lower precedence than any setting defined in a configuration file. If you want settings to be modifable by the theme you select you cannot specify the option your main configuration file. |

### Input

Configuration relating the input processing of ttx (primarily key bindings).

| Field                    | Type        | Default                    | Description                                                                                                                                                                                                                                                                   |
| ------------------------ | ----------- | -------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| prefix                   | [Key](#Key) | "B"                        | Prefix key to use. Almost all key bindings require using the prefix key to enter 'NORMAL' mode first. Currently, the prefix key must be pressed in conjunction with the control key. So specifying a prefix of 'A' means that to enter 'NORMAL' mode you must press 'ctrl+a'. |
| disable_default_keybinds | boolean     | false                      | Disable all default key bindings to allow full customization.                                                                                                                                                                                                                 |
| save_state_path          | string      | "/tmp/ttx-save-state.ansi" | Path to write save state files captured by ttx. These save state files can be replayed using `ttx replay`. This functionality is useful for testing and reproducing bugs.                                                                                                     |

### Key

Represents a supported key for specifying key bindings.

| Value        | Description            |
| ------------ | ---------------------- |
| None         | Disables a key binding |
| A            |                        |
| B            |                        |
| C            |                        |
| D            |                        |
| E            |                        |
| F            |                        |
| G            |                        |
| H            |                        |
| I            |                        |
| J            |                        |
| K            |                        |
| L            |                        |
| M            |                        |
| N            |                        |
| O            |                        |
| P            |                        |
| Q            |                        |
| R            |                        |
| S            |                        |
| T            |                        |
| U            |                        |
| V            |                        |
| W            |                        |
| X            |                        |
| Y            |                        |
| Z            |                        |
| 0            |                        |
| 1            |                        |
| 2            |                        |
| 3            |                        |
| 4            |                        |
| 5            |                        |
| 6            |                        |
| 7            |                        |
| 8            |                        |
| 9            |                        |
| Backtick     |                        |
| Minus        |                        |
| Equal        |                        |
| Star         |                        |
| Plus         |                        |
| LeftBracket  |                        |
| RightBracket |                        |
| BackSlash    |                        |
| SemiColon    |                        |
| Quote        |                        |
| Comma        |                        |
| Period       |                        |
| Slash        |                        |
| Escape       |                        |
| Enter        |                        |
| Tab          |                        |
| Backspace    |                        |
| Space        |                        |
| Insert       |                        |
| Delete       |                        |
| Left         |                        |
| Right        |                        |
| Up           |                        |
| Down         |                        |

### Colors

The colors used for by terminals running inside ttx. This includes the default values for the color palette, cursor colors, and selection colors. When not specified ttx will use the default colors used by the outer terminal.

| Field                | Type                    | Default | Description                                                                                                                                                      |
| -------------------- | ----------------------- | ------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| palette              | list of [Color](#Color) | unset   | The terminal palette colors to use as an array. Up to 256 colors can be specified but typically only the first 16 are, which correspond the typical ANSI colors. |
| foreground           | [Color](#Color)         | unset   | The default foreground color to use in the terminal.                                                                                                             |
| background           | [Color](#Color)         | unset   | The default background color to use in the terminal.                                                                                                             |
| selection_background | [Color](#Color)         | unset   | The background color to use when highlighting selection text. A value of dynamic will cause the foreground and background to be swapped for selected cells.      |
| selection_foreground | [Color](#Color)         | unset   | The foreground color to use when highlighting selection text. A value of dynamic will cause the foreground and foreground to be swapped for selected cells.      |
| cursor               | [Color](#Color)         | unset   | The default color to use when displaying the cursor. A value of dynamic indicates the default foreground color will be used.                                     |
| cursor_text          | [Color](#Color)         | unset   | The default color to use when displaying text under the cursor. A value of dynamic indicates the default background color will be used.                          |

### Color

A color in ttx can be defined in one of four ways:

- A named special color ("default" or "dynamic"). Default has the special meaning of using the outer terminal's default color. Dynamic has the special depending on the specific color entry being specified and is not normally valid.
- A normal named color (such as "red" or "blue") as defined in the kitty color stack [docs](https://sw.kovidgoyal.net/kitty/color-stack).
- An 8 bit hex color (such as "#ff0000").
- A palette color via palette:N (such as "palette:0"). This is only valid when specifying colors not in already a palette color. For example, you can set the status bar's background to a palette color but not palette color 0 itself.

### Clipboard

Configuration relating to the clipboard handling of ttx (OSC 52).

| Field | Type                            | Default  | Description                                                                                                                                                                                                                                                                                                                                            |
| ----- | ------------------------------- | -------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| mode  | [ClipboardMode](#ClipboardMode) | "system" | Configure the way ttx interacts with the system clipboard when inner applications use OSC 52. By default ttx will read/write from the system clipboard. This requires configuring your terminal emulator to allow interacting with the system clipboard. If you do not want applications to access your system clipboard, specify the mode as 'local'. |

### ClipboardMode

This mode controls how ttx interacts with applications which request to read or write to the clipboard.

| Value                   | Description                                                |
| ----------------------- | ---------------------------------------------------------- |
| system                  | Attempt to read and write the system clipboard             |
| system-write-local-read | Write to system clipboard but read from internal clipboard |
| system-write-no-read    | Write to system clipboard but disallow reading clipboard   |
| local                   | Read and write to internal clipboard                       |
| local-write-no-read     | Write to internal clipboard but disallow reading clipboard |
| disabled                | Disallow read/writing the clipboard                        |

### Session

Configuration relating the session management, such as automatically saving and restoring the current layout.

| Field          | Type    | Default | Description                                                                                                                                                                                                          |
| -------------- | ------- | ------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| restore_layout | boolean | true    | Automatically restore the previous layout on startup. This does not preserve commands but does restore all sessions, tabs, and panes as well as their working directories.                                           |
| save_layout    | boolean | true    | Automaticaly save the current layout to a file whenever updates occur. The layout is saved to $XDG_DATA_HOME/ttx/layouts (~/.local/share/ttx/layouts). The layout file is meant for use the `restore_layout` option. |
| layout_name    | string  | unset   | Layout name to use for save/restore. This defaults the name of the chosen profile.                                                                                                                                   |

### Shell

Configuration relating to the shell ttx starts in each pane.

| Field   | Type           | Default | Description                                                                                                                                                                                                                               |
| ------- | -------------- | ------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| command | list of string | unset   | The command to run inside is pane. This is a list of arguments passed directly to the OS. If shell expansion is desired, use a command like this: ["sh", "-c", "<COMMAND>"]. By default '$SHELL' is started with no additional arguments. |

### Fzf

Configure the fzf popups used by the ttx UI.

| Field  | Type                    | Default | Description                             |
| ------ | ----------------------- | ------- | --------------------------------------- |
| colors | [FzfColors](#FzfColors) | {}      | Configure the theme used by fzf popups. |

### FzfColors

theme for the fzf popups used in the ttx UI. The names of the fields match directly the cooresponding fzf color name, documented [here](https://man.archlinux.org/man/fzf.1.en#GLOBAL_STYLE), although only a subset of keys are available.

| Field          | Type            | Default | Description                                                    |
| -------------- | --------------- | ------- | -------------------------------------------------------------- |
| fg             | [Color](#Color) | unset   | Text color of unselected entries.                              |
| fg+            | [Color](#Color) | unset   | Text color of selected entries.                                |
| bg             | [Color](#Color) | unset   | Background color of unselected entries.                        |
| bg+            | [Color](#Color) | unset   | Background color of selected entries.                          |
| border         | [Color](#Color) | unset   | Outer border color.                                            |
| header         | [Color](#Color) | unset   | Text color of the fzf header.                                  |
| hl             | [Color](#Color) | unset   | Text color to show the matched parts of an entry.              |
| hl+            | [Color](#Color) | unset   | Text color to show the matched parts of selected entries.      |
| info           | [Color](#Color) | unset   | Text color for info (located on the right of the prompt line). |
| label          | [Color](#Color) | unset   | Text color for text placed in an fzf border.                   |
| marker         | [Color](#Color) | unset   | Text color of multi-select indicator.                          |
| pointer        | [Color](#Color) | unset   | Text color of current entry indicator.                         |
| preview-border | [Color](#Color) | unset   | Border color for the preview window.                           |
| prompt         | [Color](#Color) | unset   | Text color of the query prompt.                                |
| selected-bg    | [Color](#Color) | unset   | Background color of selected entries.                          |
| separator      | [Color](#Color) | unset   | Color of line separator below the query text.                  |
| spinner        | [Color](#Color) | unset   | Color of the loading spinner.                                  |

### StatusBar

Configuration for the ttx status bar, including colors and layout.

| Field  | Type                                | Default | Description                                                             |
| ------ | ----------------------------------- | ------- | ----------------------------------------------------------------------- |
| hide   | boolean                             | false   | Hide the status bar. This is useful for minimal layouts or for testing. |
| colors | [StatusBarColors](#StatusBarColors) | {}      | Configure the colors used by the status bar.                            |

### StatusBarColors

Configuration for the ttx status bar colors.

| Field                          | Type            | Default | Description                                                                                                                                                     |
| ------------------------------ | --------------- | ------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| badge_text_color               | [Color](#Color) | unset   | The text color to use for colored badges (the left part of status bar components). A value of dynamic means use the default background color as the text color. |
| label_text_color               | [Color](#Color) | unset   | The text color to use for the text label of tabs.                                                                                                               |
| background_color               | [Color](#Color) | unset   | The default background color to use for the status bar.                                                                                                         |
| label_background_color         | [Color](#Color) | unset   | The background color to use for tab labels.                                                                                                                     |
| active_tab_badge_color         | [Color](#Color) | unset   | The color to use to signify a tab is active.                                                                                                                    |
| inactive_tab_badge_color       | [Color](#Color) | unset   | The color to use to signify a tab is inactive.                                                                                                                  |
| session_badge_background_color | [Color](#Color) | unset   | The background color to use for the session name badge.                                                                                                         |
| host_badge_background_color    | [Color](#Color) | unset   | The background color to use for the host name badge.                                                                                                            |
| switch_mode_color              | [Color](#Color) | unset   | The color to use to signify you are in SWITCH mode.                                                                                                             |
| insert_mode_color              | [Color](#Color) | unset   | The color to use to signify you are in INSERT mode.                                                                                                             |
| normal_mode_color              | [Color](#Color) | unset   | The color to use to signify you are in NORMAL mode.                                                                                                             |
| resize_mode_color              | [Color](#Color) | unset   | The color to use to signify you are in RESIZE mode.                                                                                                             |

### Terminfo

Configuration relating to the terminfo ttx passes to inner applications.

| Field                | Type    | Default     | Description                                                                                                                                                                                                                                                                                                      |
| -------------------- | ------- | ----------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| term                 | string  | "xterm-ttx" | Value of the TERM enviornment variable to pass to inner applications. ttx automaticaly sets up the terminfo so the default value of 'xterm-ttx' will work locally. However, ttx's terminfo is not automaticaly available when using ssh or sudo. If this is causing issues, set this option to 'xterm-256color'. |
| force_local_terminfo | boolean | false       | Compile ttx's terminfo on startup and set TERMINFO_DIR, even if the terminfo is already installed. This shouldn't be necessary but can be used if the system's terminfo is broken or for testing.                                                                                                                |

## Default JSON Configuration

This JSON block contains the default JSON configuration used by ttx:

```json
{
  "$schema": "https://github.com/coletrammer/ttx/raw/refs/heads/main/meta/schema/config.json",
  "clipboard": {
    "mode": "system"
  },
  "extends": [],
  "input": {
    "disable_default_keybinds": false,
    "prefix": "B",
    "save_state_path": "/tmp/ttx-save-state.ansi"
  },
  "session": {
    "restore_layout": true,
    "save_layout": true
  },
  "status_bar": {
    "hide": false
  },
  "terminfo": {
    "force_local_terminfo": false,
    "term": "xterm-ttx"
  },
  "theme": {
    "name": "ansi"
  },
  "version": 1
}
```
