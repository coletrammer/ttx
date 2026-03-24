#include "actions.h"

#include "action.h"
#include "config_json.h"
#include "di/container/string/conversion.h"
#include "di/format/prelude.h"
#include "di/util/construct.h"
#include "fzf.h"
#include "input.h"
#include "render.h"
#include "tab.h"
#include "theme.h"
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

auto navigate(terminal::NavigateDirection direction) -> Action {
    return {
        .description = di::format(
            "Navigate to the first pane in direction ({}) starting from the current active pane"_sv, direction),
        .apply =
            [direction](ActionContext const& context) {
                context.input_thread.request_navigate(direction);
            },
    };
}

auto resize(ResizeDirection direction, i32 amount_in_cells) -> Action {
    return {
        .description = di::format("{} the current active pane's {} border by {} terminal cells"_sv,
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
                                             context.render_thread, context.input_thread);
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

                        auto [create_pane_args, popup_layout] =
                            FzfCommand()
                                .as_text_box()
                                .with_title("Rename Tab"_s)
                                .with_prompt("Name"_s)
                                .with_query(tab.name().to_owned())
                                .popup_args(context.create_pane_args.clone(), context.config.fzf);
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
                                                    context.render_thread, context.input_thread);
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
        .description = di::format("Switch to tab {} (1 indexed)"_sv, index),
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
                            tab_names.push_back(di::format("{} {}"_sv, i + 1, tab->name()));
                        }

                        auto [create_pane_args, popup_layout] =
                            FzfCommand()
                                .with_prompt("Switch to tab"_s)
                                .with_title("Tabs"_s)
                                .with_input(di::move(tab_names))
                                .popup_args(context.create_pane_args.clone(), context.config.fzf);
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
                                                    di::move(create_pane_args), context.render_thread,
                                                    context.input_thread);
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
                    (void) state.add_session(context.create_pane_args.clone(), context.render_thread,
                                             context.input_thread);
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
                        auto [create_pane_args, popup_layout] =
                            FzfCommand()
                                .as_text_box()
                                .with_title("Rename Session"_s)
                                .with_prompt("Name"_s)
                                .with_query(session.name().to_owned())
                                .popup_args(context.create_pane_args.clone(), context.config.fzf);
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
                                                    context.render_thread, context.input_thread);
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
                        session_names.push_back(di::format("{} {}"_sv, i + 1, session->name()));
                    }

                    auto [create_pane_args, popup_layout] =
                        FzfCommand()
                            .with_prompt("Switch to session"_s)
                            .with_title("Sessions"_s)
                            .with_input(di::move(session_names))
                            .popup_args(context.create_pane_args.clone(), context.config.fzf);
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
                                                    di::move(create_pane_args), context.render_thread,
                                                    context.input_thread);
                        }
                    }
                });
                context.render_thread.request_render();
            },
    };
}

auto reload_config() -> Action {
    return {
        .description = "Reload configuration files"_s,
        .apply =
            [](ActionContext const& context) {
                auto new_config = config_json::v1::resolve_profile(context.profile, di::clone(context.base_config));
                if (!new_config) {
                    context.render_thread.status_message(
                        di::format("Failed to load configuration: {}"_sv, new_config.error()));
                    return;
                }

                context.input_thread.set_config(di::clone(new_config.value()));
                context.render_thread.set_config(di::clone(new_config.value()));
                context.save_layout_thread.set_config(di::clone(new_config.value().session));
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
                auto [create_pane_args, popup_layout] =
                    FzfCommand()
                        .as_text_box()
                        .with_title("Save Layout To File"_s)
                        .with_prompt("Name"_s)
                        .popup_args(context.create_pane_args.clone(), context.config.fzf);
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
                                                    di::move(create_pane_args), context.render_thread,
                                                    context.input_thread);
                        }
                    }
                });
            },
    };
}

