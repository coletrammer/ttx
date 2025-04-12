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
auto switch_tab(usize index) -> Action;
auto quit() -> Action;
auto save_state(di::Path path) -> Action;
auto stop_capture() -> Action;
auto exit_pane() -> Action;
auto soft_reset() -> Action;
auto toggle_full_screen_pane() -> Action;
auto add_pane(Direction direction) -> Action;
auto scroll(Direction direction, i32 amount_in_cells) -> Action;
auto send_to_pane() -> Action;
}
