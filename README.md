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

- [ ] Platform support
  - [x] Linux
  - [x] MacOS
  - [ ] Windows
- [ ] Basic multiplexing functionality
  - [x] Layout tree with horizontal and vertical splits
  - [x] Multiple tabs (tmux windows)
  - [ ] Multiple sessions
  - [ ] Popup windows
  - [ ] Support session/window/pane switcher via Fzf popup
- [ ] Graphics
  - [x] Kitty image protocol (APC passthrough)
    - [ ] Proper Unicode handling (specifically 0 width characters)
  - [ ] Kitty image protocol (actual support)
    - [ ] Conversion of image to text (e.g. via `chafa`) when not supported by the host terminal
  - [ ] Sixels
  - [ ] Iterm image protocol
- [ ] Daemon Mode
  - [ ] Terminal sessions are managed by a background process, allowing sessions to be saved after closing the terminal
        or dropping the SSH connection.
- [ ] Session Persistence Across Reboots
  - [ ] Manual save/restore (tmux-resurrect)
  - [ ] Automatic save/restore (tmux-continuum)
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

## Dependencies

This project depends on the [dius](https://github.com/coletrammer/dius) library, to use its cross-platform
abstractions.

## Installing

See [here](docs/pages/install.md).

## Building

See [here](docs/pages/build.md).

## Developing

See [here](docs/pages/developing.md).
