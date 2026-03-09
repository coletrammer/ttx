#include "save_layout.h"

#include "config.h"
#include "dius/filesystem/operations.h"
#include "layout_state.h"

namespace ttx {
auto SaveLayoutThread::create(di::Synchronized<LayoutState>& layout_state, di::Path save_dir, SessionConfig config)
    -> di::Result<di::Box<SaveLayoutThread>> {
    auto result = di::make_box<SaveLayoutThread>(layout_state, di::move(save_dir), di::move(config));
    result->m_thread = TRY(dius::Thread::create([&self = *result.get()] {
        self.save_layout_thread();
    }));
    return result;
}

auto SaveLayoutThread::create_mock(di::Synchronized<LayoutState>& layout_state) -> di::Box<SaveLayoutThread> {
    return di::make_box<SaveLayoutThread>(layout_state, di::Path {}, SessionConfig {});
}

SaveLayoutThread::SaveLayoutThread(di::Synchronized<LayoutState>& layout_state, di::Path save_dir, SessionConfig config)
    : m_layout_state(layout_state), m_save_dir(di::move(save_dir)), m_config(di::move(config)) {}

SaveLayoutThread::~SaveLayoutThread() {
    (void) m_thread.join();
}

void SaveLayoutThread::push_event(SaveLayoutEvent event) {
    m_events.with_lock([&](auto& queue) {
        queue.push(di::move(event));
        m_condition.notify_one();
    });
}

auto SaveLayoutThread::save_layout(di::TransparentStringView layout_name) -> di::Result<> {
    auto state = m_layout_state.with_lock([&](LayoutState const& state) {
        return state.as_json();
    });
    auto json_string = TRY(di::to_json_string(state, di::JsonSerializerConfig().pretty().indent_width(4)));

    // TODO: we probably should backup the layout file before writing, in case something goes wrong.
    auto save_path = m_save_dir.clone();
    save_path /= layout_name;
    save_path += ".json"_tsv;
    auto file = TRY(dius::open_sync(save_path, dius::OpenMode::WriteClobber));
    return file.write_exactly(di::as_bytes(json_string.span()));
}

void SaveLayoutThread::save_layout_thread() {
    auto renderer = Renderer();
    auto _ = di::ScopeExit([&] {
        (void) renderer.cleanup(dius::std_in);
    });

    (void) dius::filesystem::create_directories(m_save_dir);

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
                if (ev->layout_name) {
                    (void) save_layout(ev->layout_name.value());
                }
                continue;
            }
            if (auto ev = di::get_if<SaveLayoutUpdateConfig>(event)) {
                m_config = di::move(ev->config);
                continue;
            }
            if (auto ev = di::get_if<SaveLayoutExit>(event)) {
                // Exit.
                return;
            }
        }

        if (m_config.save_layout) {
            (void) save_layout(m_config.layout_name);
        }
    }
}
}
