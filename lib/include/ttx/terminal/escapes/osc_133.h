#pragma once

#include "di/container/string/prelude.h"
#include "di/reflect/prelude.h"
#include "ttx/terminal/multi_cell_info.h"

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

/// @brief Represents the marker beginning a shell prompt
struct BeginPrompt {
    di::String application_id {};                         ///< Application id, used to detect sub-shells
    PromptClickMode click_mode { PromptClickMode::None }; ///< Application support for prompt click mode
    PromptKind kind { PromptKind::Initial };              ///< Prompt kind
    bool redraw { true }; ///< Shell redraws the prompt, allows clearing prompt on resize

    auto operator==(BeginPrompt const&) const -> bool = default;

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<BeginPrompt>) {
        return di::make_fields<"BeginPrompt">(di::field<"application_id", &BeginPrompt::application_id>,
                                              di::field<"click_mode", &BeginPrompt::click_mode>,
                                              di::field<"kind", &BeginPrompt::kind>,
                                              di::field<"redraw", &BeginPrompt::redraw>);
    }
};

/// @brief Represents the marker ending a shell prompt
struct EndPrompt {
    auto operator==(EndPrompt const&) const -> bool = default;

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<EndPrompt>) {
        return di::make_fields<"EndPrompt">();
    }
};

/// @brief Represents the marker ending user input (and beginning of a command)
///
/// The actual user input appears to be the text between prompts (or the end of line), on
/// the lines which a prompt for the current command.
struct EndInput {
    auto operator==(EndInput const&) const -> bool = default;

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<EndInput>) {
        return di::make_fields<"EndInput">();
    }
};

/// @brief Represents the end of a command, including exit status
///
/// The start of a command is either marked by the end of the user input.
struct EndCommand {
    di::String application_id {}; ///< Application id, used to detect sub-shells
    u32 exit_code { 0 };          ///< Command exit code
    di::String error {};          ///< Command error string (empty means success)

    auto operator==(EndCommand const&) const -> bool = default;

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<EndCommand>) {
        return di::make_fields<"EndCommand">(di::field<"application_id", &EndCommand::application_id>,
                                             di::field<"exit_code", &EndCommand::exit_code>,
                                             di::field<"error", &EndCommand::error>);
    }
};

/// @brief Represents the explicit ending of a prompt
///
/// When this isn't specified, it is assumed that the prompt ends at the end of the next line.

/// @brief Represents a semantic prompt command
///
/// This protocal is useful for shell integration with terminals, specified
/// [here](https://gitlab.freedesktop.org/Per_Bothner/specifications/blob/master/proposals/semantic-prompts.md).
///
/// This implementation deviates from the protocol to align with other terminals (iTerm, ghostty, and kitty),
/// while do not implement the full set of commands specified. Instead, only "A", "B", "C", and "D" are implemented.
/// Additionally, the "fresh-line" part of the spec is ignored because with the removal of the "P" command, there
/// would be no way to specify a "right" prompt without creating a new line. Other terminals also don't create
/// a fresh-line with the "A" command. Also, a "fresh-line" can be emulated by simply outputting N space characters,
/// followed by \r, and then clearing the line, where N is the terminal width (which appears to be what zsh does).
///
/// Additionally, the prompt begin command supports the "redraw" option, which defaults to true, and indicates
/// whether or not the shell redraws the prompt on resize. If the shell does redraw the prompt, when resizing
/// the screen, all text below and including the current prompt is cleared.
struct OSC133 {
    di::Variant<BeginPrompt, EndPrompt, EndInput, EndCommand> command;

    static auto parse(di::StringView data) -> di::Optional<OSC133>;

    auto serialize() const -> di::String;

    auto operator==(OSC133 const& other) const -> bool = default;

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<OSC133>) {
        return di::make_fields<"OSC133">(di::field<"command", &OSC133::command>);
    }
};
}
