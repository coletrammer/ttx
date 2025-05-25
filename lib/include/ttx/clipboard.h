#pragma once

#include "dius/steady_clock.h"
#include "ttx/features.h"
#include "ttx/terminal/escapes/osc_52.h"

namespace ttx {
/// @brief Implementation of clipboard handling for ttx
///
/// This class supports a number of different simulataneous
/// selections, each of which can be queried or set.
class Clipboard {
    // Each selection type operates independently.
    struct SelectionState {
        di::Vector<byte> data;
    };

public:
    constexpr static auto request_timeout = di::chrono::Seconds(1);

    struct Identifier {
        u64 session_id { 0 };
        u64 tab_id { 0 };
        u64 pane_id { 0 };
    };

    struct Reply {
        Identifier identifier;
        terminal::SelectionType type = terminal::SelectionType::Selection;
        di::Vector<byte> data;
    };

    explicit Clipboard(Feature features);

    auto set_clipboard(terminal::SelectionType type, di::Vector<byte> data,
                       dius::SteadyClock::TimePoint reception = dius::SteadyClock::now()) -> bool;
    auto request_clipboard(terminal::SelectionType type, Identifier const& identifier,
                           dius::SteadyClock::TimePoint reception = dius::SteadyClock::now()) -> bool;
    void got_clipboard_response(terminal::SelectionType type, di::Vector<byte> data,
                                dius::SteadyClock::TimePoint reception = dius::SteadyClock::now());
    auto get_replies(dius::SteadyClock::TimePoint reception = dius::SteadyClock::now()) -> di::Vector<Reply>;

private:
    void expire(dius::SteadyClock::TimePoint reception);

    Feature m_features { Feature::None };
    di::Array<SelectionState, usize(terminal::SelectionType::Max)> m_state;
    di::Vector<Reply> m_replies;
};
}
