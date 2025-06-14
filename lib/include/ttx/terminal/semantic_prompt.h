#pragma once

#include "di/container/ring/prelude.h"
#include "di/reflect/prelude.h"
#include "ttx/terminal/cursor.h"

namespace ttx::terminal {
/// @brief Controls how mouse events on the shell prompt are translated to the application
///
/// This allows clicking in the shell input region to actual work as the user was intending.
enum class PromptClickMode {
    None,                       ///< Doesn't support prompt click mode
    Line,                       ///< Can only navigative a single line via mouse movement, using left-right presses
    MultipleLeftRight,          ///< Can simulate up-down movement via lots of left-right presses
    MultipleUpDown,             ///< Can use up-down and left-right
    MultipleUpDownConservative, ///< Only allowed to use up down at col 0
};

constexpr auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<PromptClickMode>) {
    using enum PromptClickMode;
    return di::make_enumerators<"PromptClickMode">(
        di::enumerator<"None", None>, di::enumerator<"Line", Line>,
        di::enumerator<"MultipleLeftRight", MultipleLeftRight>, di::enumerator<"MultipleUpDown", MultipleUpDown>,
        di::enumerator<"MultipleUpDownConservative", MultipleUpDownConservative>);
}

/// @brief Kind of prompt
enum class PromptKind {
    Initial,      ///< Initial prompt (default)
    Continuation, ///< Continuation prompt (user can edit previous lines)
    Secondary,    ///< Secondary prompt (user cannot edit previous lines)
    Right,        ///< Right-aligned prompt (can reflow to the right on resize)
};

constexpr auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<PromptKind>) {
    using enum PromptKind;
    return di::make_enumerators<"PromptKind">(di::enumerator<"Initial", Initial>,
                                              di::enumerator<"Continuation", Continuation>,
                                              di::enumerator<"Secondary", Secondary>, di::enumerator<"Right", Right>);
}

/// @brief Represents a completed annotated command via OSC 133
///
/// The completed command consists of regions of lines corresponding to the command
/// output, shell prompt, and user input. We also subshells by assigning a depth of
/// the command.
struct Command {
    u64 output_start { 0 }; ///< Absolute row marking the command output start
    u64 output_end { 0 };   ///< Absolute row marking the command output end (inclusive)
    u32 depth { 0 };        ///< Level of nesting within other commands
    bool failed { false };  ///< Did the command exit successfully?
};

/// @brief Represents all commands received for the screen
class Commands {
public:
    void clamp_commands(u64 absolute_row_start, u64 absolute_row_end);

    void begin_prompt(di::String application_id, PromptClickMode clock_mode, PromptKind kind, bool redraw,
                      Cursor const& cursor);
    void end_prompt(Cursor const& cursor);
    void end_input(Cursor const& cursor);
    void end_command(di::String application_id, bool failed, Cursor const& cursor);

private:
    di::Ring<Command> m_commands;
    u32 m_current_depth { 0 };
};
}
