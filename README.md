# ttx Overview

A modern terminal multiplexer.

## Features

The main goal of ttx to provide a good user experience when mixing local and remote terminal sessions, and support new
terminal features in a timely manner (think kitty graphics/keyboard protocol). Instead of being a source of
compatibility issues, I want ttx to _just work_ when users try out new terminal features. And managing remote sessions should
be simple and painless. Instead of having 1 terminal window and 1 tmux session per host, or nesting tmux sessions inside
each other, ttx aims to unify remote hosts into a single terminal environment. I think this can be a huge improvement for
developers who regularly need to develop on remote machines.

This project is currently in very early development, so there is a lot of the features listed here haven't
yet been implemented. This list focuses only on higher level features, leaving out a lot of the lower level
details.

- [x] Platform support
  - [x] Linux
  - [x] MacOS
  - [ ] Windows (not a priority)
- [x] Basic multiplexing functionality
  - [x] Layout tree with horizontal and vertical splits
  - [x] Multiple tabs (tmux windows)
  - [x] Multiple sessions
  - [x] Popup windows
  - [x] Support session/window/pane switcher via Fzf popup
  - [x] Full Unicode support (grapheme clustering and wide characters)
- [ ] Graphics
  - [x] Kitty image protocol (APC passthrough)
    - [x] Proper Unicode handling (specifically 0 width characters)
  - [ ] Kitty image protocol (actual support)
    - [ ] Conversion of image to text (e.g. via `chafa`) when not supported by the host terminal
  - [ ] Sixels
  - [ ] Iterm image protocol
- [ ] Daemon Mode
  - [ ] Terminal sessions are managed by a background process, allowing sessions to be saved after closing the terminal
        or dropping the SSH connection.
- [ ] Session Persistence Across Reboots
  - [x] Manual save/restore (tmux-resurrect) (partial support (layout+cwd only, not processes or scrollback))
  - [x] Automatic save/restore (tmux-continuum) (partial support (layout+cwd only, not processes or scrollback))
- [ ] Remote Machines
  - [ ] Support ephemeral connections via ssh
  - [ ] Support persistent connections via ssh
    - [ ] Will run a daemon on the remote which will synchronize locally.
  - [ ] Connection management UX
  - [ ] Support creating sessions/windows/panes using remote connections
  - [ ] See remote and local on the same screen at the same time.
- [ ] Extensibility
  - [ ] Settings
  - [ ] Plugins

## Installing

See [here](docs/pages/install.md).

## Unicode Support

We aim to match the Unicode processing specified by [kitty](https://github.com/kovidgoyal/kitty/blob/master/docs/text-sizing-protocol.rst#the-algorithm-for-splitting-text-into-cells).
This includes correctly handling emoji sequences and grapheme clusters. When `ttx` is run in a terminal which does not
support this, text to fit into whatever the outer terminal thinks is a single cell. This matches the behavior of
Neovim 0.11 when running in these terminals.

These issues can be avoided by using a terminal like [kitty](https://github.com/kovidgoyal/kitty),
[ghostty](https://github.com/ghostty-org/ghostty), or [wezterm](https://github.com/wezterm/wezterm). To check if your terminal
supports grapheme clustering, run `ttx --features`, and see if the `BasicGraphemeClustering` feature is detected.

## Building

See [here](docs/pages/build.md).

## Developing

See [here](docs/pages/developing.md).

## Dependencies

- [fzf](https://github.com/junegunn/fzf) program, for various popup memus
- [dius](https://github.com/coletrammer/dius) library, to use its cross-platform
  abstractions.
