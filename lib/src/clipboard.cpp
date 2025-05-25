#include "ttx/clipboard.h"

#include "di/assert/prelude.h"
#include "ttx/features.h"

namespace ttx {
Clipboard::Clipboard(Feature features) : m_features(features) {}

auto Clipboard::set_clipboard(terminal::SelectionType type, di::Vector<byte> data,
                              dius::SteadyClock::TimePoint reception) -> bool {
    ASSERT_LT(type, terminal::SelectionType::Max);
    m_state[usize(type)].data = di::move(data);
    return true;
}

auto Clipboard::request_clipboard(terminal::SelectionType type, Identifier const& identifier,
                                  dius::SteadyClock::TimePoint reception) -> bool {
    ASSERT_LT(type, terminal::SelectionType::Max);
    m_replies.push_back(Reply {
        .identifier = identifier,
        .type = type,
        .data = m_state[usize(type)].data.clone(),
    });
    return true;
}

void Clipboard::got_clipboard_response(terminal::SelectionType type, di::Vector<byte> data,
                                       dius::SteadyClock::TimePoint reception) {
    ASSERT_LT(type, terminal::SelectionType::Max);
}

auto Clipboard::get_replies(dius::SteadyClock::TimePoint reception) -> di::Vector<Reply> {
    expire(reception);
    return di::exchange(m_replies, di::Vector<Reply> {});
}

void Clipboard::expire(dius::SteadyClock::TimePoint reception) {}
}
