#include "save_layout.h"

#include "dius/filesystem/operations.h"
#include "layout_state.h"

namespace ttx {
auto SaveLayoutThread::create(di::Synchronized<LayoutState>& layout_state, di::Path save_path)
    -> di::Result<di::Box<SaveLayoutThread>> {
    auto result = di::make_box<SaveLayoutThread>(layout_state, di::move(save_path));
    result->m_thread = TRY(dius::Thread::create([&self = *result.get()] {
        self.save_layout_thread();
    }));
    return result;
}

SaveLayoutThread::SaveLayoutThread(di::Synchronized<LayoutState>& layout_state, di::Path save_path)
    : m_layout_state(layout_state), m_save_path(di::move(save_path)) {}

SaveLayoutThread::~SaveLayoutThread() {
    (void) m_thread.join();
}

void SaveLayoutThread::push_event(SaveLayoutEvent event) {
    m_events.with_lock([&](auto& queue) {
        queue.push(di::move(event));
        m_condition.notify_one();
    });
}

auto SaveLayoutThread::save_layout() -> di::Result<> {
    auto state = m_layout_state.with_lock([&](LayoutState const& state) {
        return state.as_json_v1();
    });
    auto json_string = TRY(di::to_json_string(state, di::JsonSerializerConfig().pretty().indent_width(4)));

    // TODO: we probably should backup the layout file before writing, in case something goes wrong.
    auto file = TRY(dius::open_sync(m_save_path, dius::OpenMode::WriteClobber));
    return file.write_exactly(di::as_bytes(json_string.span()));
}

void SaveLayoutThread::save_layout_thread() {
    auto renderer = Renderer();
    auto _ = di::ScopeExit([&] {
        (void) renderer.cleanup(dius::stdin);
    });

    // TODO: this should recursively make the directory.
    if (auto parent_path = m_save_path.parent_path()) {
        (void) dius::filesystem::create_directory(parent_path.value());
    }

    auto deadline = dius::SteadyClock::now();
    for (;;) {
        while (deadline < dius::SteadyClock::now()) {
            deadline += di::Seconds(1); // Save layout with a rate limit
        }
        dius::this_thread::sleep_until(deadline);

        // Fetch events all events from the queue.
        auto events = [&] {
            auto lock = di::UniqueLock(m_events.get_lock());
            m_condition.wait(lock, [&] {
                // SAFETY: we acquired the lock manually above.
                return !m_events.get_assuming_no_concurrent_accesses().empty();
            });

            // SAFETY: we acquired the lock manually above.
            auto result = di::Vector<SaveLayoutEvent> {};
            for (auto& event : m_events.get_assuming_no_concurrent_accesses()) {
                result.push_back(di::move(event));
            }
            return result;
        }();

        // Process any pending events.
        for (auto& event : events) {
            // Pattern matching would be nice here...
            if (auto ev = di::get_if<SaveLayout>(event)) {
                continue;
            }
            if (auto ev = di::get_if<SaveLayoutExit>(event)) {
                // Exit.
                return;
            }
        }

        (void) save_layout();
    }
}
}
