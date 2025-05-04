#pragma once

#include "dius/steady_clock.h"
#include "ttx/mouse.h"
#include "ttx/mouse_event.h"

namespace ttx {
class MouseClickTracker {
public:
    using Duration = dius::SteadyClock::Duration;
    using TimePoint = dius::SteadyClock::TimePoint;

    explicit MouseClickTracker(u32 max_clicks, Duration threshold = di::chrono::Milliseconds(200))
        : m_max_clicks(max_clicks), m_threshold(threshold) {}

    /// @brief Track a mouse event and return the number of consecutive clicks.
    ///
    /// @return The number of consecutive clicks
    ///
    /// This functions returns 0 for non-click mouse events. So to track double clicks,
    /// look for a return value of 2.
    auto track(MouseEvent const& event, TimePoint now = dius::SteadyClock::now()) -> u32;

private:
    struct Prev {
        MouseButton last_button { MouseButton::None };
        TimePoint time {};
        u32 consecutive_clicks { 0 };
    };

    di::Optional<Prev> m_prev;
    u32 m_max_clicks { 0 };
    Duration const m_threshold {};
};
}
