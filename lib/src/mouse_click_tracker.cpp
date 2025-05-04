#include "ttx/mouse_click_tracker.h"

#include "ttx/mouse_event.h"

namespace ttx {
auto MouseClickTracker::track(MouseEvent const& event, TimePoint now) -> u32 {
    switch (event.type()) {
        case MouseEventType::Press: {
            if (!m_prev || m_prev.value().last_button != event.button() || now > m_prev.value().time + m_threshold) {
                m_prev = { event.button(), now, 1 };
                return 1;
            }

            m_prev.value().time = now;
            return 1 + ((m_prev.value().consecutive_clicks++) % m_max_clicks);
        }
        case MouseEventType::Move:
        case MouseEventType::Release:
            break;
    }
    return 0;
}
}
