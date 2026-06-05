# Server

In order to persist terminals in the background we need a client/server architecture where the server maintains
all of the terminals, and the client renders these terminals for the user.

## Requirements

In `tmux`, the server is responsible for compositing and almost functionality. This means that the client mostly
forwards IO events and renders whatever the server tells it to. However, this means there is less flexibility
in what the client can do.

For the `ttx` server, we want to use a model where the client does compositing and the server focus on persisting
a set of terminals. The client will make layout and rendering decisions. This allows having multiple clients
connecting displaying different "sessions" at the same time, and also makes it possible to create new clients
(perhaps an actual GUI program) easily.

Although the server will support multiple clients, it needs to maintain the current "active" client for specific
functionality that requires bi-directional communication. The main example of this is clipboard requests, which
need to be proxied via OSC 52 in the outer terminal.

### Detailed List of Pane Feature

#### Callbacks

| Name                            | Requires Active Client | Description                                                                                                                                                                          |
| ------------------------------- | ---------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| did_exit                        | Yes                    | When the process running inside the terminal exits. This must be broadcast to the clients.                                                                                           |
| did_update                      | No                     | When the terminal was updated. Rendering will work different in the client/server model.                                                                                             |
| did_selection                   | Yes                    | These a clipboard requests.                                                                                                                                                          |
| did_receive_seamless_navigation | Yes                    | This requires the client change the outer pane, so does require client communication.                                                                                                |
| apc_passthrough                 | Yes                    | Since we're passing through APC (kitty graphics), this must be broadcast to all clients. This approach is somewhat flawed and should be replaced by actually support kitty graphics. |
| did_finish_output               | No                     | This is specifically for running fzf popups. These do not need to run in the server context since they aren't persistent.                                                            |
| did_get_extra_output            | No                     | Same as did_finish_output.                                                                                                                                                           |
| did_update_cwd                  | No                     | This affects status bar rendering and some actions (like creating new panes), but its unclear how this will be translated.                                                           |

#### Methods

| Name                      | Needs IPC | Require Change | Description                                                                                                  |
| ------------------------- | --------- | -------------- | ------------------------------------------------------------------------------------------------------------ |
| id                        | No        | No             | The pane id will be stored on the client and server.                                                         |
| draw                      | Yes       | Yes            | Rendering terminals requires IPC, but the mechanism is going to differ.                                      |
| event                     | Yes       | No             | All events (key/mouse/focus/paste) require communication with the server.                                    |
| accepts_scrolling         | Yes       | Yes            | This is needed for smart scrolling keyboard shortcuts. This should be part of the return value of scrolling. |
| invalidate_all            | Yes       | Yes            | Rendering will work differently.                                                                             |
| resize                    | Yes       | No             | Updating the visible size will work similar to how it works now.                                             |
| scroll\_\*                | Yes       | No             | As long as scroll state is handled on the server side, no significant changes are needed.                    |
| copy_last_command         | Yes       | No             | This could be updated to return the selected text instead of faking an OSC 52.                               |
| save_state                | Yes       | No             | This is for testing/debugging.                                                                               |
| send_clipboard            | Yes       | No             | This is to inform the server of clipboard requests which has now been answered.                              |
| stop_capture              | Yes       | No             | This is for testing/debugging.                                                                               |
| soft_reset                | Yes       | No             | This is for fixing broken terminals when applications don't clean up.                                        |
| exit                      | Yes       | No             | This for when the user kills a pane.                                                                         |
| seamless_navigate         | Yes       | No             | Like send_clipboard.                                                                                         |
| current_working_directory | Yes       | Maybe          | This is used for the tab display name as well as creating new panes. We may broadcast updates instead.       |
| window_title              | Yes       | Maybe          | This is used for the tab display name. We may broadcast updates instead.                                     |
| update_local_palette      | Yes       | Yes            | This is used for the theme preview and will need at least some rework.                                       |
| set_global_palette        | Yes       | No             | This is broadcast by the active client to all panes.                                                         |
| set_theme_mode            | Yes       | No             | This is broadcast by the active client to all panes.                                                         |

#### Threads

| Name                     | Purpose                               |
| ------------------------ | ------------------------------------- |
| process_thread           | Wait the the spawn process to exit.   |
| output_thread            | Write messages to the tty.            |
| reader_thread            | Read messages from the ttx.           |
| pipe_writer_thread       | Write the input (for fzf popups)      |
| pipe_reader_thread       | Read the pipe output (for fzf popups) |
| pipe_extra_reader_thread | Read the pipe output (for fzf popups) |

The main reason each of these are separate threads is because of the lack of event loop abstraction in dius.
As long as async IO is used, all these threads could be collapsed. Note it is very important that
tty writes and tty reads can progress in parallel.

## Concerns

The above analysis reveals the main point to consider during the migration. Starting with easiest to hardest to answer:

### Multiple Clients vs. Global Palette