auto save_state(di::Path path) -> Action {
    return {
        .description = di::format("Save the state of the active pane to a file ({})"_sv, path),
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
                                                    context.render_thread, context.input_thread);
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
        .description = di::format("Add a new pane, in a {} position relative to the active pane"_sv, direction),
        .apply =
            [direction](ActionContext const& context) {
                context.layout_state.with_lock([&](LayoutState& state) {
                    for (auto& session : state.active_session()) {
                        for (auto& tab : session.active_tab()) {
                            auto cwd = tab.active().and_then(&Pane::current_working_directory);
                            (void) state.add_pane(session, tab,
                                                  context.create_pane_args.with_cwd(cwd.transform(di::to_owned)),
                                                  direction, context.render_thread, context.input_thread);
                        }
                    }
                });
                context.render_thread.request_render();
            },
    };
}

static void scroll_action(ActionContext const& context, di::concepts::Invocable<Pane&> auto do_scroll) {
    auto const did_scroll = context.layout_state.with_lock([&](LayoutState& state) -> bool {
        if (auto pane = state.active_pane()) {
            // Its important to not try to scroll pane's in the alternate screen buffer
            // (which are running a fullscreen application with no scroll back buffer).
            // By falling back to forwarding the key event we ensure the inner application
            // can do its own scrolling.
            if (pane->accepts_scrolling()) {
                di::invoke(do_scroll, *pane);
                return true;
            }
            pane->event(context.key_event);
        }
        return false;
    });
    if (did_scroll) {
        context.render_thread.request_render();
    }
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
        .description = di::format("Scroll active pane {} by {} cells"_sv, direction_name, di::abs(amount_in_cells)),
        .apply =
            [direction, amount_in_cells](ActionContext const& context) {
                scroll_action(context, di::bind_back(&Pane::scroll, direction, amount_in_cells));
            },
    };
}

auto scroll_page_up() -> Action {
    return {
        .description = "Scroll active pane up a page"_s,
        .apply =
            [](ActionContext const& context) {
                scroll_action(context, &Pane::scroll_page_up);
            },
    };
}

auto scroll_page_down() -> Action {
    return {
        .description = "Scroll active pane up a page"_s,
        .apply =
            [](ActionContext const& context) {
                scroll_action(context, &Pane::scroll_page_down);
            },
    };
}

auto scroll_to_top() -> Action {
    return {
        .description = "Scroll active pane to the top of the scroll back buffer"_s,
        .apply =
            [](ActionContext const& context) {
                scroll_action(context, &Pane::scroll_to_top);
            },
    };
}

auto scroll_to_bottom() -> Action {
    return {
        .description = "Scroll active pane to the bottom of the scroll back buffer"_s,
        .apply =
            [](ActionContext const& context) {
                scroll_action(context, &Pane::scroll_to_bottom);
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
                context.render_thread.request_render();
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
                context.render_thread.request_render();
            },
    };
}

