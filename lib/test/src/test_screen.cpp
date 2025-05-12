#include "di/test/prelude.h"
#include "dius/print.h"
#include "ttx/graphics_rendition.h"
#include "ttx/terminal/screen.h"
#include "ttx/terminal/selection.h"

namespace screen {
using namespace ttx::terminal;

static void put_text(Screen& screen, di::StringView text) {
    for (auto code_point : text) {
        if (code_point == '\n') {
            screen.set_cursor_col(0);
            if (screen.cursor().row == screen.max_height() - 1) {
                screen.scroll_down();
            } else {
                screen.set_cursor_row(screen.cursor().row + 1);
            }
        } else {
            screen.put_code_point(code_point, AutoWrapMode::Enabled);
        }
    }
}

[[maybe_unused]] static void print_text(Screen& screen) {
    for (auto i : di::range(screen.absolute_row_start(), screen.absolute_row_end())) {
        dius::print("\""_sv);
        for (auto row : screen.iterate_row(i)) {
            auto [_, _, text, _, _] = row;
            if (text.empty()) {
                text = " "_sv;
            }
            dius::print("{}"_sv, text);
        }
        dius::println("\""_sv);
    }
}

static void validate_text(Screen& screen, di::StringView text) {
    ASSERT_EQ(screen.absolute_row_start(), 0);
    auto lines = text | di::split(U'\n');
    for (auto [i, line] : lines | di::enumerate) {
        if (line.contains(U'|')) {
            auto expected_text = line | di::split(U'|');
            ASSERT_EQ(di::distance(expected_text), screen.max_width());

            for (auto [expected, row] : di::zip(expected_text, screen.iterate_row(i))) {
                auto [_, cell, text, _, _] = row;
                if (expected == "."_sv) {
                    ASSERT(cell.is_nonprimary_in_multi_cell());
                    continue;
                }

                if (text.empty()) {
                    text = " "_sv;
                    ASSERT(!cell.is_multi_cell());
                }
                ASSERT_EQ(text, expected);
            }
        } else {
            ASSERT_EQ(di::distance(line), screen.max_width());

            for (auto [ch, row] : di::zip(line, screen.iterate_row(i))) {
                auto [_, cell, text, _, _] = row;
                if (ch == U'.') {
                    ASSERT(cell.is_nonprimary_in_multi_cell());
                    continue;
                }

                if (text.empty()) {
                    text = " "_sv;
                    ASSERT(!cell.is_multi_cell());
                }

                auto expected = di::String {};
                expected.push_back(ch);
                ASSERT_EQ(text, expected);
            }
        }
    }
}

static void validate_dirty(Screen& screen, di::StringView text) {
    ASSERT_EQ(screen.absolute_row_start(), 0);
    auto lines = text | di::split(U'\n');
    for (auto [i, line] : lines | di::enumerate) {
        ASSERT_EQ(di::distance(line), screen.max_width());

        auto [row_index, row_group] = screen.find_row(i);
        auto const& row = row_group.rows()[row_index];
        for (auto [ch, data] : di::zip(line, row_group.iterate_row(row_index))) {
            auto [_, cell, _, _, _] = data;

            if (ch != U' ') {
                auto cell_dirty = screen.whole_screen_dirty() || !row.stale || !cell.stale;
                auto expected = ch == U'y';
                ASSERT_EQ(cell_dirty, expected);
            }
            cell.stale = true;
        }
        row.stale = true;
    }
    screen.clear_whole_screen_dirty_flag();
}

static void put_text_basic() {
    auto screen = Screen({ 5, 5 }, Screen::ScrollBackEnabled::No);

    put_text(screen, "abcde"
                     "fghij"
                     "klmno"
                     "pqrst"
                     "uvwxy"_sv);

    auto cursor = screen.cursor();
    ASSERT_EQ(cursor, (Cursor {
                          .row = 4,
                          .col = 4,
                          .text_offset = 4,
                          .overflow_pending = true,
                      }));

    validate_text(screen, "abcde\n"
                          "fghij\n"
                          "klmno\n"
                          "pqrst\n"
                          "uvwxy"_sv);

    // Now overwrite some text.
    screen.set_cursor(2, 2);
    put_text(screen, u8"€𐍈"_sv);

    cursor = screen.cursor();
    ASSERT_EQ(cursor, (Cursor {
                          .row = 2,
                          .col = 4,
                          .text_offset = 9,
                      }));

    validate_text(screen, u8"abcde\n"
                          u8"fghij\n"
                          u8"kl€𐍈o\n"
                          u8"pqrst\n"
                          u8"uvwxy"_sv);
}

static void put_text_unicode() {
    auto screen = Screen({ 5, 5 }, Screen::ScrollBackEnabled::No);

    // Row 1 includes multi-byte utf8 characters, and
    // row 2 includes a zero-width diacritic.
    put_text(screen, u8"$¢€𐍈 "
                     u8"a\u0305"_sv);

    auto cursor = screen.cursor();
    ASSERT_EQ(cursor, (Cursor {
                          .row = 1,
                          .col = 1,
                          .text_offset = 3,
                      }));

    validate_text(screen, u8"$¢€𐍈 \n"
                          u8"a\u0305| | | | \n"
                          u8"     \n"
                          u8"     \n"
                          u8"     "_sv);
}

static void put_text_wide() {
    auto screen = Screen({ 5, 5 }, Screen::ScrollBackEnabled::No);

    put_text(screen, u8"ab猫e"_sv);  // Width 2 character
    put_text(screen, u8"abcd猫"_sv); // This wraps

    auto cursor = screen.cursor();
    ASSERT_EQ(cursor, (Cursor {
                          .row = 2,
                          .col = 2,
                          .text_offset = 3,
                          .overflow_pending = false,
                      }));

    validate_text(screen, u8"ab猫.e\n"
                          u8"abcd \n"
                          u8"猫.   \n"
                          u8"     \n"
                          u8"     "_sv);

    put_text(screen, "xxx"_sv);
    put_text(screen, "qwert"_sv);
    screen.put_code_point(U'猫', AutoWrapMode::Disabled);

    cursor = screen.cursor();
    ASSERT_EQ(cursor, (Cursor {
                          .row = 3,
                          .col = 4,
                          .text_offset = 6,
                          .overflow_pending = true,
                      }));

    validate_text(screen, u8"ab猫.e\n"
                          u8"abcd \n"
                          u8"猫.xxx\n"
                          u8"qwe猫.\n"
                          u8"     "_sv);

    put_text(screen, u8"猫猫e"_sv);
    screen.set_cursor(4, 1);
    screen.put_code_point(U'猫', AutoWrapMode::Disabled);

    cursor = screen.cursor();
    ASSERT_EQ(cursor, (Cursor {
                          .row = 4,
                          .col = 3,
                          .text_offset = 3,
                          .overflow_pending = false,
                      }));

    validate_text(screen, u8"ab猫.e\n"
                          u8"abcd \n"
                          u8"猫.xxx\n"
                          u8"qwe猫.\n"
                          u8" 猫. e"_sv);

    screen.set_cursor(2, 0);
    put_text(screen, "1"_sv);
    ASSERT_EQ(screen.cursor().text_offset, 1);

    validate_text(screen, u8"ab猫.e\n"
                          u8"abcd \n"
                          u8"1 xxx\n"
                          u8"qwe猫.\n"

                          u8" 猫. e"_sv);

    screen.set_cursor(3, 4);
    put_text(screen, "2"_sv);
    ASSERT_EQ(screen.cursor().text_offset, 3);

    validate_text(screen, u8"ab猫.e\n"
                          u8"abcd \n"
                          u8"1 xxx\n"
                          u8"qwe 2\n"
                          u8" 猫. e"_sv);
}

static void put_text_damage_tracking() {
    auto screen = Screen({ 5, 5 }, Screen::ScrollBackEnabled::No);

    validate_dirty(screen, "yyyyy\n"
                           "yyyyy\n"
                           "yyyyy\n"
                           "yyyyy\n"
                           "yyyyy"_sv);

    put_text(screen, u8"ab猫e"_sv);
    validate_text(screen, u8"ab猫.e\n"
                          u8"     \n"
                          u8"     \n"
                          u8"     \n"
                          u8"     "_sv);
    validate_dirty(screen, "yyy y\n"
                           "nnnnn\n"
                           "nnnnn\n"
                           "nnnnn\n"
                           "nnnnn"_sv);

    // Writing the same text should not mark the cells as dirty.
    screen.set_cursor(0, 0);
    put_text(screen, u8"ab猫e"_sv);
    validate_text(screen, u8"ab猫.e\n"
                          u8"     \n"
                          u8"     \n"
                          u8"     \n"
                          u8"     "_sv);
    validate_dirty(screen, "nnn n\n"
                           "nnnnn\n"
                           "nnnnn\n"
                           "nnnnn\n"
                           "nnnnn"_sv);
}

static void selection() {
    auto screen = Screen({ 3, 5 }, Screen::ScrollBackEnabled::Yes);

    put_text(screen, u8"ab猫\n"_sv
                     u8"猫fgh"_sv
                     u8"h猫f "_sv
                     u8"aa猫f"_sv
                     u8"a猫bb"_sv
                     u8"ddd猫"_sv);

    // Single mode (clamp to wide cell boundary)
    screen.begin_selection({ 5, 4 }, Screen::BeginSelectionMode::Single);
    ASSERT_EQ(screen.selection(), Selection({ 5, 3 }, { 5, 3 }));
    ASSERT_EQ(screen.selected_text(), u8"猫"_sv);

    // Word mode (expand towards spaces)
    screen.begin_selection({ 2, 2 }, Screen::BeginSelectionMode::Word);
    ASSERT_EQ(screen.selection(), Selection({ 2, 0 }, { 2, 3 }));
    ASSERT_EQ(screen.selected_text(), u8"h猫f"_sv);

    // Line mode (handles overlay smalls lines in scroll back)
    screen.begin_selection({ 0, 3 }, Screen::BeginSelectionMode::Line);
    ASSERT_EQ(screen.selection(), Selection({ 0, 0 }, { 0, 3 }));
    ASSERT_EQ(screen.selected_text(), u8"ab猫"_sv);

    // Update selection (clamps to multi cell and works). And selection text
    // only includes newlines that weren't caused by auto-wrapping.
    screen.update_selection({ 2, 2 });
    ASSERT_EQ(screen.selection(), Selection({ 0, 0 }, { 2, 1 }));
    ASSERT_EQ(screen.selected_text(), u8"ab猫\n猫fghh猫"_sv);
}

static void cursor_movement() {
    auto screen = Screen({ 5, 5 }, Screen::ScrollBackEnabled::No);

    put_text(screen, u8"abcde"
                     u8"fghij"
                     u8"$¢€𐍈 "
                     u8"pqrst"
                     u8"uvwxy"_sv);

    auto expected = Cursor {};

    screen.set_cursor(0, 0);
    ASSERT_EQ(screen.cursor(), expected);

    screen.set_cursor_col(2);
    expected = { .col = 2, .text_offset = 2 };
    ASSERT_EQ(screen.cursor(), expected);

    screen.set_cursor_col(1);
    expected = { .col = 1, .text_offset = 1 };
    ASSERT_EQ(screen.cursor(), expected);

    screen.set_cursor_row(2);
    expected = { .row = 2, .col = 1, .text_offset = 1 };
    ASSERT_EQ(screen.cursor(), expected);

    screen.set_cursor_col(100);
    expected = { .row = 2, .col = 4, .text_offset = 10 };
    ASSERT_EQ(screen.cursor(), expected);

    screen.set_cursor_row(1000);
    expected = { .row = 4, .col = 4, .text_offset = 4 };
    ASSERT_EQ(screen.cursor(), expected);

    screen.set_cursor(3, 2);
    expected = { .row = 3, .col = 2, .text_offset = 2 };
    ASSERT_EQ(screen.cursor(), expected);

    screen.set_cursor(1000, 1000);
    expected = { .row = 4, .col = 4, .text_offset = 4 };
    ASSERT_EQ(screen.cursor(), expected);

    screen.set_cursor(4, 4, true);
    ;
    expected = { .row = 4, .col = 4, .text_offset = 4, .overflow_pending = true };
    ASSERT_EQ(screen.cursor(), expected);

    screen.set_cursor(4, 4);
    expected = { .row = 4, .col = 4, .text_offset = 4 };
    ASSERT_EQ(screen.cursor(), expected);
}

static void origin_mode_cursor_movement() {
    auto screen = Screen({ 5, 5 }, Screen::ScrollBackEnabled::No);
    put_text(screen, u8"abcde"
                     u8"fghij"
                     u8"$¢€𐍈 "
                     u8"pqrst"
                     u8"uvwxy"_sv);

    screen.set_scroll_region({ 1, 4 });
    screen.set_origin_mode(OriginMode::Enabled);

    auto expected = Cursor { .row = 1, .col = 0 };
    ASSERT_EQ(screen.cursor(), expected);

    screen.set_cursor_relative(0, 0);
    ASSERT_EQ(screen.cursor(), expected);

    screen.set_cursor_col_relative(2);
    expected = { .row = 1, .col = 2, .text_offset = 2 };
    ASSERT_EQ(screen.cursor(), expected);

    screen.set_cursor_col_relative(1);
    expected = { .row = 1, .col = 1, .text_offset = 1 };
    ASSERT_EQ(screen.cursor(), expected);

    screen.set_cursor_row_relative(2);
    expected = { .row = 3, .col = 1, .text_offset = 1 };
    ASSERT_EQ(screen.cursor(), expected);

    screen.set_cursor_col_relative(100);
    expected = { .row = 3, .col = 4, .text_offset = 4 };
    ASSERT_EQ(screen.cursor(), expected);

    screen.set_cursor_row_relative(1000);
    expected = { .row = 3, .col = 4, .text_offset = 4 };
    ASSERT_EQ(screen.cursor(), expected);

    screen.set_cursor_relative(3, 2);
    expected = { .row = 3, .col = 2, .text_offset = 2 };
    ASSERT_EQ(screen.cursor(), expected);

    screen.set_cursor_relative(1000, 1000);
    expected = { .row = 3, .col = 4, .text_offset = 4 };
    ASSERT_EQ(screen.cursor(), expected);
}

static void clear_row() {
    auto screen = Screen({ 7, 5 }, Screen::ScrollBackEnabled::No);

    put_text(screen, u8"abcde"
                     u8"fghij"
                     u8"$¢€𐍈 "
                     u8"pq猫t"
                     u8"uv猫y"
                     u8"xx猫t"
                     u8"yy猫y"_sv);

    screen.set_cursor(0, 2, true);

    screen.clear_row_after_cursor();
    ASSERT_EQ(screen.cursor().text_offset, 2);
    ASSERT_EQ(screen.cursor().overflow_pending, false);

    screen.set_cursor(1, 2, true);

    screen.clear_row_before_cursor();
    ASSERT_EQ(screen.cursor().text_offset, 0);
    ASSERT_EQ(screen.cursor().overflow_pending, false);

    screen.set_cursor(2, 4, true);

    screen.clear_row();
    ASSERT_EQ(screen.cursor().text_offset, 0);
    ASSERT_EQ(screen.cursor().overflow_pending, false);

    screen.set_cursor(3, 2);
    screen.clear_row_after_cursor();
    ASSERT_EQ(screen.cursor().text_offset, 2);

    screen.set_cursor(4, 3);
    screen.clear_row_after_cursor();

    screen.set_cursor(5, 2);
    screen.clear_row_before_cursor();
    ASSERT_EQ(screen.cursor().text_offset, 0);

    screen.set_cursor(6, 3);
    screen.clear_row_before_cursor();
    ASSERT_EQ(screen.cursor().text_offset, 0);

    validate_text(screen, u8"ab   \n"
                          u8"   ij\n"
                          u8"     \n"
                          u8"pq   \n"
                          u8"uv   \n"
                          u8"    t\n"
                          u8"    y"_sv);
}

static void clear_screen() {
    auto screen = Screen({ 5, 5 }, Screen::ScrollBackEnabled::No);

    put_text(screen, u8"abcde"
                     u8"fghij"
                     u8"$¢€𐍈x"
                     u8"pqrst"
                     u8"uvwxy"_sv);

    screen.set_cursor(2, 2, true);

    screen.clear_before_cursor();
    ASSERT_EQ(screen.cursor().text_offset, 0);
    ASSERT_EQ(screen.cursor().overflow_pending, false);

    screen.set_cursor(3, 1, true);

    screen.clear_after_cursor();
    ASSERT_EQ(screen.cursor().text_offset, 1);
    ASSERT_EQ(screen.cursor().overflow_pending, false);

    validate_text(screen, u8"     \n"
                          u8"     \n"
                          u8"   𐍈x\n"
                          u8"p    \n"
                          u8"     "_sv);
}

static void clear_all() {
    auto screen = Screen({ 5, 5 }, Screen::ScrollBackEnabled::No);

    put_text(screen, u8"abcde"
                     u8"fghij"
                     u8"$¢€𐍈x"
                     u8"pqrst"
                     u8"uvwxy"_sv);

    screen.set_cursor(2, 2, true);

    screen.clear();
    ASSERT_EQ(screen.cursor().text_offset, 0);
    ASSERT_EQ(screen.cursor().overflow_pending, false);

    validate_text(screen, "     \n"
                          "     \n"
                          "     \n"
                          "     \n"
                          "     "_sv);
    validate_dirty(screen, "yyyyy\n"
                           "yyyyy\n"
                           "yyyyy\n"
                           "yyyyy\n"
                           "yyyyy"_sv);

    screen.clear();
    ASSERT_EQ(screen.cursor().text_offset, 0);
    ASSERT_EQ(screen.cursor().overflow_pending, false);

    validate_text(screen, "     \n"
                          "     \n"
                          "     \n"
                          "     \n"
                          "     "_sv);
    validate_dirty(screen, "nnnnn\n"
                           "nnnnn\n"
                           "nnnnn\n"
                           "nnnnn\n"
                           "nnnnn"_sv);
}

static void erase_characters() {
    auto screen = Screen({ 5, 5 }, Screen::ScrollBackEnabled::No);

    put_text(screen, u8"ab猫e"
                     u8"fg猫j"
                     u8"$¢€𐍈x"
                     u8"pqrst"
                     u8"uvwxy"_sv);

    screen.set_cursor(0, 2);
    screen.erase_characters(1);
    ASSERT_EQ(screen.cursor().text_offset, 2);

    screen.set_cursor(1, 3);
    screen.erase_characters(2);
    ASSERT_EQ(screen.cursor().text_offset, 2);

    screen.set_cursor(2, 2, true);

    screen.erase_characters(1);
    ASSERT_EQ(screen.cursor().text_offset, 3);
    ASSERT_EQ(screen.cursor().overflow_pending, false);

    screen.set_cursor(3, 1);
    screen.erase_characters(1000);

    validate_text(screen, u8"ab  e\n"
                          u8"fg   \n"
                          u8"$¢ 𐍈x\n"
                          u8"p    \n"
                          u8"uvwxy"_sv);
}

static void insert_blank_characters() {
    auto screen = Screen({ 5, 5 }, Screen::ScrollBackEnabled::No);

    put_text(screen, u8"abcde"
                     u8"fghij"
                     u8"$¢€𐍈x"
                     u8"p猫tv"
                     u8"u猫yz"_sv);

    auto expected = Cursor {};
    screen.set_cursor(0, 0, true);

    screen.insert_blank_characters(0); // No-op, but clears cursor overflow pending.
    ASSERT_EQ(screen.cursor(), expected);
    screen.insert_blank_characters(1);
    ASSERT_EQ(screen.cursor(), expected);

    screen.set_cursor(1, 1);
    screen.insert_blank_characters(2000000);
    expected = { .row = 1, .col = 1, .text_offset = 1 };
    ASSERT_EQ(screen.cursor(), expected);

    screen.set_cursor(2, 2);
    screen.insert_blank_characters(2);
    expected = { .row = 2, .col = 2, .text_offset = 3 };
    ASSERT_EQ(screen.cursor(), expected);

    screen.set_cursor(3, 1);
    screen.insert_blank_characters(3);
    ASSERT_EQ(screen.cursor().text_offset, 1);

    screen.set_cursor(4, 2);
    screen.insert_blank_characters(1);
    ASSERT_EQ(screen.cursor().text_offset, 1);

    validate_text(screen, u8" abcd\n"
                          u8"f    \n"
                          u8"$¢  €\n"
                          u8"p    \n"
                          u8"u   y"_sv);
}

static void delete_characters() {
    auto screen = Screen({ 5, 5 }, Screen::ScrollBackEnabled::No);

    put_text(screen, u8"abcde"
                     u8"fghij"
                     u8"$¢€𐍈x"
                     u8"p猫st"
                     u8"u猫xy"_sv);

    auto expected = Cursor {};
    screen.set_cursor(0, 0, true);
    ;
    screen.delete_characters(0); // No-op, but clears cursor overflow pending.
    ASSERT_EQ(screen.cursor(), expected);
    screen.delete_characters(1);
    ASSERT_EQ(screen.cursor(), expected);

    screen.set_cursor(1, 1);
    screen.delete_characters(2000000);
    expected = { .row = 1, .col = 1, .text_offset = 1 };
    ASSERT_EQ(screen.cursor(), expected);

    screen.set_cursor(2, 2);
    screen.delete_characters(2);
    expected = { .row = 2, .col = 2, .text_offset = 3 };
    ASSERT_EQ(screen.cursor(), expected);

    screen.set_cursor(3, 1);
    screen.delete_characters(1);
    ASSERT_EQ(screen.cursor().text_offset, 1);

    screen.set_cursor(4, 2);
    screen.delete_characters(2);
    ASSERT_EQ(screen.cursor().text_offset, 1);

    validate_text(screen, u8"bcde \n"
                          u8"f    \n"
                          u8"$¢x  \n"
                          u8"p st \n"
                          u8"u y  "_sv);
}

static void insert_blank_lines() {
    auto screen = Screen({ 5, 5 }, Screen::ScrollBackEnabled::No);

    put_text(screen, u8"abcde"
                     u8"fghij"
                     u8"$¢€𐍈x"
                     u8"pqrst"
                     u8"uvwxy"_sv);

    auto expected = Cursor {};
    screen.set_cursor(0, 4, true);
    ;
    screen.insert_blank_lines(0); // No-op, but sets the cursor the left margin.
    ASSERT_EQ(screen.cursor(), expected);
    screen.insert_blank_lines(1);
    ASSERT_EQ(screen.cursor(), expected);

    screen.set_cursor(3, 1);
    screen.insert_blank_lines(200000);
    expected = { .row = 3 };
    ASSERT_EQ(screen.cursor(), expected);

    validate_text(screen, u8"     \n"
                          u8"abcde\n"
                          u8"fghij\n"
                          u8"     \n"
                          u8"     "_sv);
}

static void vertical_scroll_region_insert_blank_lines() {
    auto screen = Screen({ 5, 2 }, Screen::ScrollBackEnabled::No);
    put_text(screen, "ab"
                     "cd"
                     "ef"
                     "gh"
                     "ij"_sv);
    screen.set_scroll_region({ 1, 4 });

    auto expected = Cursor {};
    screen.set_cursor(0, 0, true);
    ;
    screen.insert_blank_lines(1); // No-op, because outside scroll region. But overflow flag is cleared.
    ASSERT_EQ(screen.cursor(), expected);

    screen.set_cursor(2, 1);
    screen.insert_blank_lines(1);
    expected = { .row = 2 };
    ASSERT_EQ(screen.cursor(), expected);

    validate_text(screen, "ab\n"
                          "cd\n"
                          "  \n"
                          "ef\n"
                          "ij"_sv);
}

static void delete_lines() {
    auto screen = Screen({ 5, 5 }, Screen::ScrollBackEnabled::No);

    put_text(screen, u8"abcde"
                     u8"fghij"
                     u8"$¢€𐍈x"
                     u8"pqrst"
                     u8"uvwxy"_sv);

    auto expected = Cursor {};
    screen.set_cursor(0, 4, true);
    ;
    screen.delete_lines(0); // No-op, but sets the cursor the left margin.
    ASSERT_EQ(screen.cursor(), expected);
    screen.delete_lines(1);
    ASSERT_EQ(screen.cursor(), expected);

    screen.set_cursor(3, 1);
    screen.delete_lines(200000);
    expected = { .row = 3 };
    ASSERT_EQ(screen.cursor(), expected);

    validate_text(screen, u8"fghij\n"
                          u8"$¢€𐍈x\n"
                          u8"pqrst\n"
                          u8"     \n"
                          u8"     "_sv);
}

static void vertical_scroll_region_delete_lines() {
    auto screen = Screen({ 5, 2 }, Screen::ScrollBackEnabled::No);
    put_text(screen, "ab"
                     "cd"
                     "ef"
                     "gh"
                     "ij"_sv);
    screen.set_scroll_region({ 1, 4 });

    auto expected = Cursor {};
    screen.set_cursor(0, 0, true);
    ;
    screen.delete_lines(1); // No-op, because outside scroll region. But overflow flag is cleared.
    ASSERT_EQ(screen.cursor(), expected);

    screen.set_cursor(2, 1);
    screen.delete_lines(1);
    expected = { .row = 2 };
    ASSERT_EQ(screen.cursor(), expected);

    validate_text(screen, "ab\n"
                          "cd\n"
                          "gh\n"
                          "  \n"
                          "ij"_sv);
}

static void autowrap() {
    for (auto scroll_back_enabled : { Screen::ScrollBackEnabled::No, Screen::ScrollBackEnabled::Yes }) {
        auto screen = Screen({ 4, 2 }, scroll_back_enabled);

        put_text(screen, "ab"
                         "cd"
                         "ef"
                         "gh"
                         "ij"
                         "kl"_sv);

        auto expected = Cursor { .row = 3, .col = 1, .text_offset = 1, .overflow_pending = true };
        ASSERT_EQ(screen.cursor(), expected);

        if (scroll_back_enabled == Screen::ScrollBackEnabled::Yes) {
            validate_text(screen, "ab\n"
                                  "cd\n"
                                  "ef\n"
                                  "gh\n"
                                  "ij\n"
                                  "kl"_sv);

        } else {
            validate_text(screen, "ef\n"
                                  "gh\n"
                                  "ij\n"
                                  "kl"_sv);
        }
    }
}

static void vertical_scroll_region_autowrap() {
    for (auto scroll_back_enabled : { Screen::ScrollBackEnabled::No, Screen::ScrollBackEnabled::Yes }) {
        auto screen = Screen({ 4, 2 }, scroll_back_enabled);
        screen.set_scroll_region({ 1, 3 });

        put_text(screen, "ab"
                         "cd"
                         "ef"
                         "gh"
                         "ij"
                         "kl"_sv);

        auto expected = Cursor { .row = 2, .col = 1, .text_offset = 1, .overflow_pending = true };
        ASSERT_EQ(screen.cursor(), expected);

        if (scroll_back_enabled == Screen::ScrollBackEnabled::Yes) {
            validate_text(screen, "cd\n"
                                  "ef\n"
                                  "gh\n"
                                  "ab\n"
                                  "ij\n"
                                  "kl\n"
                                  "  "_sv);
        } else {
            validate_text(screen, "ab\n"
                                  "ij\n"
                                  "kl\n"
                                  "  "_sv);
        }
    }
}

static void save_restore_cursor() {
    auto screen = Screen({ 5, 2 }, Screen::ScrollBackEnabled::No);
    put_text(screen, "ab"
                     "cd"
                     "ef"
                     "gh"
                     "ij"_sv);

    auto save = screen.save_cursor();

    put_text(screen, u8"¢¢¢¢¢¢¢¢¢¢"_sv);
    screen.set_current_graphics_rendition({ .blink_mode = ttx::BlinkMode::Normal });
    screen.set_origin_mode(OriginMode::Enabled);

    screen.restore_cursor(save);

    auto expected_cursor = Cursor { .row = 4, .col = 1, .text_offset = 2, .overflow_pending = true };
    ASSERT_EQ(screen.cursor(), expected_cursor);
    ASSERT_EQ(screen.origin_mode(), OriginMode::Disabled);
    ASSERT_EQ(screen.current_graphics_rendition(), ttx::GraphicsRendition {});
}

TEST(screen, put_text_basic)
TEST(screen, put_text_unicode)
TEST(screen, put_text_wide)
TEST(screen, put_text_damage_tracking)
TEST(screen, selection)
TEST(screen, cursor_movement)
TEST(screen, origin_mode_cursor_movement)
TEST(screen, clear_row)
TEST(screen, clear_screen)
TEST(screen, clear_all)
TEST(screen, erase_characters)
TEST(screen, insert_blank_characters)
TEST(screen, delete_characters)
TEST(screen, insert_blank_lines)
TEST(screen, vertical_scroll_region_insert_blank_lines)
TEST(screen, delete_lines)
TEST(screen, vertical_scroll_region_delete_lines)
TEST(screen, autowrap)
TEST(screen, vertical_scroll_region_autowrap)
TEST(screen, save_restore_cursor)
}
