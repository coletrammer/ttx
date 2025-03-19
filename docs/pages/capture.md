# Capture

To facilitate testing and debugging issues, it is very useful to capture the escape sequences
being sent to the terminal, and be able to replay them.

## Saving Escape Sequences

Saving escape sequences is as easy as writing the raw incoming bytes to a file. Replaying these bytes
is also simple, and can be done via the `cat` command. However, in order for the terminal to behave
the same way as when the sequences was initially ran, the terminal must start in the same state.

For now, this means these captures must always start from the beginning of the terminal session.
In the future, once full persistence is implemented, the terminal can write out its state the moment
the capture begins.

This leaves the only relevant configuration being the terminal's size. To keep everything self-contained,
we will inject `CSI 8; height; width t` every time the terminal size changes (and once at the beginning).
This escape sequence (XTWINOPS) allows the application to set the terminal width and height in cells.
Since this is standard, our captures will be replayable in other terminal emulators. For good measure,
we can also begin captures with an escape sequence to fully reset the terminal state.

## Basic UI

Currently, the only application using the `libttx` terminal emulator is `ttx`, which also contains
functionality for having multiples panes and a status bar. For working with the captured escape
sequences, we really want a variant which just runs the terminal emulator as a single pane. This
super lightweight client could eventually be used with the same daemon `ttx`, to implement something
like `shpool`. But initially we just want to have a completely isolated `libttx` terminal.

## Validating Output

To validate that the terminal emulator is functioning correctly, we need to write out the terminal
state to disk. When developing a test, you would manually view the terminal output to make sure it
is correct, and then check in the expected output to prevent future regressions. The captured content
can also be replayed on other terminals to verify correctness. This nice thing about this method of
testing is that it is fully independent of the actual terminal implementation. (Which we want,
because the terminal implementation is going to go through lots of changes).

Because `ttx` will need to support disk persistence, the validation output ought to be the same
format. Also doing so naturally lets us test backwards compatibility with old versions of the
disk format if we need to change it. For now, it seems like the best format for writing out
the terminal state is a restricted subset of escape sequences. This makes it possible to validate
the terminal state using other terminal emulators, and additionally will be completely future
compatible with any refactoring on terminal state internals.

We will additionally need a full headless mode for the basic UI which just outputs the result of running
the program to disk. This will be used by an automated test suite.

## UI Tests

We can actually build upon this mechanism by running `ttx` inside itself to do UI level testing of
`ttx`. We would have an outer headless terminal which `ttx` runs inside and renders to. We could
instrument this outer terminal to send key events and window resizes as needed, and validate
`ttx` responds correctly.
