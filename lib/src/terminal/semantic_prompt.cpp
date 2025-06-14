#include "ttx/terminal/semantic_prompt.h"

namespace ttx::terminal {
void Commands::clamp_commands(u64 absolute_row_start, u64 absolute_row_end) {}

void Commands::begin_prompt(di::String application_id, PromptClickMode clock_mode, PromptKind kind, bool redraw,
                            Cursor const& cursor) {}

void Commands::end_prompt(Cursor const& cursor) {}

void Commands::end_input(Cursor const& cursor) {}

void Commands::end_command(di::String application_id, bool failed, Cursor const& cursor) {}
}
