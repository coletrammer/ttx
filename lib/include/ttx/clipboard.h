#pragma once

#include "di/container/queue/priority_queue.h"
#include "di/function/compare_backwards.h"
#include "di/reflect/prelude.h"
#include "dius/steady_clock.h"
#include "ttx/features.h"
#include "ttx/terminal/escapes/osc_52.h"

namespace ttx {
/// @brief Clipboard modes
enum class ClipboardMode {
    System,               ///< Attempt to read and write the system clipboard
    SystemWriteLocalRead, ///< Write to system clipboard but read from internal clipboard
    SystemWriteNoRead,    ///< Write to system clipboard but disallow reading clipboard
    Local,                ///< Read and write to internal clipboard
    LocalWriteNoRead,     ///< Write to internal clipboard but disallow reading clipboard
    Disabled,             ///< Disallow read/writing the clipboard
};

constexpr auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<ClipboardMode>) {
    using enum ClipboardMode;
    return di::make_enumerators<"ClipboardMode">(
        di::enumerator<"System", System>, di::enumerator<"SystemWriteLocalRead", SystemWriteLocalRead>,
        di::enumerator<"SystemWriteNoRead", SystemWriteNoRead>, di::enumerator<"Local", Local>,
        di::enumerator<"LocalWriteNoRead", LocalWriteNoRead>, di::enumerator<"Disabled", Disabled>);
}

/// @brief Implementation of clipboard handling for ttx
///
/// This class supports a number of different simulataneous
/// selections, each of which can be queried or set.
class Clipboard {
public:
    constexpr static auto request_timeout = di::chrono::Seconds(1);

    struct Identifier {
        u64 session_id { 0 };
        u64 tab_id { 0 };
        u64 pane_id { 0 };

        auto operator==(Identifier const&) const -> bool = default;
        auto operator<=>(Identifier const&) const = default;
    };

    struct Reply {
        Identifier identifier;
        terminal::SelectionType type = terminal::SelectionType::Selection;
        di::Vector<byte> data;
    };

    explicit Clipboard(ClipboardMode mode, Feature features);

    [[nodiscard]] auto set_clipboard(terminal::SelectionType type, di::Vector<byte> data,
                                     dius::SteadyClock::TimePoint reception = dius::SteadyClock::now()) -> bool;
    [[nodiscard]] auto request_clipboard(terminal::SelectionType type, Identifier const& identifier,
                                         dius::SteadyClock::TimePoint reception = dius::SteadyClock::now()) -> bool;
    void got_clipboard_response(terminal::SelectionType type, di::Vector<byte> data,
                                dius::SteadyClock::TimePoint reception = dius::SteadyClock::now());
    [[nodiscard]] auto get_replies(dius::SteadyClock::TimePoint reception = dius::SteadyClock::now())
        -> di::Vector<Reply>;

    [[nodiscard]] auto mode() const { return m_mode; }

private:
    void expire(dius::SteadyClock::TimePoint reception);

    enum class ClipboardReadAction {
        Ignore,
        ReadLocal,
        ReadSystem,
        RequestSystemReadLocal,
    };

    enum class ClipboardWriteAction {
        Ignore,
        WriteLocal,
        WriteSystem,
    };

    auto action_for_clipboard_read(terminal::SelectionType type) -> ClipboardReadAction;
    auto action_for_clipboard_write(terminal::SelectionType type) -> ClipboardWriteAction;

    struct Request {
        dius::SteadyClock::TimePoint reception;
        Identifier identifier;

        auto operator==(Request const&) const -> bool = default;
        auto operator<=>(Request const&) const = default;
    };

    struct SelectionState {
        di::Vector<byte> data;
        di::PriorityQueue<Request, di::Vector<Request>, di::CompareBackwards> requests;
        bool system_working { false };
    };

    ClipboardMode m_mode { ClipboardMode::System };
    Feature m_features { Feature::None };
    di::Array<SelectionState, usize(terminal::SelectionType::Max)> m_state;
    di::Vector<Reply> m_replies;
};
}