auto copy_last_command(bool include_command) -> Action {
    return {
        .description = di::format("Copy text from latest command (include command text = {})"_sv, include_command),
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

auto switch_theme() -> Action {
    return {
        .description = "Switch the current theme using fzf"_s,
        .apply =
            [](ActionContext const& context) {
                auto themes = config_json::v1::list_themes(ThemeSource::All);
                if (!themes) {
                    context.render_thread.status_message(di::format("Failed to list themes: {}"_sv, themes.error()));
                }
                auto original_theme = di::clone(context.config.theme.name);
                auto theme_names = di::Vector<di::String> {};
                theme_names.push_back(di::format("{}"_sv, original_theme));
                for (auto const& [name, source] : themes.value()) {
                    if (name != context.config.theme.name) {
                        theme_names.push_back(di::format("{}"_sv, name));
                    }
                }

                context.layout_state.with_lock([&](LayoutState& state) {
                    if (auto session = state.active_session()) {
                        if (auto tab = state.active_tab()) {
                            auto [create_pane_args, popup_layout] =
                                FzfCommand()
                                    .with_prompt("Switch to theme"_s)
                                    .with_title("Themes"_s)
                                    .with_input(di::move(theme_names))
                                    .with_selection_as_extra_output()
                                    .with_indirect_color_palette()
                                    .with_preview_command(
                                        "echo -e '\n\n\t\x1b[1mANSI Colors (0-7)\x1b[m\n\n\t\x1b[40m  \x1b[41m  "
                                        "\x1b[42m  \x1b[43m  \x1b[44m  \x1b[45m  \x1b[46m  \x1b[47m  \x1b[m\n\n"
                                        "\t\x1b[1mANSI Colors (8-15)\x1b[m\n\n\t\x1b[100m  \x1b[101m  \x1b[102m  \x1b[103m  \x1b[104m  \x1b[105m  \x1b[106m  \x1b[107m  \x1b[m\n\n'"_ts)
                                    .popup_args(context.create_pane_args.clone(), context.config.fzf);
                            create_pane_args.hooks.did_finish_output = di::make_function<void(di::StringView)>(
                                [&input_thread = context.input_thread, &render_thread = context.render_thread,
                                 original_theme = di::move(original_theme)](di::StringView name) {
                                    auto name_ts = di::to_transparent_string(name);
                                    while (name_ts.back() == '\n' || name_ts.back() == ' ') {
                                        name_ts.pop_back();
                                    }
                                    if (name_ts.empty()) {
                                        name_ts = original_theme.clone();
                                    }
                                    auto base_config = di::clone(input_thread.base_config());
                                    base_config.theme.name = di::to_utf8_string_lossy(name_ts);
                                    auto new_config =
                                        config_json::v1::resolve_profile(input_thread.profile(), di::move(base_config));
                                    if (!new_config) {
                                        render_thread.status_message(
                                            di::format("Failed to load configuration: {}"_sv, new_config.error()));
                                        return;
                                    }

                                    input_thread.set_config(di::clone(new_config.value()));
                                    render_thread.set_config(di::clone(new_config.value()));
                                });
                            create_pane_args.hooks.did_get_extra_output = di::make_function<void(di::StringView)>(
                                [&input_thread = context.input_thread, &render_thread = context.render_thread,
                                 original_theme = di::clone(context.config.theme.name),
                                 &layout_state = context.layout_state](di::StringView name) {
                                    auto name_ts = di::to_transparent_string(name);
                                    if (name_ts.empty()) {
                                        return;
                                    }
                                    auto base_config = di::clone(input_thread.base_config());
                                    base_config.theme.name = di::to_utf8_string_lossy(name_ts);
                                    auto new_config =
                                        config_json::v1::resolve_profile(input_thread.profile(), di::move(base_config));
                                    if (!new_config) {
                                        render_thread.status_message(
                                            di::format("Failed to load configuration: {}"_sv, new_config.error()));
                                        return;
                                    }

                                    layout_state.with_lock([&](LayoutState& state) {
                                        for (auto& pane : state.active_popup()) {
                                            pane.update_palette([&](terminal::Palette& palette) {
                                                palette = new_config.value().colors;
                                                FzfCommand::update_palette_with_indirect_fzf_colors(
                                                    palette, new_config.value().fzf.colors);
                                            });
                                        }
                                    });

                                    input_thread.set_config(di::clone(new_config.value()));
                                    render_thread.set_config(di::clone(new_config.value()));
                                });
                            create_pane_args.palette = [&] {
                                auto palette = context.config.colors;
                                FzfCommand::update_palette_with_indirect_fzf_colors(palette, context.config.fzf.colors);
                                return palette;
                            }();
                            (void) state.popup_pane(session.value(), tab.value(), popup_layout,
                                                    di::move(create_pane_args), context.render_thread,
                                                    context.input_thread);
                        }
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
