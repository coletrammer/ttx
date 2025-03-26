#include "di/test/prelude.h"
#include "ttx/terminal/screen.h"

namespace screen {
using namespace ttx::terminal;

static void put_single_cell() {
    auto screen = Screen {};
    screen.resize({ 20, 5 });

    screen.put_code_point(U'a');
    screen.put_code_point(U'b');
    screen.put_code_point(U'│');

    auto cursor = screen.cursor();
    ASSERT_EQ(cursor, (Cursor {
                          .row = 0,
                          .col = 3,
                          .text_offset = 5,
                      }));

    screen.set_cursor_col(0);
    ASSERT_EQ(screen.text_at_cursor(), "a"_sv);

    screen.set_cursor_col(1);
    ASSERT_EQ(screen.text_at_cursor(), "b"_sv);

    screen.set_cursor_col(2);
    ASSERT_EQ(screen.text_at_cursor(), "│"_sv);
}

TEST(screen, put_single_cell)
}
