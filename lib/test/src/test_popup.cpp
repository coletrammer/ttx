#include "di/test/prelude.h"
#include "ttx/layout.h"
#include "ttx/popup.h"
#include "ttx/size.h"

namespace popup {
using namespace ttx;

static auto alignments() {
    auto size = Size { 50, 60, 600, 50000 };

    struct Case {
        PopupLayout input {};
        LayoutEntry expected {};
    };

    auto make_input = [&](PopupAlignment alignment, i64 height_percent, i64 width_percent) {
        return PopupLayout {
            .alignment = alignment,
            .width = RelatizeSize(max_layout_precision / 100 * width_percent),
            .height = RelatizeSize(max_layout_precision / 100 * height_percent),
        };
    };

    auto make_fixed_input = [&](PopupAlignment alignment, u32 height, u32 width) {
        return PopupLayout {
            .alignment = alignment,
            .width = AbsoluteSize(width),
            .height = AbsoluteSize(height),
        };
    };

    auto make_entry = [&](u32 row, u32 col, u32 rows, u32 cols) {
        return LayoutEntry {
            .row = row,
            .col = col,
            .size = Size(rows, cols, size.xpixels / cols, size.ypixels / rows),
        };
    };

    auto cases = di::Array {
        // Fixed
        Case {
            .input = make_input(PopupAlignment::Center, 50, 50),
            .expected = make_entry(13, 15, 25, 30),
        },
        Case {
            .input = make_input(PopupAlignment::Bottom, 50, 50),
            .expected = make_entry(25, 15, 25, 30),
        },
        Case {
            .input = make_input(PopupAlignment::Top, 50, 50),
            .expected = make_entry(0, 15, 25, 30),
        },
        Case {
            .input = make_input(PopupAlignment::Left, 50, 50),
            .expected = make_entry(13, 0, 25, 30),
        },
        Case {
            .input = make_input(PopupAlignment::Right, 50, 50),
            .expected = make_entry(13, 30, 25, 30),
        },
        // Too small
        Case {
            .input = make_input(PopupAlignment::Center, 1, 1),
            .expected = make_entry(25, 30, 1, 1),
        },
        // Absolute
        Case {
            .input = make_fixed_input(PopupAlignment::Center, 25, 30),
            .expected = make_entry(13, 15, 25, 30),
        },
        // Too big
        Case {
            .input = make_fixed_input(PopupAlignment::Center, 100, 100),
            .expected = make_entry(0, 0, 50, 60),
        },
    };

    for (auto const& [input, expected] : cases) {
        auto popup = Popup { nullptr, input };
        auto result = popup.layout(size);
        ASSERT_EQ(result, expected);
    }
};

TEST(popup, alignments)
}
