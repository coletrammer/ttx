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

| Field     | Type                    | Default | Description                                                                                                                                              |
| --------- | ----------------------- | ------- | -------------------------------------------------------------------------------------------------------------------------------------------------------- |
| extends   | list of string          | []      | List of configuration files to extend. These can recursively extend more files. Priority is given to the last time the configuration option is specified |
| input     | [Input](#Input)         | {}      | Configuration relating the input processing of ttx (primarily key bindings)                                                                              |
| layout    | [Layout](#Layout)       | {}      | Configuration relating to the layout of panes in ttx, including the status bar and pane spacing                                                          |
| clipboard | [Clipboard](#Clipboard) | {}      | Configuration relating to the clipboard handling of ttx (OSC 52)                                                                                         |
| session   | [Session](#Session)     | {}      | Configuration relating the session management, such as automatically saving and restoring the current layout                                             |
| shell     | [Shell](#Shell)         | {}      | Configuration relating to the shell ttx starts in each pane                                                                                              |
| terminfo  | [Terminfo](#Terminfo)   | {}      | Configuration relating to the terminfo ttx passes to inner applications                                                                                  |

### Input

Configuration relating the input processing of ttx (primarily key bindings).

| Field                    | Type        | Default                    | Description                                                                                                                                                                                                                                                                  |
| ------------------------ | ----------- | -------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| prefix                   | [Key](#Key) | "B"                        | Prefix key to use. Almost all key bindings require using the prefix key to enter 'NORMAL' mode first. Currently, the prefix key must be pressed in conjunction with the control key. So specifying a prefix of 'A' means that to enter 'NORMAL' mode you must press 'ctrl+a' |
| disable_default_keybinds | boolean     | false                      | Disable all default key bindings to allow full customization                                                                                                                                                                                                                 |
| save_state_path          | string      | "/tmp/ttx-save-state.ansi" | Path to write save state files captured by ttx. These save state files can be replayed using `ttx replay`. This functionality is useful for testing and reproducing bugs                                                                                                     |

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

### Layout

Configuration relating to the layout of panes in ttx, including the status bar and pane spacing.

| Field           | Type    | Default | Description                                         |
| --------------- | ------- | ------- | --------------------------------------------------- |
| hide_status_bar | boolean | false   | Hide the status bar for an extremely minimal layout |

### Clipboard

Configuration relating to the clipboard handling of ttx (OSC 52).

| Field | Type                            | Default  | Description                                                                                                                                                                                                                                                                                                                                           |
| ----- | ------------------------------- | -------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| mode  | [ClipboardMode](#ClipboardMode) | "system" | Configure the way ttx interacts with the system clipboard when inner applications use OSC 52. By default ttx will read/write from the system clipboard. This requires configuring your terminal emulator to allow interacting with the system clipboard. If you do not want applications to access your system clipboard, specify the mode as 'local' |

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

| Field          | Type    | Default | Description                                                                                                                                                                                                         |
| -------------- | ------- | ------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| restore_layout | boolean | true    | Automatically restore the previous layout on startup. This does not preserve commands but does restore all sessions, tabs, and panes as well as their working directories                                           |
| save_layout    | boolean | true    | Automaticaly save the current layout to a file whenever updates occur. The layout is saved to $XDG_DATA_HOME/ttx/layouts (~/.local/share/ttx/layouts). The layout file is meant for use the `restore_layout` option |
| layout_name    | string  | unset   | Layout name to use for save/restore. This defaults the name of the chosen profile                                                                                                                                   |

### Shell

Configuration relating to the shell ttx starts in each pane.

| Field   | Type           | Default | Description                                                                                                                                                                 |
| ------- | -------------- | ------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| command | list of string | []      | The command to run inside is pane. This is a list of arguments passed directly to the OS. If shell expansion is desired, use a command like this: ["sh", "-c", "<COMMAND>"] |

### Terminfo

Configuration relating to the terminfo ttx passes to inner applications.

| Field                | Type    | Default     | Description                                                                                                                                                                                                                                                                                                     |
| -------------------- | ------- | ----------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| term                 | string  | "xterm-ttx" | Value of the TERM enviornment variable to pass to inner applications. ttx automaticaly sets up the terminfo so the default value of 'xterm-ttx' will work locally. However, ttx's terminfo is not automaticaly available when using ssh or sudo. If this is causing issues, set this option to 'xterm-256color' |
| force_local_terminfo | boolean | false       | Compile ttx's terminfo on startup and set TERMINFO_DIR, even if the terminfo is already installed. This shouldn't be necessary but can be used if the system's terminfo is broken or for testing                                                                                                                |

## Default JSON Configuration

This JSON block contains the default JSON configuration used by ttx:

```json
{
  "$schema": "https://github.com/coletrammer/ttx/raw/refs/heads/main/meta/schema/config.json",
  "version": 1,
  "extends": [],
  "input": {
    "prefix": "B",
    "disable_default_keybinds": false,
    "save_state_path": "/tmp/ttx-save-state.ansi"
  },
  "layout": {
    "hide_status_bar": false
  },
  "clipboard": {
    "mode": "system"
  },
  "session": {
    "restore_layout": true,
    "save_layout": true
  },
  "shell": {
    "command": []
  },
  "terminfo": {
    "term": "xterm-ttx",
    "force_local_terminfo": false
  }
}
```
