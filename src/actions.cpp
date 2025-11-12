#include "actions.h"

#include "action.h"
#include "di/format/prelude.h"
#include "di/util/construct.h"
#include "fzf.h"
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
                        state.layout_did_update();
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
                    for (auto& session : state.active_session()) {
                        auto cwd = session.active_pane().and_then(&Pane::current_working_directory);
                        (void) state.add_tab(session, context.create_pane_args.with_cwd(cwd.transform(di::to_owned)),
                                             context.render_thread);
                    }
                });
                context.render_thread.request_render();
            },
    };
}

auto rename_tab() -> Action {
    return {
        .description = "Rename the current active tab"_s,
        .apply =
            [](ActionContext const& context) {
                context.layout_state.with_lock([&](LayoutState& state) {
                    for (auto& session : state.active_session()) {
                        if (!session.active_tab()) {
                            return;
                        }
                        auto& tab = session.active_tab().value();

                        auto [create_pane_args, popup_layout] = Fzf()
                                                                    .as_text_box()
                                                                    .with_title("Rename Tab"_s)
                                                                    .with_prompt("Name"_s)
                                                                    .with_query(tab.name().to_owned())
                                                                    .popup_args(context.create_pane_args.clone());
                        create_pane_args.hooks.did_finish_output = di::make_function<void(di::StringView)>(
                            [&layout_state = context.layout_state, &tab,
                             &render_thread = context.render_thread](di::StringView contents) {
                                while (contents.ends_with(U'\n')) {
                                    contents = contents.substr(contents.begin(), --contents.end());
                                }
                                if (contents.empty()) {
                                    return;
                                }
                                layout_state.with_lock([&](LayoutState&) {
                                    // NOTE: we take the layout state lock to prevent data races. Also, the tab is
                                    // guaranteed to still be alive because tabs won't be killed until all their panes
                                    // have exited. And we put the popup in the tab.
                                    tab.set_name(contents.to_owned());
                                });
                                render_thread.request_render();
                            });
                        if (auto tab = state.active_tab()) {
                            (void) state.popup_pane(session, tab.value(), popup_layout, di::move(create_pane_args),
                                                    context.render_thread);
                        }
                    }
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
                    for (auto& session : state.active_session()) {
                        if (auto tab = session.tabs().at(index - 1)) {
                            state.set_active_tab(session, tab.value().get());
                        }
                    }
                });
                context.render_thread.request_render();
            },
    };
}

auto switch_next_tab() -> Action {
    return {
        .description = "Switch to the next tab by numeric index"_s,
        .apply =
            [](ActionContext const& context) {
                context.layout_state.with_lock([&](LayoutState& state) {
                    for (auto& session : state.active_session()) {
                        for (auto& tab : session.active_tab()) {
                            auto tabs = session.tabs() | di::transform(&di::Box<Tab>::get);
                            auto it = di::find(tabs, &tab);
                            if (it == tabs.end()) {
                                return;
                            }
                            auto index = usize(it - tabs.begin());
                            index++;
                            index %= tabs.size();
                            state.set_active_tab(session, tabs[isize(index)]);
                        }
                    }
                });
                context.render_thread.request_render();
            },
    };
}

auto switch_prev_tab() -> Action {
    return {
        .description = "Switch to the previous tab by numeric index"_s,
        .apply =
            [](ActionContext const& context) {
                context.layout_state.with_lock([&](LayoutState& state) {
                    for (auto& session : state.active_session()) {
                        for (auto& tab : session.active_tab()) {
                            auto tabs = session.tabs() | di::transform(&di::Box<Tab>::get);
                            auto it = di::find(tabs, &tab);
                            if (it == tabs.end()) {
                                return;
                            }
                            auto index = usize(it - tabs.begin());
                            index += tabs.size();
                            index--;
                            index %= tabs.size();
                            state.set_active_tab(session, tabs[isize(index)]);
                        }
                    }
                });
                context.render_thread.request_render();
            },
    };
}

auto find_tab() -> Action {
    return {
        .description = "Find a tab in the current session by name using fzf"_s,
        .apply =
            [](ActionContext const& context) {
                context.layout_state.with_lock([&](LayoutState& state) {
                    auto tab_names = di::Vector<di::String>();
                    if (auto session = state.active_session()) {
                        for (auto [i, tab] : session.value().tabs() | di::enumerate) {
                            tab_names.push_back(*di::present("{} {}"_sv, i + 1, tab->name()));
                        }

                        auto [create_pane_args, popup_layout] = Fzf()
                                                                    .with_prompt("Switch to tab"_s)
                                                                    .with_title("Tabs"_s)
                                                                    .with_input(di::move(tab_names))
                                                                    .popup_args(context.create_pane_args.clone());
                        create_pane_args.hooks.did_finish_output = di::make_function<void(di::StringView)>(
                            [&layout_state = context.layout_state, &render_thread = context.render_thread,
                             &session](di::StringView contents) {
                                // Try to parse the first index. This will fail if contents is empty.
                                auto maybe_tab_index = di::parse_partial<usize>(contents);
                                if (!maybe_tab_index || maybe_tab_index == 0) {
                                    return;
                                }
                                auto tab_index = maybe_tab_index.value() - 1;
                                layout_state.with_lock([&](LayoutState& state) {
                                    if (auto tab = session.value().tabs().at(tab_index)) {
                                        state.set_active_tab(session.value(), tab.value().get());
                                    }
                                });
                                render_thread.request_render();
                            });
                        if (auto tab = state.active_tab()) {
                            (void) state.popup_pane(session.value(), tab.value(), popup_layout,
                                                    di::move(create_pane_args), context.render_thread);
                        }
                    }
                });
                context.render_thread.request_render();
            },
    };
}

