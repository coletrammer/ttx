#include "di/test/prelude.h"
#include "ttx/mouse.h"
#include "ttx/mouse_click_tracker.h"
#include "ttx/mouse_event.h"

namespace mouse_click_tracker {
static void basic() {
    auto tracker = ttx::MouseClickTracker { 3, di::chrono::Seconds(1) };

    auto now = ttx::MouseClickTracker::TimePoint(di::chrono::Seconds(0));
    ASSERT_EQ(tracker.track(ttx::MouseEvent::press(ttx::MouseButton::Left), now), 1);
    now += di::chrono::Milliseconds(999);
    ASSERT_EQ(tracker.track(ttx::MouseEvent(ttx::MouseEventType::Move, ttx::MouseButton::Left), now), 0);
    ASSERT_EQ(tracker.track(ttx::MouseEvent::press(ttx::MouseButton::Left), now), 2);
    now += di::chrono::Milliseconds(999);
    ASSERT_EQ(tracker.track(ttx::MouseEvent::press(ttx::MouseButton::Left), now), 3);
    now += di::chrono::Milliseconds(999);
    ASSERT_EQ(tracker.track(ttx::MouseEvent::press(ttx::MouseButton::Left), now), 1);
    now += di::chrono::Milliseconds(1001);
    ASSERT_EQ(tracker.track(ttx::MouseEvent::press(ttx::MouseButton::Left), now), 1);
    ASSERT_EQ(tracker.track(ttx::MouseEvent::press(ttx::MouseButton::Right), now), 1);
}

TEST(mouse_click_tracker, basic)
}
