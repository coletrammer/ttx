#include "ttx/terminal/escapes/osc_52.h"

#include "di/format/prelude.h"

namespace ttx::terminal {
constexpr auto selection_mapping = di::Array {
    di::Tuple { SelectionType::Clipboard, U'c' }, di::Tuple { SelectionType::Selection, U'p' },
    di::Tuple { SelectionType::Selection, U's' }, di::Tuple { SelectionType::_0, U'0' },
    di::Tuple { SelectionType::_1, U'1' },        di::Tuple { SelectionType::_2, U'2' },
    di::Tuple { SelectionType::_3, U'3' },        di::Tuple { SelectionType::_4, U'4' },
    di::Tuple { SelectionType::_5, U'5' },        di::Tuple { SelectionType::_6, U'6' },
    di::Tuple { SelectionType::_7, U'7' },
};

auto OSC52::parse(di::StringView data) -> di::Optional<OSC52> {
    auto semicolon = data.find(U';');
    if (!semicolon) {
        return {};
    }

    auto result = OSC52 {};
    auto selection = data.substr(data.begin(), semicolon.begin());
    auto added = u16(0);
    for (auto ch : selection) {
        for (auto [t, c] : selection_mapping) {
            if (c == ch) {
                if (!(added & (1 << u16(t)))) {
                    (void) result.selections.push_back(t);
                    added |= (1 << u16(t));
                    break;
                }
            }
        }
    }
    if (result.selections.empty()) {
        if (!selection.empty()) {
            return {};
        }
        (void) result.selections.push_back(SelectionType::Clipboard);
    }

    data = data.substr(semicolon.end());
    if (data == "?"_sv) {
        result.query = true;
        return result;
    }

    // Xterm spec says that invalid base64 strings will result in clearing
    // the selection, instead of ignoring. Which is weird, but we'll do what
    // it says.
    result.data = di::parse<di::Base64<>>(data).value_or(di::Base64<>());
    return result;
}

auto OSC52::serialize() const -> di::String {
    auto selection = "c"_s;
    if (!selections.empty()) {
        selection = selections | di::transform([](SelectionType type) -> c32 {
                        for (auto [t, c] : selection_mapping) {
                            if (t == type) {
                                return c;
                            }
                        }
                        return 'c';
                    }) |
                    di::to<di::String>();
    }
    if (query) {
        return *di::present("\033]52;{};?\033\\"_sv, selection);
    }
    return *di::present("\033]52;{};{}\033\\"_sv, selection, data);
}
}
