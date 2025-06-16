#include "ttx/terminal/semantic_prompt.h"

#include "di/container/algorithm/find_last.h"
#include "di/container/algorithm/remove_if.h"
#include "di/function/equal_or_greater.h"
#include "di/function/greater.h"
#include "di/function/less.h"

namespace ttx::terminal {
void Commands::clamp_commands(u64 absolute_row_start, u64 absolute_row_end) {
    // Remove commands whose prompt start is no longer valid.
    while (m_commands.front().transform(&Command::prompt_start).transform(di::less(absolute_row_start)) == true) {
        m_commands.pop_front();
    }

    // Remove commands whose output end is greater than the row end. This is safe for pending commands as the default
    // value for the command end is 0.
    while (m_commands.back().transform(&Command::output_end).transform(di::greater(absolute_row_end)) == true) {
        m_commands.pop_back();
    }
}

void Commands::begin_prompt(di::String application_id, PromptClickMode click_mode, PromptKind kind, bool redraw,
                            u64 absolute_row, u32 col) {
    // Sanity check - if the current depth is too high, ignore.
    if (m_current_depth >= max_depth) {
        return;
    }

    // Delete any command which starts after this one. This ensures the commands are sorted by command
    // start, any cleans up any commands which shouldn't been deleted when clearing the screen.
    while (m_commands.back().transform(&Command::prompt_start).transform(di::equal_or_greater(absolute_row)) == true) {
        m_commands.pop_back();
    }

    // Initialize the new command
    auto& command = m_commands.emplace_back();
    command.prompt_click_mode = click_mode;
    command.prompt_kind = kind;
    command.prompt_redraw = redraw;
    command.application_id = di::move(application_id);
    command.prompt_start = absolute_row;
    command.prompt_start_col = col;
    command.prompt_end = absolute_row;
    command.prompt_end_col = col;
    command.depth = m_current_depth++;
}

void Commands::end_prompt(u64 absolute_row, u32 col) {
    // Sanity check - ensure there is a command to terminate the prompt of.
    if (m_commands.empty()) {
        return;
    }

    // If the prompt end is before the prompt start, disgard this command.
    auto& command = m_commands.back().value();
    if (absolute_row < command.prompt_start ||
        (absolute_row == command.prompt_start && col < command.prompt_start_col)) {
        m_commands.pop_back();
        m_current_depth--;
        return;
    }

    command.prompt_end = absolute_row;
    command.prompt_end_col = col; // Exclusive bound, unlike row based boundaries.
}

void Commands::end_input(u64 absolute_row, u32) {
    // Sanity check - ensure there is a command to terminate the input of.
    if (m_commands.empty()) {
        return;
    }

    // If the input end row is above the prompt start, digard this command.
    auto& command = m_commands.back().value();
    if (absolute_row < command.prompt_start) {
        m_commands.pop_back();
        m_current_depth--;
        return;
    }

    // Ending the input really just manes starting the command output. We only care
    // about the row for this.
    command.output_start = absolute_row;
}

void Commands::end_command(di::String application_id, bool failed, u64 absolute_row, u32) {
    // Search for the command to end - if there is no application id that is the last command,
    // and otherwise is the one with a matching application id. When terminating by application
    // id, all commands further down are also terminated.
    auto command_to_end = m_commands.end();
    if (application_id.empty() && !m_commands.empty()) {
        --command_to_end;
    } else if (!application_id.empty()) {
        command_to_end = di::find_last(m_commands, application_id, &Command::application_id).begin();
    }

    // Now terminate all necessary commands.
    for (auto& command : di::View(command_to_end, m_commands.end())) {
        // The command status only applies to the first command. This also applies when restoring the depth.
        if (&command == &*command_to_end) {
            command.failed = failed;
            m_current_depth = command.depth;
        }
        command.output_end = absolute_row;
    }

    // Delete any commands which are invalid - due to having output end values which are before the
    // output start.
    m_commands.erase(di::container::remove_if(di::View(command_to_end, m_commands.end()), di::greater(absolute_row),
                                              &Command::output_start)
                         .begin(),
                     m_commands.end());
}
}
