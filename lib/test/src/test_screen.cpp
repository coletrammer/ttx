#include "di/test/prelude.h"
#include "dius/print.h"
#include "ttx/graphics_rendition.h"
#include "ttx/terminal/screen.h"

namespace screen {
using namespace ttx::terminal;

static void put_text(Screen& screen, di::StringView text) {
    for (auto code_point : text) {
        screen.put_code_point(code_point, AutoWrapMode::Enabled);
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
                auto [_, _, text, _, _] = row;
                if (text.empty()) {
                    text = " "_sv;
                }

                ASSERT_EQ(text, expected);
            }
        } else {
            ASSERT_EQ(di::distance(line), screen.max_width());

            for (auto [ch, row] : di::zip(line, screen.iterate_row(i))) {
                auto [_, _, text, _, _] = row;
                if (text.empty()) {
                    text = " "_sv;
                }

                auto expected = di::String {};
                expected.push_back(ch);
                ASSERT_EQ(text, expected);
            }
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

    screen.set_cursor(0, 0);
    ASSERT_EQ(screen.cursor(), expected);

    screen.set_cursor_col(2);
    expected = { .row = 1, .col = 2, .text_offset = 2 };
    ASSERT_EQ(screen.cursor(), expected);

    screen.set_cursor_col(1);
    expected = { .row = 1, .col = 1, .text_offset = 1 };
    ASSERT_EQ(screen.cursor(), expected);

    screen.set_cursor_row(2);
    expected = { .row = 3, .col = 1, .text_offset = 1 };
    ASSERT_EQ(screen.cursor(), expected);

    screen.set_cursor_col(100);
    expected = { .row = 3, .col = 4, .text_offset = 4 };
    ASSERT_EQ(screen.cursor(), expected);

    screen.set_cursor_row(1000);
    expected = { .row = 3, .col = 4, .text_offset = 4 };
    ASSERT_EQ(screen.cursor(), expected);

    screen.set_cursor(3, 2);
    expected = { .row = 3, .col = 2, .text_offset = 2 };
    ASSERT_EQ(screen.cursor(), expected);

    screen.set_cursor(1000, 1000);
    expected = { .row = 3, .col = 4, .text_offset = 4 };
    ASSERT_EQ(screen.cursor(), expected);
}

static void clear_row() {
    auto screen = Screen({ 5, 5 }, Screen::ScrollBackEnabled::No);

    put_text(screen, u8"abcde"
                     u8"fghij"
                     u8"$¢€𐍈 "
                     u8"pqrst"
                     u8"uvwxy"_sv);

    screen.set_cursor(0, 2, true);
    ;
    screen.clear_row_after_cursor();
    ASSERT_EQ(screen.cursor().text_offset, 2);
    ASSERT_EQ(screen.cursor().overflow_pending, false);

    screen.set_cursor(1, 2, true);
    ;
    screen.clear_row_before_cursor();
    ASSERT_EQ(screen.cursor().text_offset, 0);
    ASSERT_EQ(screen.cursor().overflow_pending, false);

    screen.set_cursor(2, 4, true);
    ;
    screen.clear_row();
    ASSERT_EQ(screen.cursor().text_offset, 0);
    ASSERT_EQ(screen.cursor().overflow_pending, false);

    validate_text(screen, "ab   \n"
                          "   ij\n"
                          "     \n"
                          "pqrst\n"
                          "uvwxy"_sv);
}

static void clear_screen() {
    auto screen = Screen({ 5, 5 }, Screen::ScrollBackEnabled::No);

    put_text(screen, u8"abcde"
                     u8"fghij"
                     u8"$¢€𐍈x"
                     u8"pqrst"
                     u8"uvwxy"_sv);

    screen.set_cursor(2, 2, true);
    ;
    screen.clear_before_cursor();
    ASSERT_EQ(screen.cursor().text_offset, 0);
    ASSERT_EQ(screen.cursor().overflow_pending, false);

    screen.set_cursor(3, 1, true);
    ;
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
}

static void erase_characters() {
    auto screen = Screen({ 5, 5 }, Screen::ScrollBackEnabled::No);

    put_text(screen, u8"abcde"
                     u8"fghij"
                     u8"$¢€𐍈x"
                     u8"pqrst"
                     u8"uvwxy"_sv);

    screen.set_cursor(2, 2, true);

    screen.erase_characters(1);
    ASSERT_EQ(screen.cursor().text_offset, 3);
    ASSERT_EQ(screen.cursor().overflow_pending, false);

    screen.set_cursor(3, 1);
    screen.erase_characters(1000);

    validate_text(screen, u8"abcde\n"
                          u8"fghij\n"
                          u8"$¢ 𐍈x\n"
                          u8"p    \n"
                          u8"uvwxy"_sv);
}

static void insert_blank_characters() {
    auto screen = Screen({ 5, 5 }, Screen::ScrollBackEnabled::No);

    put_text(screen, u8"abcde"
                     u8"fghij"
                     u8"$¢€𐍈x"
                     u8"pqrst"
                     u8"uvwxy"_sv);

    auto expected = Cursor {};
    screen.set_cursor(0, 0, true);
    ;
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

    validate_text(screen, u8" abcd\n"
                          u8"f    \n"
                          u8"$¢  €\n"
                          u8"pqrst\n"
                          u8"uvwxy"_sv);
}

static void delete_characters() {
    auto screen = Screen({ 5, 5 }, Screen::ScrollBackEnabled::No);

    put_text(screen, u8"abcde"
                     u8"fghij"
                     u8"$¢€𐍈x"
                     u8"pqrst"
                     u8"uvwxy"_sv);

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

    validate_text(screen, u8"bcde \n"
                          u8"f    \n"
                          u8"$¢x  \n"
                          u8"pqrst\n"
                          u8"uvwxy"_sv);
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