auto create_session() -> Action {
    return {
        .description = "Create a new session"_s,
        .apply =
            [](ActionContext const& context) {
                context.layout_state.with_lock([&](LayoutState& state) {
                    (void) state.add_session(context.create_pane_args.clone(), context.render_thread);
                });
                context.render_thread.request_render();
            },
    };
}

auto rename_session() -> Action {
    return {
        .description = "Rename the current active session"_s,
        .apply =
            [](ActionContext const& context) {
                context.layout_state.with_lock([&](LayoutState& state) {
                    for (auto& session : state.active_session()) {
                        auto [create_pane_args, popup_layout] = Fzf()
                                                                    .as_text_box()
                                                                    .with_title("Rename Session"_s)
                                                                    .with_prompt("Name"_s)
                                                                    .with_query(session.name().to_owned())
                                                                    .popup_args(context.create_pane_args.clone());
                        create_pane_args.hooks.did_finish_output = di::make_function<void(di::StringView)>(
                            [&layout_state = context.layout_state, &session,
                             &render_thread = context.render_thread](di::StringView contents) {
                                while (contents.ends_with(U'\n')) {
                                    contents = contents.substr(contents.begin(), --contents.end());
                                }
                                if (contents.empty()) {
                                    return;
                                }
                                layout_state.with_lock([&](LayoutState&) {
                                    // NOTE: we take the layout state lock to prevent data races. Also, the session is
                                    // guaranteed to still be alive because sessions won't be killed until all their
                                    // panes have exited. And we put the popup in the session.
                                    session.set_name(contents.to_owned());
                                });
                                render_thread.request_render();
                            });
                        if (auto tab = state.active_tab()) {
                            (void) state.popup_pane(session, tab.value(), popup_layout, di::move(create_pane_args),
                                                    context.render_thread);
                        }
                    }
                });
                context.render_thread.request_render();
            },
    };
}

auto switch_next_session() -> Action {
    return {
        .description = "Switch to the next session by creation order"_s,
        .apply =
            [](ActionContext const& context) {
                context.layout_state.with_lock([&](LayoutState& state) {
                    for (auto& session : state.active_session()) {
                        auto sessions = state.sessions() | di::transform([](di::Box<Session>& session) {
                                            return session.get();
                                        });
                        auto it = di::find(sessions, &session);
                        if (it == sessions.end()) {
                            return;
                        }
                        auto index = usize(it - sessions.begin());
                        index++;
                        index %= sessions.size();
                        state.set_active_session(sessions[isize(index)]);
                    }
                });
                context.render_thread.request_render();
            },
    };
}

auto switch_prev_session() -> Action {
    return {
        .description = "Switch to the previous session by creation order"_s,
        .apply =
            [](ActionContext const& context) {
                context.layout_state.with_lock([&](LayoutState& state) {
                    for (auto& session : state.active_session()) {
                        auto sessions = state.sessions() | di::transform([](di::Box<Session>& session) {
                                            return session.get();
                                        });
                        auto it = di::find(sessions, &session);
                        if (it == sessions.end()) {
                            return;
                        }
                        auto index = usize(it - sessions.begin());
                        index += sessions.size();
                        index--;
                        index %= sessions.size();
                        state.set_active_session(sessions[isize(index)]);
                    }
                });
                context.render_thread.request_render();
            },
    };
}

