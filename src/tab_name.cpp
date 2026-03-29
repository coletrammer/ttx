#include "tab_name.h"

#include "di/container/string/conversion.h"
#include "di/format/prelude.h"
#include "tab.h"

namespace ttx {
auto evaluate_tab_name(di::Span<TabNameSource const> sources, Tab& tab, usize index) -> di::String {
    for (auto source : sources) {
        switch (source) {
            case TabNameSource::Manual:
                for (auto const& name : tab.name()) {
                    return name.to_owned();
                }
                break;
            case TabNameSource::WindowTitle:
                for (auto& pane : tab.active()) {
                    auto title = pane.window_title();
                    if (title) {
                        return di::move(title).value();
                    }
                }
                break;
            case TabNameSource::CurrentWorkingDirectory:
                for (auto& pane : tab.active()) {
                    for (auto const& directory : pane.current_working_directory()) {
                        for (auto part : directory.back()) {
                            return di::to_utf8_string_lossy(part.view());
                        }
                    }
                }
                break;
        }
    }
    return di::format("tab-{}"_sv, index + 1);
}
}
