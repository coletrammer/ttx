#pragma once

#include "config.h"
#include "di/random/prelude.h"
#include "di/reflect/prelude.h"
#include "input_mode.h"
#include "key_bind.h"
#include "layout_state.h"
#include "save_layout.h"
#include "ttx/features.h"
#include "ttx/focus_event.h"
#include "ttx/key_event.h"
#include "ttx/mouse.h"
#include "ttx/pane.h"
#include "ttx/paste_event.h"
#include "ttx/terminal/escapes/device_attributes.h"
#include "ttx/terminal/escapes/device_status.h"
#include "ttx/terminal/escapes/mode.h"
#include "ttx/terminal/escapes/osc_52.h"
#include "ttx/terminal/escapes/osc_8671.h"
#include "ttx/terminal/escapes/terminfo_string.h"
#include "ttx/terminal/navigation_direction.h"
#include "ttx/terminal/palette.h"
#include "ttx/terminal_input.h"

namespace ttx {
class RenderThread;

class InputThread {
public:
    static auto create(CreatePaneArgs create_pane_args, Config config, config_json::v1::Config base_config,
                       di::TransparentStringView profile, di::Synchronized<LayoutState>& layout_state,
                       FeatureResult features, RenderThread& render_thread, SaveLayoutThread& save_layout_thread)
        -> di::Result<di::Box<InputThread>>;
    static auto create_mock(di::Synchronized<LayoutState>& layout_state, RenderThread& render_thread,
                            SaveLayoutThread& save_layout_thread) -> di::Box<InputThread>;

    explicit InputThread(CreatePaneArgs create_pane_args, Config config, config_json::v1::Config base_config,
                         di::TransparentStringView profile, di::Synchronized<LayoutState>& layout_state,
                         FeatureResult features, RenderThread& render_thread, SaveLayoutThread& save_layout_thread);
    ~InputThread();

    void request_exit();
    void request_navigate(terminal::NavigateDirection direction);
    void set_config(Config config);
    auto config() const -> Config const& { return m_config; };
    auto profile() const -> di::TransparentStringView { return m_profile; }
    auto base_config() const -> config_json::v1::Config const& { return m_base_config; }

    void notify_osc_8671(terminal::OSC8671&& osc_8671);

private:
    void input_thread();

    void set_input_mode(InputMode mode);

    void handle_event(KeyEvent&& event);
    void handle_event(MouseEvent&& event);
    void handle_event(FocusEvent&& event);
    void handle_event(PasteEvent&& event);
    void handle_event(terminal::PrimaryDeviceAttributes&&) {}
    void handle_event(terminal::ModeQueryReply&&) {}
    void handle_event(terminal::CursorPositionReport&&) {}
    void handle_event(terminal::KittyKeyReport&&) {}
    void handle_event(terminal::DarkLightModeDetectionReport&&);
    void handle_event(terminal::StatusStringResponse&&) {}
    void handle_event(terminal::TerminfoString&&) {}
    void handle_event(terminal::OSC21&&);
    void handle_event(terminal::OSC52&& event);
    auto handle_event(terminal::OSC8671& event, bool did_timeout) -> bool;

    auto handle_drag(LayoutState& state, MouseCoordinate const& coordinate) -> bool;

    void process_pending_events();

    struct PendingEvent {
        Event event;
        dius::SteadyClock::TimePoint receptionTime;
        bool pending { false };
    };

    InputMode m_mode { InputMode::Insert };
    Config m_config;
    config_json::v1::Config m_base_config;
    di::TransparentStringView m_profile;
    di::Vector<KeyBind> m_key_binds;
    CreatePaneArgs m_create_pane_args;
    di::Atomic<bool> m_done { false };
    di::Optional<MouseCoordinate> m_drag_origin;
    di::Synchronized<LayoutState>& m_layout_state;
    di::Synchronized<di::Ring<PendingEvent>> m_pending_events;
    RenderThread& m_render_thread;
    SaveLayoutThread& m_save_layout_thread;
    Feature m_features { Feature::None };
    terminal::Palette m_outer_terminal_palette;
    di::MinstdRand m_rng;
    dius::Thread m_thread;
};
}
