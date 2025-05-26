#include "ttx/clipboard.h"

#include "di/assert/prelude.h"
#include "ttx/features.h"
#include "ttx/terminal/escapes/osc_52.h"

namespace ttx {
Clipboard::Clipboard(ClipboardMode mode, Feature features) : m_mode(mode), m_features(features) {}

auto Clipboard::set_clipboard(terminal::SelectionType type, di::Vector<byte> data,
                              dius::SteadyClock::TimePoint reception) -> bool {
    ASSERT_LT(type, terminal::SelectionType::Max);

    auto _ = di::ScopeExit([&] {
        expire(reception);
    });

    auto write_system = false;
    auto action = action_for_clipboard_write(type);
    switch (action) {
        case ClipboardWriteAction::Ignore:
            break;
        case ClipboardWriteAction::WriteSystem:
            write_system = true;
            [[fallthrough]];
        case ClipboardWriteAction::WriteLocal:
            m_state[usize(type)].data = di::move(data);
            break;
    }
    return write_system;
}

auto Clipboard::request_clipboard(terminal::SelectionType type, Identifier const& identifier,
                                  dius::SteadyClock::TimePoint reception) -> bool {
    ASSERT_LT(type, terminal::SelectionType::Max);

    expire(reception);

    auto request_system = false;
    auto data = di::Optional<di::Vector<byte>> {};
    auto action = action_for_clipboard_read(type);
    switch (action) {
        case ClipboardReadAction::Ignore:
            data.emplace();
            break;
        case ClipboardReadAction::RequestSystemReadLocal:
            request_system = true;
            [[fallthrough]];
        case ClipboardReadAction::ReadLocal:
            data = m_state[usize(type)].data.clone();
            break;
        case ClipboardReadAction::ReadSystem:
            request_system = true;
            break;
    }

    if (data) {
        m_replies.push_back(Reply {
            .identifier = identifier,
            .type = type,
            .data = di::move(data).value(),
        });
    } else {
        m_state[usize(type)].requests.push(Request {
            .reception = reception,
            .identifier = identifier,
        });
    }
    return request_system;
}

void Clipboard::got_clipboard_response(terminal::SelectionType type, di::Vector<byte> data,
                                       dius::SteadyClock::TimePoint reception) {
    ASSERT_LT(type, terminal::SelectionType::Max);

    auto _ = di::ScopeExit([&] {
        expire(reception);
    });

    auto& state = m_state[usize(type)];
    state.system_working = true;

    // Don't overwrite the clipboard if an empty response is detected. In this case, its
    // possible the user has configured their terminal in such a way that applications don't
    // have permission to read the clipboard. We want to fallback to our own local clipboard
    // in this case.
    if (!data.empty()) {
        state.data = di::move(data);
    }

    if (auto request = state.requests.pop()) {
        m_replies.push_back(Reply {
            .identifier = request.value().identifier,
            .type = type,
            .data = state.data.clone(),
        });
    }
}

auto Clipboard::get_replies(dius::SteadyClock::TimePoint reception) -> di::Vector<Reply> {
    expire(reception);
    return di::exchange(m_replies, di::Vector<Reply> {});
}

void Clipboard::expire(dius::SteadyClock::TimePoint reception) {
    for (auto [i, state] : m_state | di::enumerate) {
        while (!state.requests.empty()) {
            auto const& top = state.requests.top().value();
            if (top.reception + request_timeout <= reception) {
                m_replies.push_back(Reply {
                    .identifier = top.identifier,
                    .type = terminal::SelectionType(i),
                    .data = state.data.clone(),
                });
                state.requests.pop();
                state.system_working = false;
                continue;
            }
            break;
        }
    }
}

auto Clipboard::action_for_clipboard_read(terminal::SelectionType type) -> ClipboardReadAction {
    switch (m_mode) {
        case ClipboardMode::System:
            break;
        case ClipboardMode::SystemWriteLocalRead:
        case ClipboardMode::Local:
            return ClipboardReadAction::ReadLocal;
        case ClipboardMode::SystemWriteNoRead:
        case ClipboardMode::LocalWriteNoRead:
        case ClipboardMode::Disabled:
            return ClipboardReadAction::Ignore;
    }

    if (!(m_features & Feature::Clipboard)) {
        return ClipboardReadAction::ReadLocal;
    }

    if (type != terminal::SelectionType::Selection && type != terminal::SelectionType::Clipboard) {
        return ClipboardReadAction::ReadLocal;
    }

    auto const& state = m_state[usize(type)];
    if (state.system_working) {
        return ClipboardReadAction::ReadSystem;
    }
    return ClipboardReadAction::RequestSystemReadLocal;
}

auto Clipboard::action_for_clipboard_write(terminal::SelectionType type) -> ClipboardWriteAction {
    switch (m_mode) {
        case ClipboardMode::System:
        case ClipboardMode::SystemWriteLocalRead:
        case ClipboardMode::SystemWriteNoRead:
            break;
        case ClipboardMode::Local:
        case ClipboardMode::LocalWriteNoRead:
            return ClipboardWriteAction::WriteLocal;
        case ClipboardMode::Disabled:
            return ClipboardWriteAction::Ignore;
    }

    if (!(m_features & Feature::Clipboard)) {
        return ClipboardWriteAction::WriteLocal;
    }

    if (type != terminal::SelectionType::Selection && type != terminal::SelectionType::Clipboard) {
        return ClipboardWriteAction::WriteLocal;
    }
    return ClipboardWriteAction::WriteSystem;
}
}
