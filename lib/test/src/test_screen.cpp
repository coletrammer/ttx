#include "di/test/prelude.h"
#include "ttx/terminal/screen.h"

namespace screen {
using namespace ttx::terminal;

static void put_single_cell() {
    auto screen = Screen {};
    screen.resize(5, 20);

    screen.put_single_cell("a"_sv);
    screen.put_single_cell("b"_sv);

    auto cursor = screen.cursor();
    ASSERT_EQ(cursor, (Cursor {
                          .row = 0,
                          .col = 2,
                          .text_offset = 2,
                      }));

    screen.set_cursor_col(0);
    ASSERT_EQ(screen.text_at_cursor(), "a"_sv);

    screen.set_cursor_col(1);
    ASSERT_EQ(screen.text_at_cursor(), "b"_sv);
}

TEST(screen, put_single_cell)
}
