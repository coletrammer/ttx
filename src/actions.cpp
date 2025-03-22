#include "actions.h"

#include "action.h"
#include "di/format/prelude.h"
#include "tab.h"
#include "ttx/layout.h"

namespace ttx {
auto enter_normal_mode() -> Action {
    return {
        .description = "Enter normal mode - enabling most other key bindings"_s,
        .apply = di::into_void,
    };
}

auto reset_mode() -> Action {
    return {
        .description =
            "Reset mode to insert - this means all key presses will forwarded to the application running inside ttx"_s,
        .apply = di::into_void,
    };
}

auto navigate(NavigateDirection direction) -> Action {
    return {
        .description = *di::present(
            "Navigate to the first pane in direction ({}) starting from the current active pane"_sv, direction),
        .apply =
            [direction](ActionContext const& context) {
                context.layout_state.with_lock([&](LayoutState& state) {
                    if (!state.active_tab()) {
                        return;
                    }
                    state.active_tab()->navigate(direction);
                });
                context.render_thread.request_render();
            },
    };
}

auto resize(ResizeDirection direction, i32 amount_in_cells) -> Action {
    return {
        .description =
            *di::present("{} the current active pane's {} border by {} terminal cells"_sv,
                         amount_in_cells > 0 ? "Extend"_sv : "Shrink"_sv, direction, di::abs(amount_in_cells)),
        .apply =
            [direction, amount_in_cells](ActionContext const& context) {
                context.layout_state.with_lock([&](LayoutState& state) {
                    auto tab = state.active_tab();
                    if (!tab) {
                        return;
                    }
                    auto layout = tab.value().layout_tree();
                    if (!layout) {
                        return;
                    }
                    auto pane = tab.value().active();
                    if (!pane) {
                        return;
                    }
                    auto need_relayout =
                        tab.value().layout_group().resize(*layout, pane.data(), direction, amount_in_cells);
                    if (need_relayout) {
                        state.layout();
                    }
                });
                context.render_thread.request_render();
            },
    };
}

auto create_tab() -> Action {
    return {
        .description = "Create a new tab"_s,
        .apply =
            [](ActionContext const& context) {
                context.layout_state.with_lock([&](LayoutState& state) {
                    (void) state.add_tab({ .command = di::clone(context.command) }, context.render_thread);
                });
                context.render_thread.request_render();
            },
    };
}

auto switch_tab(usize index) -> Action {
    ASSERT_GT(index, 0);
    return {
        .description = *di::present("Switch to tab {} (1 indexed)"_sv, index),
        .apply =
            [index](ActionContext const& context) {
                context.layout_state.with_lock([&](LayoutState& state) {
                    if (auto tab = state.tabs().at(index - 1)) {
                        state.set_active_tab(tab.value().get());
                    }
                });
                context.render_thread.request_render();
            },
    };
}

auto quit() -> Action {
    return {
        .description = "Quit ttx"_s,
        .apply =
            [](ActionContext const& context) {
                context.done.store(true, di::MemoryOrder::Release);
            },
    };
}

auto save_state(di::Path path) -> Action {
    return {
        .description = *di::present("Save the state of the active pane to a file ({})"_sv, path),
        .apply =
            di::make_function<void(ActionContext const&) const&>([path = di::move(path)](ActionContext const& context) {
                context.layout_state.with_lock([&](LayoutState& state) {
                    if (auto pane = state.active_pane()) {
                        auto file = dius::open_sync(path, dius::OpenMode::WriteNew);
                        if (!file) {
                            // TODO: signal error to user!
                            return;
                        }

                        auto contents = pane->state_as_escape_sequences();
                        (void) file.value().write_exactly(di::as_bytes(contents.span()));
                    }
                });
            }),
    };
}

auto stop_capture() -> Action {
    return {
        .description = "Stop input capture for the active pane"_s,
        .apply =
            [](ActionContext const& context) {
                context.layout_state.with_lock([&](LayoutState& state) {
                    if (auto pane = state.active_pane()) {
                        pane->stop_capture();
                    }
                });
            },
    };
}

auto exit_pane() -> Action {
    return {
        .description = "Exit the active pane"_s,
        .apply =
            [](ActionContext const& context) {
                context.layout_state.with_lock([&](LayoutState& state) {
                    if (auto pane = state.active_pane()) {
                        pane->exit();
                    }
                });
                context.render_thread.request_render();
            },
    };
}

auto add_pane(Direction direction) -> Action {
    return {
        .description = *di::present("Add a new pane, in a {} position relative to the active pane"_sv, direction),
        .apply =
            [direction](ActionContext const& context) {
                context.layout_state.with_lock([&](LayoutState& state) {
                    if (!state.active_tab()) {
                        return;
                    }
                    (void) state.add_pane(*state.active_tab(), { .command = di::clone(context.command) }, direction,
                                          context.render_thread);
                });
                context.render_thread.request_render();
            },
    };
}

auto scroll(Direction direction, i32 amount_in_cells) -> Action {
    auto direction_name = [&] {
        switch (direction) {
            case Direction::Horizontal:
                return amount_in_cells > 0 ? "right"_sv : "left"_sv;
            case Direction::Vertical:
                return amount_in_cells > 0 ? "down"_sv : "up"_sv;
            case Direction::None:
                break;
        }
        return "nowhere"_sv;
    }();
    return {
        .description = *di::present("Scroll active pane {} by {} cells"_sv, direction_name, di::abs(amount_in_cells)),
        .apply =
            [direction, amount_in_cells](ActionContext const& context) {
                context.layout_state.with_lock([&](LayoutState& state) {
                    if (auto pane = state.active_pane()) {
                        pane->scroll(direction, amount_in_cells);
                    }
                });
                context.render_thread.request_render();
            },
    };
}

auto send_to_pane() -> Action {
    // NOTE: we need to hold the layout state lock the entire time
    // to prevent the Pane object from being prematurely destroyed.
    return {
        .description = "Default action - forward the key press to the active pane"_s,
        .apply =
            [](ActionContext const& context) {
                context.layout_state.with_lock([&](LayoutState& state) {
                    if (auto pane = state.active_pane()) {
                        pane->event(context.key_event);
                    }
                });
            },
    };
}
}
