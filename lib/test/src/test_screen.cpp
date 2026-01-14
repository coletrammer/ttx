#include "di/random/prelude.h"
#include "di/test/prelude.h"
#include "dius/print.h"
#include "ttx/graphics_rendition.h"
#include "ttx/terminal/multi_cell_info.h"
#include "ttx/terminal/reflow_result.h"
#include "ttx/terminal/screen.h"
#include "ttx/terminal/selection.h"
#include "ttx/utf8_stream_decoder.h"

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
            auto [_, cell, text, _, _, _] = row;
            if (cell.is_nonprimary_in_multi_cell()) {
                text = "."_sv;
            } else if (text.empty()) {
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
                auto [_, cell, text, _, _, _] = row;
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
                auto [_, cell, text, _, _, _] = row;
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
            auto [_, cell, _, _, _, _] = data;

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

static void validate_bg(Screen& screen, di::StringView text) {
    ASSERT_EQ(screen.absolute_row_start(), 0);
    auto lines = text | di::split(U'\n');
    for (auto [i, line] : lines | di::enumerate) {
        ASSERT_EQ(di::distance(line), screen.max_width());

        auto [row_index, row_group] = screen.find_row(i);
        for (auto [ch, data] : di::zip(line, row_group.iterate_row(row_index))) {
            auto [_, cell, _, gfx, _, _] = data;

            auto bg = cell.background_only ? cell.background_color : gfx.bg;
            auto expected = [&] {
                switch (ch) {
                    case 'r':
                        return ttx::Color(ttx::Color::Palette::Red);
                    case 'b':
                        return ttx::Color(ttx::Color::Palette::Blue);
                    default:
                        return ttx::Color();
                }
            }();
            ASSERT_EQ(bg, expected);
        }
    }
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

    screen.set_cursor(2, 4);
    // # folowed by variation selector 16, should be rendered as 2 cells. This
    // should also wrap.
    put_text(screen, u8"#\uFE0F"_sv);

    validate_text(screen, u8"ab猫.e\n"
                          u8"abcd \n"
                          u8"1 xx \n"
                          u8"#\uFE0F|.|e| |2\n"
                          u8" 猫. e"_sv);

    // Another variation selector 16 at the start of the row this time.
    screen.set_cursor(1, 0);
    screen.clear_row();
    put_text(screen, u8"#\uFE0F"_sv);

    validate_text(screen, u8"ab猫.e\n"
                          u8"#\uFE0F|.| | | \n"
                          u8"1 xx \n"
                          u8"#\uFE0F|.|e| |2\n"
                          u8" 猫. e"_sv);

    // Putting a wide character at the end of a column overwrites
    // when not wrapping
    screen.set_cursor(4, 3);
    put_text(screen, "xy"_sv);
    screen.set_cursor(4, 4);
    screen.put_code_point(U'猫', AutoWrapMode::Disabled);
    cursor = screen.cursor();
    ASSERT_EQ(cursor.text_offset, 6);

    validate_text(screen, u8"ab猫.e\n"
                          u8"#\uFE0F|.| | | \n"
                          u8"1 xx \n"
                          u8"#\uFE0F|.|e| |2\n"
                          u8" 猫.猫."_sv);
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

static void put_text_random() {
    auto screen = Screen({ 2, 200 }, Screen::ScrollBackEnabled::Yes);
    auto decoder = ttx::Utf8StreamDecoder {};

    auto rng = di::MinstdRand(2);
    for (auto _ : di::range(1000000)) {
        auto byte = di::byte(di::UniformIntDistribution(0, 255)(rng));
        auto s = decoder.decode({ &byte, 1 });
        for (auto c : s) {
            screen.put_code_point(c, AutoWrapMode::Enabled);
        }
    }
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

    screen.set_current_graphics_rendition({ .bg = ttx::Color(ttx::Color::Palette::Red) });
    put_text(screen, u8"abcde"
                     u8"fghij"
                     u8"$¢€𐍈x"
                     u8"pqrst"
                     u8"uvwxy"_sv);

    screen.set_cursor(2, 2, true);

    screen.set_current_graphics_rendition({ .bg = ttx::Color(ttx::Color::Palette::Blue) });
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
    validate_bg(screen, "bbbbb\n"
                        "bbbbb\n"
                        "bbbrr\n"
                        "rbbbb\n"
                        "bbbbb"_sv);
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

static void reflow_basic() {
    auto screen = Screen({ 5, 5 }, Screen::ScrollBackEnabled::Yes);
    put_text(screen, "abcd\n"
                     "fghij"
                     "klmno"
                     "pqr\n"
                     "uvw"_sv);

    auto result = screen.resize({ 11, 2 });
    auto expected = ReflowResult();
    expected.add_offset({ 0, 2 }, 1, -2);
    expected.add_offset({ 0, 4 }, 1, -2);
    expected.add_offset({ 1, 0 }, 1, 0);
    expected.add_offset({ 1, 2 }, 2, -2);
    expected.add_offset({ 1, 4 }, 3, -4);
    expected.add_offset({ 2, 0 }, 2, 1);
    expected.add_offset({ 2, 1 }, 3, -1);
    expected.add_offset({ 2, 3 }, 4, -3);
    expected.add_offset({ 3, 0 }, 4, 0);
    expected.add_offset({ 3, 2 }, 5, -2);
    expected.add_offset({ 4, 0 }, 5, 0);
    expected.add_offset({ 4, 2 }, 6, -2);
    expected.add_offset({ 5, 0 }, 6, 0);
    ASSERT_EQ(result, expected);
    ASSERT_EQ(screen.cursor().row, 10);
    ASSERT_EQ(screen.cursor().col, 1);

    validate_text(screen, "ab\n"
                          "cd\n"
                          "fg\n"
                          "hi\n"
                          "jk\n"
                          "lm\n"
                          "no\n"
                          "pq\n"
                          "r \n"
                          "uv\n"
                          "w "_sv);

    result = screen.resize({ 4, 10 });
    expected = ReflowResult();
    expected.add_offset({ 1, 0 }, -1, 2);
    expected.add_offset({ 2, 0 }, -1, 0);
    expected.add_offset({ 3, 0 }, -2, 2);
    expected.add_offset({ 4, 0 }, -3, 4);
    expected.add_offset({ 5, 0 }, -4, 6);
    expected.add_offset({ 6, 0 }, -5, 8);
    expected.add_offset({ 7, 0 }, -5, 0);
    expected.add_offset({ 8, 0 }, -6, 2);
    expected.add_offset({ 9, 0 }, -6, 0);
    expected.add_offset({ 10, 0 }, -7, 2);
    expected.add_offset({ 11, 0 }, -7, 0);
    ASSERT_EQ(result, expected);
    ASSERT_EQ(screen.cursor().row, 3);
    ASSERT_EQ(screen.cursor().col, 3);

    validate_text(screen, "abcd      \n"
                          "fghijklmno\n"
                          "pqr       \n"
                          "uvw       "_sv);
}

static void reflow_wide() {
    auto screen = Screen({ 3, 5 }, Screen::ScrollBackEnabled::Yes);
    put_text(screen, u8"a猫cb\n"
                     u8"fgh猫"
                     u8"猫kl"_sv);

    auto result = screen.resize({ 8, 2 });
    auto expected = ReflowResult();
    expected.add_offset({ 0, 1 }, 1, -1);
    expected.add_offset({ 0, 3 }, 2, -3);
    expected.add_offset({ 1, 0 }, 2, 0);
    expected.add_offset({ 1, 2 }, 3, -2);
    expected.add_offset({ 1, 3 }, 4, -3);
    expected.add_offset({ 2, 0 }, 4, 0);
    expected.add_offset({ 2, 2 }, 5, -2);
    expected.add_offset({ 3, 0 }, 5, 0);
    ASSERT_EQ(result, expected);
    ASSERT_EQ(screen.cursor().row, 7);
    ASSERT_EQ(screen.cursor().col, 1);

    validate_text(screen, u8"a \n"
                          u8"猫.\n"
                          u8"cb\n"
                          u8"fg\n"
                          u8"h \n"
                          u8"猫.\n"
                          u8"猫.\n"
                          u8"kl"_sv);

    result = screen.resize({ 2, 9 });
    expected = ReflowResult();
    expected.add_offset({ 1, 0 }, -1, 1);
    expected.add_offset({ 2, 0 }, -2, 3);
    expected.add_offset({ 3, 0 }, -2, 0);
    expected.add_offset({ 4, 0 }, -3, 2);
    expected.add_offset({ 5, 0 }, -4, 3);
    expected.add_offset({ 6, 0 }, -5, 5);
    expected.add_offset({ 7, 0 }, -6, 7);
    expected.add_offset({ 8, 0 }, -6, 0);
    ASSERT_EQ(result, expected);
    ASSERT_EQ(screen.cursor().row, 1);
    ASSERT_EQ(screen.cursor().col, 8);

    validate_text(screen, "a猫.cb    \n"
                          u8"fgh猫.猫.k "_sv);
}

static void reflow_truncate() {
    auto screen = Screen({ 1, 11 }, Screen::ScrollBackEnabled::Yes);
    screen.put_code_point('x', AutoWrapMode::Enabled);
    screen.put_code_point('x', AutoWrapMode::Enabled);
    screen.put_code_point('x', AutoWrapMode::Enabled);
    screen.put_code_point('a', AutoWrapMode::Enabled);
    screen.put_code_point('b', AutoWrapMode::Enabled);
    screen.put_osc66(
        OSC66 {
            .info =
                MultiCellInfo {
                    .width = 4,
                },
            .text = "X"_sv,
        },
        AutoWrapMode::Enabled);
    screen.put_code_point('c', AutoWrapMode::Enabled);

    auto result = screen.resize({ 2, 3 });
    auto expected = ReflowResult();
    expected.add_offset({ 0, 3 }, 1, -3);
    expected.add_offset({ 0, 5 }, 1, 2, true);
    expected.add_offset({ 0, 9 }, 1, -7);
    expected.add_offset({ 1, 0 }, 1, 0);
    ASSERT_EQ(result, expected);
    ASSERT_EQ(screen.cursor().row, 1);
    ASSERT_EQ(screen.cursor().col, 2);

    validate_text(screen, "xxx\n"
                          "abc"_sv);
}

TEST(screen, put_text_basic)
TEST(screen, put_text_unicode)
TEST(screen, put_text_wide)
TEST(screen, put_text_damage_tracking)
TEST(screen, put_text_random)
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
TEST(screen, reflow_basic)
TEST(screen, reflow_wide)
TEST(screen, reflow_truncate)
}
