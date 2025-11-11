#pragma once

#include "action.h"
#include "tab.h"
#include "ttx/layout.h"

namespace ttx {
auto enter_normal_mode() -> Action;
auto reset_mode() -> Action;
auto navigate(NavigateDirection direction) -> Action;
auto resize(ResizeDirection direction, i32 amount_in_cells) -> Action;
auto create_tab() -> Action;
auto rename_tab() -> Action;
auto switch_tab(usize index) -> Action;
auto switch_next_tab() -> Action;
auto switch_prev_tab() -> Action;
auto find_tab() -> Action;
auto create_session() -> Action;
auto rename_session() -> Action;
auto switch_next_session() -> Action;
auto switch_prev_session() -> Action;
auto find_session() -> Action;
auto quit() -> Action;
auto save_layout() -> Action;
auto save_state(di::Path path) -> Action;
auto stop_capture() -> Action;
auto exit_pane() -> Action;
auto soft_reset() -> Action;
auto hard_reset() -> Action;
auto toggle_full_screen_pane() -> Action;
auto add_pane(Direction direction) -> Action;
auto scroll(Direction direction, i32 amount_in_cells) -> Action;
auto scroll_prev_command() -> Action;
auto scroll_next_command() -> Action;
auto send_to_pane() -> Action;
}
