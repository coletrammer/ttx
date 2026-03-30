#include "input.h"

#include "config.h"
#include "input_mode.h"
#include "key_bind.h"
#include "layout_state.h"
#include "render.h"
#include "save_layout.h"
#include "tab.h"
#include "ttx/features.h"
#include "ttx/focus_event.h"
#include "ttx/key_event.h"
#include "ttx/layout.h"
#include "ttx/modifiers.h"
#include "ttx/mouse.h"
#include "ttx/mouse_event.h"
#include "ttx/pane.h"
#include "ttx/paste_event.h"
#include "ttx/terminal/escapes/device_status.h"
#include "ttx/terminal/escapes/osc_52.h"
#include "ttx/terminal/escapes/osc_8671.h"
#include "ttx/terminal/navigation_direction.h"
#include "ttx/terminal_input.h"
#include "ttx/utf8_stream_decoder.h"

namespace ttx {
auto InputThread::create(CreatePaneArgs create_pane_args, Config config, config_json::v1::Config base_config,
                         di::TransparentStringView profile, di::Synchronized<LayoutState>& layout_state,
                         FeatureResult features, RenderThread& render_thread, SaveLayoutThread& save_layout_thread)
    -> di::Result<di::Box<InputThread>> {
    auto result = di::make_box<InputThread>(di::move(create_pane_args), di::move(config), di::move(base_config),
                                            profile, layout_state, features, render_thread, save_layout_thread);
    result->m_thread = TRY(dius::Thread::create([&self = *result.get()] {
        self.input_thread();
    }));
    return result;
}

auto InputThread::create_mock(di::Synchronized<LayoutState>& layout_state, RenderThread& render_thread,
                              SaveLayoutThread& save_layout_thread) -> di::Box<InputThread> {
    return di::make_box<InputThread>(CreatePaneArgs {}, Config {}, config_json::v1::Config {}, ""_tsv, layout_state,
                                     FeatureResult { Feature::All }, render_thread, save_layout_thread);
}

InputThread::InputThread(CreatePaneArgs create_pane_args, Config config, config_json::v1::Config base_config,
                         di::TransparentStringView profile, di::Synchronized<LayoutState>& layout_state,
                         FeatureResult features, RenderThread& render_thread, SaveLayoutThread& save_layout_thread)
    : m_config(di::move(config))
    , m_base_config(di::move(base_config))
    , m_profile(profile)
    , m_key_binds(make_key_binds(m_config.input, create_pane_args.replay_path.has_value()))
    , m_create_pane_args(di::move(create_pane_args))
    , m_layout_state(layout_state)
    , m_render_thread(render_thread)
    , m_save_layout_thread(save_layout_thread)
    , m_features(features.features)
    , m_outer_terminal_palette(features.palette)
    , m_rng(u32(dius::SteadyClock::now().time_since_epoch().count())) {}

InputThread::~InputThread() {
    request_exit();
    (void) m_thread.join();
}

void InputThread::request_exit() {
    if (!m_done.exchange(true, di::MemoryOrder::Release)) {
        // Ensure the input thread exits. (By requesting device attributes, thus waking up the input thread).
        // It would be better to use something else to cancel the input thread.
        (void) dius::std_in.write_exactly(di::as_bytes("\033[c"_sv.span()));
    }
}

void InputThread::set_input_mode(InputMode mode) {
    if (m_mode == mode) {
        return;
    }

    m_mode = mode;
    m_render_thread.push_event(InputStatus { .mode = mode });
}

void InputThread::set_config(Config config) {
    auto palette_did_change = config.colors != m_config.colors;
    m_config = di::move(config);
    m_key_binds = make_key_binds(m_config.input, m_create_pane_args.replay_path.has_value());
    m_create_pane_args.command = m_config.shell.command.clone();
    m_create_pane_args.save_state_path = m_config.input.save_state_path.clone();
    m_create_pane_args.term = m_config.terminfo.term.clone();

    if (palette_did_change) {
        auto global_palette = m_config.colors;
        for (auto index_number : di::range(u32(terminal::PaletteIndex::Count))) {
            auto index = terminal::PaletteIndex(index_number);
            if (global_palette.get(index).is_default()) {
                global_palette.set(index, m_outer_terminal_palette.get(index));
            }
        }

        m_layout_state.with_lock([&](LayoutState& state) {
            state.for_each_pane([&](Pane& pane) {
                pane.set_global_palette(global_palette);
            });
        });
        m_create_pane_args.global_palette = global_palette;
    }
}

void InputThread::input_thread() {
    auto _ = di::ScopeExit([&] {
        m_render_thread.request_exit();
        m_done.exchange(true, di::MemoryOrder::Release);
    });

    auto buffer = di::Vector<byte> {};
    buffer.resize(4096);

    auto parser = TerminalInputParser {};
    auto utf8_decoder = Utf8StreamDecoder {};
    while (!m_done.load(di::MemoryOrder::Acquire)) {
        auto nread = dius::std_in.read_some(buffer.span());
        if (!nread.has_value() || m_done.load(di::MemoryOrder::Acquire)) {
            return;
        }

        auto now = dius::SteadyClock::now();
        auto utf8_string = utf8_decoder.decode(buffer | di::take(*nread));
        auto events = parser.parse(utf8_string, m_features);
        m_pending_events.with_lock([&](auto& pending_events) {
            for (auto& event : events) {
                pending_events.push_back({
                    .event = di::move(event),
                    .receptionTime = now,
                });
            }
        });

        process_pending_events();
    }
}

void InputThread::process_pending_events() {
    constexpr auto timeout = di::Milliseconds(200);

    while (!m_done.load(di::MemoryOrder::Acquire)) {
        auto first = m_pending_events.lock()->pop_front();
        if (!first) {
            break;
        }

        // Check for timeout.
        if (first->pending && dius::SteadyClock::now() > first->receptionTime + timeout) {
            di::visit(
                [&](auto& x) {
                    if constexpr (requires { this->handle_event(x, true); }) {
                        return this->handle_event(x, true);
                    }
                },
                first->event);
            continue;
        }

        // If the event is still processing, just wait for now.
        if (first->pending) {
            m_pending_events.lock()->push_back(di::move(first).value());
            break;
        }

        auto was_processed = di::visit(
            [&](auto& x) {
                if constexpr (requires { this->handle_event(x, false); }) {
                    return this->handle_event(x, false);
                } else {
                    this->handle_event(di::move(x));
                    return true;
                }
            },
            first->event);
        if (!was_processed) {
            // Add the event back in but mark it as pending.
            first->pending = true;
            m_pending_events.lock()->push_back(di::move(first).value());
            break;
        }
    }
}

void InputThread::handle_event(KeyEvent&& event) {
    // Popup panes swallow all key events.
    if (m_layout_state.with_lock([&](LayoutState& state) -> bool {
            for (auto& pane : state.active_popup()) {
                pane.event(event);
                return true;
            }
            return false;
        })) {
        return;
    }

    for (auto const& bind : m_key_binds) {
        // Ignore key up events and modifier keys when not in insert mode.
        if (m_mode != InputMode::Insert && (event.type() == KeyEventType::Release ||
                                            (event.key() > Key::ModifiersBegin && event.key() < Key::ModifiersEnd))) {
            continue;
        }

        auto modifiers = event.modifiers() & ~(Modifiers::LockModifiers);
        auto key_matches = bind.key == Key::None || (event.type() != KeyEventType::Release && event.key() == bind.key &&
                                                     modifiers == bind.modifiers);
        if (m_mode == bind.mode && key_matches) {
            bind.action.apply({
                .key_event = event,
                .layout_state = m_layout_state,
                .render_thread = m_render_thread,
                .save_layout_thread = m_save_layout_thread,
                .input_thread = *this,
                .create_pane_args = m_create_pane_args,
                .config = m_config,
                .base_config = m_base_config,
                .profile = m_profile,
                .done = m_done,
            });
            set_input_mode(bind.next_mode);
            return;
        }
    }
}

void InputThread::handle_event(MouseEvent&& event) {
    m_layout_state.with_lock([&](LayoutState& state) {
        // Check if the event intersects with the status bar.
        if (!state.hide_status_bar() && event.position().in_cells().y() == state.status_bar_position()) {
            m_render_thread.push_event(event);
            return;
        }

        // Check if we should clear the drag origin (we got anything other than a mouse move with left held).
        if (event.type() != MouseEventType::Move || event.button() != MouseButton::Left) {
            m_drag_origin = {};
        }

        if (!state.active_tab()) {
            return;
        }
        auto& tab = *state.active_tab();

        auto ev = event.translate({ 0, u32(-(state.status_bar_position() == 0)) }, state.size());

        // Check if we're hitting any popup with the mouse.
        auto row = event.position().in_cells().y();
        auto col = event.position().in_cells().x();
        for (auto entry : state.popup_layout()) {
            if (row >= entry.row && row < entry.row + entry.size.rows && col >= entry.col &&
                col < entry.col + entry.size.cols) {
                if (ev.type() != MouseEventType::Move) {
                    tab.set_active(entry.pane);
                }
                if (entry.pane->event(ev.translate({ -entry.col, -entry.row }, state.size()))) {
                    m_render_thread.request_render();
                }
                return;
            }
        }

        // Check if the user is dragging the pane edge.
        if (m_drag_origin) {
            // The intended amount the user wants to move is determined by the amount of motion between the drag
            // origin and the current position. Each mouse event is consolidated into single ticks where the cursor
            // only moves by 1 cell at a time.
            auto current_position = m_drag_origin.value();
            auto end_position = ev.position().in_cells();

            auto do_drag = [&](MouseCoordinate const& coordinate) {
                if (handle_drag(state, coordinate)) {
                    state.layout();
                    state.layout_did_update();
                }
            };

            while (current_position.y() < end_position.y()) {
                current_position = { current_position.x(), current_position.y() + 1 };
                do_drag(current_position);
            }
            while (current_position.y() > end_position.y()) {
                current_position = { current_position.x(), current_position.y() - 1 };
                do_drag(current_position);
            }
            while (current_position.x() < end_position.x()) {
                current_position = { current_position.x() + 1, current_position.y() };
                do_drag(current_position);
            }
            while (current_position.x() > end_position.x()) {
                current_position = { current_position.x() - 1, current_position.y() };
                do_drag(current_position);
            }
            ASSERT_EQ(current_position, end_position);
            return;
        }

        // Check if the event interests with any pane.
        for (auto const& entry :
             tab.layout_tree()->hit_test(ev.position().in_cells().y(), ev.position().in_cells().x())) {
            if (ev.type() != MouseEventType::Move) {
                // Set the pane the user just clicked on as active.
                tab.set_active(entry.pane);
                // If we had a popup, exit it as the user clicked out.
                for (auto popup_entry : state.popup_layout()) {
                    popup_entry.pane->exit();
                }
            }
            if (entry.pane == tab.active().data()) {
                if (entry.pane->event(ev.translate({ -entry.col, -entry.row }, state.size()))) {
                    m_render_thread.request_render();
                }
            }
            return;
        }

        // Check if the user is attempting to drag an edge.
        if (ev.type() == MouseEventType::Press && ev.button() == MouseButton::Left) {
            m_drag_origin = ev.position().in_cells();
        }
    });
}

void InputThread::handle_event(terminal::DarkLightModeDetectionReport&& report) {
    if (report.mode != m_create_pane_args.theme_mode) {
        m_create_pane_args.theme_mode = report.mode;
        m_layout_state.with_lock([&](LayoutState& state) {
            state.for_each_pane([&](Pane& pane) {
                pane.set_theme_mode(report.mode);
            });
        });
        m_force_reload_config_on_osc21 = true;
    }

    m_render_thread.push_event(QueryPalette {});
}

void InputThread::handle_event(terminal::OSC21&& osc21) {
    auto did_change = false;
    for (auto const& item : osc21.requests) {
        if (m_create_pane_args.global_palette.get(item.palette) != item.color) {
            m_outer_terminal_palette.set(item.palette, item.color);
            did_change = true;
        }
    }
    if (did_change || m_force_reload_config_on_osc21) {
        // The global palette, which is used for resolving palette queries, is determined
        // by merging our configured palette with the outer terminal's actual colors. When
        // rendering, we do not use the resolved palette colors, so this is only used for
        // replying to application requests to fetch the value of terminal colors. This also
        // forwards theme change events from the outer terminal to inner applications.
        auto global_palette = m_config.colors;
        for (auto index_number : di::range(u32(terminal::PaletteIndex::Count))) {
            auto index = terminal::PaletteIndex(index_number);
            if (global_palette.get(index).is_default()) {
                global_palette.set(index, m_outer_terminal_palette.get(index));
            }
        }

        m_layout_state.with_lock([&](LayoutState& state) {
            state.for_each_pane([&](Pane& pane) {
                pane.set_global_palette(global_palette);
            });
        });
        m_create_pane_args.global_palette = global_palette;
        m_render_thread.push_event(UpdateOuterTerminalPalette(m_outer_terminal_palette));

        m_force_reload_config_on_osc21 = false;

        // Reload configuration, as the "auto" theme may resolve differently now, as will the "dark" and
        // "light" specific theme configuration.
        auto new_config = config_json::v1::resolve_profile(m_profile, m_create_pane_args.theme_mode,
                                                           m_outer_terminal_palette, di::clone(m_base_config));
        if (!new_config) {
            m_render_thread.status_message(di::format("Failed to load configuration: {}"_sv, new_config.error()));
            return;
        }

        set_config(di::clone(new_config.value()));
        m_render_thread.set_config(di::clone(new_config.value()));
        m_save_layout_thread.set_config(di::clone(new_config.value().session));
    }
}

void InputThread::handle_event(FocusEvent&& event) {
    m_layout_state.with_lock([&](LayoutState& state) {
        if (auto pane = state.active_pane()) {
            pane->event(event);
        }
    });
}

void InputThread::handle_event(PasteEvent&& event) {
    m_layout_state.with_lock([&](LayoutState& state) {
        if (auto pane = state.active_pane()) {
            pane->event(event);
        }
    });
}

void InputThread::handle_event(terminal::OSC52&& event) {
    m_render_thread.push_event(ClipboardRequest {
        .osc52 = di::move(event),
        .identifier = {},
        .manual = false,
        .reply = true,
    });
}

auto InputThread::handle_event(terminal::OSC8671& event, bool did_timeout) -> bool {
    if (event.type == terminal::SeamlessNavigationRequestType::Enter) {
        m_layout_state.with_lock([&](LayoutState& state) {
            for (auto& tab : state.active_tab()) {
                auto range_start = event.range
                                       .transform([](auto x) {
                                           return di::get<0>(x) - 1;
                                       })
                                       .value_or(0u);
                auto range_end = event.range
                                     .transform([](auto x) {
                                         return di::get<1>(x);
                                     })
                                     .value_or(event.direction == terminal::NavigateDirection::Left ||
                                                       event.direction == terminal::NavigateDirection::Right
                                                   ? state.size().rows
                                                   : state.size().cols);
                tab.navigate(event.direction.value(), terminal::NavigateWrapMode::Allow, {},
                             di::Tuple { range_start, range_end }, Tab::SeamlessNavigateMode::Disabled, true);

                // We always request a render because we configure enter events to clear the current cursor
                // to prevent flickering.
                m_render_thread.request_render();
            }
        });
        return true;
    }

    if (event.type != terminal::SeamlessNavigationRequestType::Navigate) {
        return true;
    }

    auto seamless_navigate_mode =
        did_timeout ? Tab::SeamlessNavigateMode::Disabled : Tab::SeamlessNavigateMode::Enabled;
    auto did_navigate = m_layout_state.with_lock([&](LayoutState& state) -> di::Optional<bool> {
        for (auto& tab : state.active_tab()) {
            if (!tab.layout_tree() || !tab.active()) {
                return false;
            }
            auto range = event.range.transform([&](auto x) -> di::Tuple<u32, u32> {
                auto [start, end] = x;
                auto entry = tab.layout_tree()->find_pane(tab.active().data());
                ASSERT(entry);
                auto const limit = event.direction.value() == terminal::NavigateDirection::Left ||
                                           event.direction.value() == terminal::NavigateDirection::Right
                                       ? entry->size.rows
                                       : entry->size.cols;
                auto const base = event.direction.value() == terminal::NavigateDirection::Left ||
                                          event.direction.value() == terminal::NavigateDirection::Right
                                      ? entry->row
                                      : entry->col;
                return { base + di::min(start - 1, limit), base + di::min(end, limit) };
            });
            return tab.navigate(event.direction.value(), event.wrap_mode, event.id.clone(), range,
                                seamless_navigate_mode, false);
        }
        return false;
    });

    if (did_navigate == true) {
        m_render_thread.request_render();
    }

    if (did_navigate.has_value() && event.wrap_mode == terminal::NavigateWrapMode::Disallow) {
        // We need to reply to the OSC 8671 request. Note that for navigation triggered by the user
        // pressing keyboard shortcuts wrap mode will be Enabled, so we won't ever send a response when
        // we're just simulating events.
        if (did_navigate.value()) {
            event.type = terminal::SeamlessNavigationRequestType::Acknowledge;
            event.range = {};
        }
        m_render_thread.push_event(WriteString { event.serialize() });
        return true;
    }
    return did_navigate.has_value();
}

void InputThread::request_navigate(terminal::NavigateDirection direction) {
    auto dist = di::UniformIntDistribution<u64>(0, di::NumericLimits<u64>::max);
    auto id = di::to_string(dist(m_rng));
    m_pending_events.with_lock([&](auto& pending_events) {
        // We push to the front of the queue because this function is expected to be called as
        // the result of processing another input event. Effectively meaning this event replaces
        // it in the queue.
        pending_events.push_front({
            .event =
                terminal::OSC8671 {
                    .type = terminal::SeamlessNavigationRequestType::Navigate,
                    .direction = direction,
                    .id = di::move(id),
                    .wrap_mode = terminal::NavigateWrapMode::Allow,
                },
            .receptionTime = dius::SteadyClock::now(),
        });
    });
}

auto InputThread::handle_drag(LayoutState& state, MouseCoordinate const& coordinate) -> bool {
    auto _ = di::ScopeExit([&] {
        m_drag_origin = coordinate;
    });

    auto y_amount = i32(m_drag_origin.value().y()) - i32(coordinate.y());
    auto x_amount = i32(m_drag_origin.value().x()) - i32(coordinate.x());
    if (y_amount == 0 && x_amount == 0) {
        return false;
    }
    ASSERT(x_amount == 0 || y_amount == 0);

    auto& tab = state.active_tab().value();
    auto direct_entry = tab.layout_tree()->hit_test(coordinate.y(), coordinate.x());
    for (auto const& entry : direct_entry) {
        if (di::abs(y_amount) > 0 &&
            (m_drag_origin.value().y() == entry.row - 1 || m_drag_origin.value().y() == entry.row + entry.size.rows)) {
            auto y_edge = m_drag_origin.value().y() <= entry.row ? ResizeDirection::Top : ResizeDirection::Bottom;
            if (y_edge == ResizeDirection::Bottom) {
                y_amount *= -1;
            }
            return tab.layout_group().resize(*tab.layout_tree(), entry.pane, y_edge, y_amount);
        }

        if (di::abs(x_amount) > 0 &&
            (m_drag_origin.value().x() == entry.col - 1 || m_drag_origin.value().x() == entry.col + entry.size.cols)) {
            auto x_edge = m_drag_origin.value().x() <= entry.col ? ResizeDirection::Left : ResizeDirection::Right;
            if (x_edge == ResizeDirection::Right) {
                x_amount *= -1;
            }
            return tab.layout_group().resize(*tab.layout_tree(), entry.pane, x_edge, x_amount);
        }
    }

    return false;
}

void InputThread::notify_osc_8671(terminal::OSC8671&& osc_8671) {
    if (osc_8671.type != terminal::SeamlessNavigationRequestType::Navigate &&
        osc_8671.type != terminal::SeamlessNavigationRequestType::Acknowledge) {
        return;
    }

    auto should_process = m_pending_events.with_lock([&](auto& pending_events) {
        if (auto ev = pending_events.front()) {
            if (auto event = di::get_if<terminal::OSC8671>(ev->event)) {
                if (event->id == osc_8671.id) {
                    if (osc_8671.type == terminal::SeamlessNavigationRequestType::Acknowledge) {
                        pending_events.pop_front();
                    } else {
                        // Clear reception time to force a timeout. This results in processing the event.
                        ev->receptionTime = {};
                        event->range = osc_8671.range;
                    }
                    return true;
                }
            }
        }
        return false;
    });
    if (should_process) {
        process_pending_events();
    }
}
}
