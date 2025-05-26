#include "di/test/prelude.h"
#include "ttx/clipboard.h"
#include "ttx/features.h"
#include "ttx/terminal/escapes/osc_52.h"

namespace clipboard {
static void system() {
    auto clipboard = ttx::Clipboard(ttx::ClipboardMode::System, ttx::Feature::Clipboard);

    auto t = dius::SteadyClock::TimePoint(di::chrono::Seconds(1000));
    clipboard.got_clipboard_response(ttx::terminal::SelectionType::Clipboard, { '1'_b }, t);
    ASSERT(clipboard.request_clipboard(ttx::terminal::SelectionType::Clipboard, { 1, 2, 3 }, t));
    ASSERT(clipboard.get_replies(t).empty());

    clipboard.got_clipboard_response(ttx::terminal::SelectionType::Clipboard, { '4'_b }, t);
    auto responses = clipboard.get_replies(t);
    ASSERT_EQ(responses.size(), 1zu);
    ASSERT_EQ(responses[0].identifier, ttx::Clipboard::Identifier(1, 2, 3));
    ASSERT_EQ(responses[0].type, ttx::terminal::SelectionType::Clipboard);
    ASSERT_EQ(responses[0].data, di::Vector { '4'_b });

    // Expiration
    ASSERT(clipboard.request_clipboard(ttx::terminal::SelectionType::Clipboard, { 1, 2, 3 }, t));
    ASSERT(clipboard.get_replies(t).empty());
    t += ttx::Clipboard::request_timeout;
    responses = clipboard.get_replies(t);
    ASSERT_EQ(responses.size(), 1zu);
    ASSERT_EQ(responses[0].identifier, ttx::Clipboard::Identifier(1, 2, 3));
    ASSERT_EQ(responses[0].type, ttx::terminal::SelectionType::Clipboard);
    ASSERT_EQ(responses[0].data, di::Vector { '4'_b });

    // Now, the request will be filled immiedately, since the system clipboard considered broken.
    ASSERT(clipboard.request_clipboard(ttx::terminal::SelectionType::Clipboard, { 1, 2, 3 }, t));
    responses = clipboard.get_replies(t);
    ASSERT_EQ(responses.size(), 1zu);
    ASSERT_EQ(responses[0].identifier, ttx::Clipboard::Identifier(1, 2, 3));
    ASSERT_EQ(responses[0].type, ttx::terminal::SelectionType::Clipboard);
    ASSERT_EQ(responses[0].data, di::Vector { '4'_b });

    // Setting the clipboard works
    ASSERT(clipboard.set_clipboard(ttx::terminal::SelectionType::Clipboard, { '5'_b }, t));
    ASSERT(clipboard.request_clipboard(ttx::terminal::SelectionType::Clipboard, { 1, 2, 3 }, t));
    responses = clipboard.get_replies(t);
    ASSERT_EQ(responses.size(), 1zu);
    ASSERT_EQ(responses[0].identifier, ttx::Clipboard::Identifier(1, 2, 3));
    ASSERT_EQ(responses[0].type, ttx::terminal::SelectionType::Clipboard);
    ASSERT_EQ(responses[0].data, di::Vector { '5'_b });
}

static void local() {
    auto clipboard = ttx::Clipboard(ttx::ClipboardMode::Local, ttx::Feature::Clipboard);

    auto t = dius::SteadyClock::TimePoint(di::chrono::Seconds(1000));
    ASSERT(!clipboard.request_clipboard(ttx::terminal::SelectionType::Clipboard, { 1, 2, 3 }, t));
    auto responses = clipboard.get_replies(t);
    ASSERT_EQ(responses.size(), 1zu);
    ASSERT_EQ(responses[0].identifier, ttx::Clipboard::Identifier(1, 2, 3));
    ASSERT_EQ(responses[0].type, ttx::terminal::SelectionType::Clipboard);
    ASSERT_EQ(responses[0].data, di::Vector<byte> {});

    // Setting the clipboard works
    ASSERT(!clipboard.set_clipboard(ttx::terminal::SelectionType::Clipboard, { '5'_b }, t));
    ASSERT(!clipboard.request_clipboard(ttx::terminal::SelectionType::Clipboard, { 1, 2, 3 }, t));
    responses = clipboard.get_replies(t);
    ASSERT_EQ(responses.size(), 1zu);
    ASSERT_EQ(responses[0].identifier, ttx::Clipboard::Identifier(1, 2, 3));
    ASSERT_EQ(responses[0].type, ttx::terminal::SelectionType::Clipboard);
    ASSERT_EQ(responses[0].data, di::Vector { '5'_b });
}

TEST(clipboard, system)
TEST(clipboard, local)
}
