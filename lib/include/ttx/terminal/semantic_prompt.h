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
    di::String application_id;                                   ///< Application id of the command
    PromptClickMode prompt_click_mode { PromptClickMode::None }; ///< Prompt click mode
    PromptKind prompt_kind { PromptKind::Initial }; ///< Prompt kind - for now we only support a single prompt region
    bool prompt_redraw { true };                    ///< Does the application redraw the prompt?
    u64 prompt_start { 0 };                         ///< Absolute row marking the prompt start
    u64 prompt_start_col { 0 };                     ///< Column marking start of prompt
    u64 prompt_end { 0 };                           ///< Absolute row marking the prompt end (inclusive)
    u32 prompt_end_col { 0 }; ///< Column marking the end of the prompt (thus marking the start of the input area)
    u64 output_start { 0 };   ///< Absolute row marking the command output start
    u64 output_end { 0 };     ///< Absolute row marking the command output end (exclusive)
    u32 depth { 0 };          ///< Level of nesting within other commands
    bool failed { false };    ///< Did the command exit successfully?
    bool ended { false };     ///< Has the command been completed?

    auto operator==(Command const&) const -> bool = default;

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<Command>) {
        return di::make_fields<"Command">(
            di::field<"application_id", &Command::application_id>,
            di::field<"prompt_click_mode", &Command::prompt_click_mode>,
            di::field<"prompt_kind", &Command::prompt_kind>, di::field<"prompt_redraw", &Command::prompt_redraw>,
            di::field<"prompt_start", &Command::prompt_start>,
            di::field<"prompt_start_col", &Command::prompt_start_col>, di::field<"prompt_end", &Command::prompt_end>,
            di::field<"prompt_end_col", &Command::prompt_end_col>, di::field<"output_start", &Command::output_start>,
            di::field<"output_end", &Command::output_end>, di::field<"depth", &Command::depth>,
            di::field<"failed", &Command::failed>);
    }
};

/// @brief Represents all commands received for the screen
class Commands {
    // Cap the maximum command depth to more sanely handle cases
    // where the shell fails to terminate the command, or otherwise
    // spam us with nonsense like infinite number of prompts.
    constexpr static auto max_depth = u32(20);

public:
    void clamp_commands(u64 absolute_row_start, u64 absolute_row_end);

    void begin_prompt(di::String application_id, PromptClickMode click_mode, PromptKind kind, bool redraw,
                      u64 absolute_row, u32 col);
    void end_prompt(u64 absolute_row, u32 col);
    void end_input(u64 absolute_row, u32 col);
    void end_command(di::String application_id, bool failed, u64 absolute_row, u32 col);

    auto last_command() const -> di::Optional<Command const&>;
    auto will_redraw_prompt(u64 absolute_row, u32 col) const -> bool;
    auto first_command_before(u64 absolute_row) const -> di::Optional<Command const&>;
    auto first_command_after(u64 absolute_row) const -> di::Optional<Command const&>;

private:
    di::Ring<Command> m_commands;
    u32 m_current_depth { 0 };
};
}