auto find_session() -> Action {
    return {
        .description = "Find a session by name using fzf"_s,
        .apply =
            [](ActionContext const& context) {
                context.layout_state.with_lock([&](LayoutState& state) {
                    auto session_names = di::Vector<di::String>();
                    for (auto [i, session] : state.sessions() | di::enumerate) {
                        session_names.push_back(*di::present("{} {}"_sv, i + 1, session->name()));
                    }

                    auto [create_pane_args, popup_layout] = Fzf()
                                                                .with_prompt("Switch to session"_s)
                                                                .with_title("Sessions"_s)
                                                                .with_input(di::move(session_names))
                                                                .popup_args(context.create_pane_args.clone());
                    create_pane_args.hooks.did_finish_output = di::make_function<void(di::StringView)>(
                        [&layout_state = context.layout_state,
                         &render_thread = context.render_thread](di::StringView contents) {
                            // Try to parse the first index. This will fail if contents is empty.
                            auto maybe_session_index = di::parse_partial<usize>(contents);
                            if (!maybe_session_index || maybe_session_index == 0) {
                                return;
                            }
                            auto session_index = maybe_session_index.value() - 1;
                            layout_state.with_lock([&](LayoutState& state) {
                                if (auto session = state.sessions().at(session_index)) {
                                    state.set_active_session(session.value().get());
                                }
                            });
                            render_thread.request_render();
                        });
                    if (auto session = state.active_session()) {
                        if (auto tab = state.active_tab()) {
                            (void) state.popup_pane(session.value(), tab.value(), popup_layout,
                                                    di::move(create_pane_args), context.render_thread);
                        }
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

auto save_layout() -> Action {
    return {
        .description = "Create a manual layout save"_s,
        .apply =
            [](ActionContext const& context) {
                auto [create_pane_args, popup_layout] = Fzf()
                                                            .as_text_box()
                                                            .with_title("Save Layout To File"_s)
                                                            .with_prompt("Name"_s)
                                                            .popup_args(context.create_pane_args.clone());
                create_pane_args.hooks.did_finish_output = di::make_function<void(di::StringView)>(
                    [&save_layout_thread = context.save_layout_thread](di::StringView contents) {
                        while (contents.ends_with(U'\n')) {
                            contents = contents.substr(contents.begin(), --contents.end());
                        }
                        if (contents.empty()) {
                            return;
                        }
                        auto raw_string =
                            contents.span() | di::transform(di::construct<char>) | di::to<di::TransparentString>();
                        save_layout_thread.request_save_layout(di::move(raw_string));
                    });
                context.layout_state.with_lock([&](LayoutState& state) {
                    if (auto session = state.active_session()) {
                        if (auto tab = state.active_tab()) {
                            (void) state.popup_pane(session.value(), tab.value(), popup_layout,
                                                    di::move(create_pane_args), context.render_thread);
                        }
                    }
                });
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
                        // TODO: signal error to user!
                        (void) pane->save_state(path);
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

auto soft_reset() -> Action {
    return {
        .description = "Soft reset the active pane"_s,
        .apply =
            [](ActionContext const& context) {
                context.layout_state.with_lock([&](LayoutState& state) {
                    if (auto pane = state.active_pane()) {
                        pane->soft_reset();
                    }
                });
                context.render_thread.request_render();
            },
    };
}

auto hard_reset() -> Action {
    return {
        .description = "Hard reset the active pane"_s,
        .apply =
            [](ActionContext const& context) {
                context.layout_state.with_lock([&](LayoutState& state) {
                    for (auto& tab : state.active_tab()) {
                        for (auto& pane : tab.active()) {
                            (void) tab.replace_pane(pane,
                                                    context.create_pane_args.with_cwd(
                                                        pane.current_working_directory().transform(di::to_owned)),
                                                    context.render_thread);
                        }
                    }
                });
                context.render_thread.request_render();
            },
    };
}

auto toggle_full_screen_pane() -> Action {
    return {
        .description = "Toggle full screen for the active pane"_s,
        .apply =
            [](ActionContext const& context) {
                context.layout_state.with_lock([&](LayoutState& state) {
                    for (auto& pane : state.active_pane()) {
                        auto& tab = *state.active_tab();
                        if (&pane == state.full_screen_pane().data()) {
                            tab.set_full_screen_pane(nullptr);
                        } else {
                            tab.set_full_screen_pane(&pane);
                        }
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
                    for (auto& session : state.active_session()) {
                        for (auto& tab : session.active_tab()) {
                            auto cwd = tab.active().and_then(&Pane::current_working_directory);
                            (void) state.add_pane(session, tab,
                                                  context.create_pane_args.with_cwd(cwd.transform(di::to_owned)),
                                                  direction, context.render_thread);
                        }
                    }
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

auto scroll_prev_command() -> Action {
    return {
        .description = "Scroll up to the start of the previous shell command"_s,
        .apply =
            [](ActionContext const& context) {
                context.layout_state.with_lock([&](LayoutState& state) {
                    if (auto pane = state.active_pane()) {
                        pane->scroll_prev_command();
                    }
                });
            },
    };
}

auto scroll_next_command() -> Action {
    return {
        .description = "Scroll down to the start of the next shell command"_s,
        .apply =
            [](ActionContext const& context) {
                context.layout_state.with_lock([&](LayoutState& state) {
                    if (auto pane = state.active_pane()) {
                        pane->scroll_next_command();
                    }
                });
            },
    };
}

auto copy_last_command(bool include_command) -> Action {
    return {
        .description = *di::present("Copy text from latest command (include command text = {})"_sv, include_command),
        .apply =
            [include_command](ActionContext const& context) {
                context.layout_state.with_lock([&](LayoutState& state) {
                    if (auto pane = state.active_pane()) {
                        pane->copy_last_command(include_command);
                    }
                });
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