When inner application use OSC 4 or OSC 21 or DEC MODE 2031 to determine palette colors or dark/light theme, there is
only 1 palette. If there are multiple clients each one could have its own palette and dark/light preference. This is a
bit weird but is easily solvable by getting precedence to the current "active" client.

### CWD and Window Title

This is primarily state used for rendering the status bar. Rather than querying the server for this information it makes
more sense to either broadcast updates or incorporate this information into the rendering process.

### Popups

The FZF menu popups do not strictly require persistence and so could be implemented client side. However, we will want
server side popups as well, for cases where the application requests it (like fzf-tmux would). Its therefore going to be
simpler to always run all panes on the server.

### Rendering

Rendering is the most significant requirement change when moving to a server model, because we can no longer
just draw directly to the screen. Conceptually, we need the client to request the current state of the terminal
every frame before rendering.

We can optimize state updates using the same approach we use we rendering to the screen. In this model, every client
pane would have a local `terminal::Screen` (with no scroll back) that would receive the results on rendering individual
panes. This way we only need to transmit the minimal set of updates each time, and certain affects won't require
communicating with the server.

However, this does mean that when there are multiple clients the server will have to render panes multiple times. This
isn't so bad and also allows rendering into different sized terminals.

Another important consideration is frame rate and idle behavior. Currently `ttx` runs at 50 fps when any changes are
made to the terminal but otherwise will fully sleep. To emulate this behavior the client will subscribe to change
notifications for the set of visible panes when moving to the idle state, and use that to wake itself up. When actively
running at 50 fps change events are not needed.

## Future Requirements

In addition to the current functionality there are a few new features that the new model needs to support.

### Programmatic Actions

Currently only key presses can trigger actions, but programmatic triggering is also needed. Because the client
is required for certain actions (like creating new panes) these requests will need to be forwarded from the server
to the active client. These can be triggered by connecting to the server or a special terminal escape sequence.

### Remote Servers via Proxying

For persistence on a remote and local machine, the current model would be to have nested `ttx` instances. Because the
server only persists terminals it becomes feasible to combine multiple server's together into a single client. This
could be done client side but also could be done through proxying on the local server seamlessly to the remote server.
The main benefit of proxying is making the client simpler and better supporting multiple clients.

### Multiple Clients

For multiple clients to be able to synchronize the server will allow storing an opaque JSON state object which the
active client can write to. Other clients can read this to synchronize the state of their layouts.

## Server Architecture

Every terminal owned by the server requires persistent IO for reading the tty. There are 2 possible architectures
that make sense:

- 1 Thread per Terminal
- N Worker Threads

The server connection itself is similar in that we can either use 1 thread per connection or a shared pool of workers.
If we have a good abstraction for shared worker threads there's not much reason to not use it for scenarios, since its a
bit more scalable for large numbers of terminals. But the threading model isn't the most important as long as async io
is being used, since that will make it easier to switch the execution context.

With a shared worker model it may make sense to separate IO operations from actual compute tasks. Parsing and processing
the terminal escape sequences could take a large enough time and it would be bad to block the IO from getting processed.
For the server/client connections, this seems less important because the amount of compute required should be a lot
lower and sending the response will require going back to the IO context.

### Socket Connection

The server will need a socket to listen for incoming connections. It makes sense to use a unix domain socket for this.
When we support remote connections we will want to tunnel over ssh somehow.

### Event Loop Requirements

The following async notifications need to be supported by the event loop mechanism used by the server and client:

- read/write files
- create/read/write IPC socket
- notification when files change (for config)
- notification when process exits
- timeout/timer functionality
- signal handling (SIGWINCH + SIGCHLD)
- cancellation
- thread safety

## Migration

Looking holistically at the list of changes required to migrate to a client/server architecture, we need to rewrite the
part of the client other than terminal emulation. Layout can work mostly the same but cannot directly reference `Pane`
objects anymore.

### TODO List

#### di

- [-] Async queue
- [x] Async mutex
- [x] Atomic intrusive queue (as helper for async mutex and queue?)
- [x] Async resource helper (define resource with 2 senders)
- [x] Delete async io, ipc binary, etc. (move to dius)
- [x] di::Lazy<> scheduler affinity (not sure how this works, but need to run di::Lazy<> on a specific scheduler
      regardless of what it waits for)
- [x] Time based scheduling (schedule at time point or after duration) + timeout() utility

#### dius

- [x] Thread refactor (like std::jthread)
- [x] Thread async context
- [ ] Thread pool async context
- [x] Better socket operations + async IO definitions
- [ ] Refactor io_uring backed context (thread safety + cancellation + adding operations while blocked)
- [x] Add async waiting for signals
- [x] Add async waiting for process completion (maybe use SIGCHLD?)
- [x] Add time based scheduling to contexts
- [x] Add file change notifications
- [ ] Add kqueue backed context for MacOS
