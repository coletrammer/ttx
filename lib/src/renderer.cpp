#include "ttx/renderer.h"

#include "di/container/tree/tree_set.h"
#include "di/io/vector_writer.h"
#include "di/io/writer_print.h"
#include "di/meta/constexpr.h"
#include "dius/print.h"
#include "dius/sync_file.h"
#include "dius/unicode/general_category.h"
#include "dius/unicode/grapheme_cluster.h"
#include "dius/unicode/name.h"
#include "dius/unicode/width.h"
#include "ttx/features.h"
#include "ttx/graphics_rendition.h"
#include "ttx/params.h"
#include "ttx/size.h"
#include "ttx/terminal/cursor.h"
#include "ttx/terminal/escapes/osc_52.h"
#include "ttx/terminal/escapes/osc_66.h"
#include "ttx/terminal/escapes/osc_8.h"
#include "ttx/terminal/multi_cell_info.h"
#include "ttx/terminal/screen.h"

namespace ttx {
auto Renderer::setup(dius::SyncFile& output, Feature features) -> di::Result<> {
    m_cleanup = {};
    m_features = features;

    auto buffer = di::VectorWriter<> {};

    // Setup - alternate screen buffer.
    di::writer_print<di::String::Encoding>(buffer, "\033[?1049h"_sv);
    m_cleanup.push_back("\033[?1049l\033[?25h"_s);

    // Setup - disable autowrap.
    di::writer_print<di::String::Encoding>(buffer, "\033[?7l"_sv);
    m_cleanup.push_back("\033[?7h"_s);

    // Setup - kitty key mode.
    if (!!(features & Feature::KittyKeyProtocol)) {
        di::writer_print<di::String::Encoding>(buffer, "\033[>31u"_sv);
        m_cleanup.push_back("\033[<u"_s);
    }

    // Setup - capture mouse events with shift held
    // Unfortunately, capturing shift mouse events prevents us from being able
    // to click on hyperlinks on Linux + Ghostty. On MacOS, command clicks can
    // be used. Other terminals ignore this escape anyway.
    // di::writer_print<di::String::Encoding>(buffer, "\033[>1s"_sv);
    // m_cleanup.push_back("\033[>0s"_s);

    // Setup - capture all mouse events and use SGR mosue reporting.
    di::writer_print<di::String::Encoding>(buffer, "\033[?1003h\033[?1006h"_sv);
    m_cleanup.push_back("\033[?1006l\033[?1003l"_s);

    // Setup - enable focus events.
    di::writer_print<di::String::Encoding>(buffer, "\033[?1004h"_sv);
    m_cleanup.push_back("\033[?1004l"_s);

    // Setup - bracketed paste.
    di::writer_print<di::String::Encoding>(buffer, "\033[?2004h"_sv);
    m_cleanup.push_back("\033[?2004l"_s);

    // Setup - grapheme cluster mode
    if (!!(features & Feature::GraphemeClusteringMode)) {
        di::writer_print<di::String::Encoding>(buffer, "\033[?2027h"_sv);
        m_cleanup.push_back("\033[?2027l"_s);
    }

    // Setup - requests clipboard to initialize state and determine if the host terminal
    // supports OSC 52. This is gated by the feature flag.
    if (!!(features & Feature::Clipboard)) {
        auto osc52 = terminal::OSC52 {};
        osc52.query = true;
        (void) osc52.selections.push_back(terminal::SelectionType::Clipboard);
        di::writer_print<di::String::Encoding>(buffer, "{}"_sv, osc52.serialize());
        osc52.selections[0] = terminal::SelectionType::Selection;
        di::writer_print<di::String::Encoding>(buffer, "{}"_sv, osc52.serialize());
    }

    // Setup - ensure the current and desired screens are fully cleared.
    m_current_screen.clear();
    m_desired_screen.clear();
    m_desired_screen.clear_damage_tracking();
    m_current_cursor = {};
    m_size_changed = true;

    auto text = di::move(buffer).vector();
    return output.write_exactly(di::as_bytes(text.span()));
}

auto Renderer::cleanup(dius::SyncFile& output) -> di::Result<> {
    auto buffer = di::VectorWriter<> {};
    for (auto const& string : m_cleanup | di::reverse) {
        di::writer_print<di::String::Encoding>(buffer, "{}"_sv, string);
    }
    m_cleanup.clear();

    auto text = di::move(buffer).vector();
    return output.write_exactly(di::as_bytes(text.span()));
}

void Renderer::start(Size const& size) {
    // If the size has changed, we need to flush all our state.
    if (m_size_changed || this->size() != size) {
        m_size_changed = true;
        m_current_screen.resize(size);
        m_current_screen.clear();
        m_desired_screen.resize(size);
        m_desired_screen.clear();
        m_desired_screen.clear_damage_tracking();
        m_current_cursor = {};
    }

    // Reset bounding box.
    m_row_offset = 0;
    m_col_offset = 0;
    m_bound_width = size.cols;
    m_bound_height = size.rows;
}

// The pending changes are stored in the difference between the current and desired screens. We must translate only
// the relevant changes into terminal escape sequences and write ot the output. We first need to collect all
// modified cells and store them in grouped list. The grouping is chosen to minimize the amount of bytes written (so
// the order is hyperlink > graphics > cursor position).
struct Change {
    u32 phase { 0 }; // Allow separating rendering into 2 phases.
    di::Optional<terminal::Hyperlink const&> hyperlink;
    GraphicsRendition const& graphics_rendition;
    u32 row { 0 };
    u32 col { 0 };
    di::StringView text;
    terminal::MultiCellInfo const& multi_cell_info;
    bool explicitly_sized { false };
    bool complex_grapheme_cluster { false };

    auto operator==(Change const& o) const -> bool {
        return di::tie(phase, hyperlink, graphics_rendition, row, col) ==
               di::tie(o.phase, o.hyperlink, o.graphics_rendition, o.row, o.col);
    };
    auto operator<=>(Change const& o) const {
        if (auto rv = phase <=> o.phase; rv != 0) {
            return rv;
        }
        if (phase == 0) {
            // In phase 0 sort entirely by position.
            return di::tie(row, col) <=> di::tie(o.row, o.col);
        }
        return di::tie(hyperlink, graphics_rendition, row, col) <=>
               di::tie(o.hyperlink, o.graphics_rendition, o.row, o.col);
    }
};

static void move_cursor(di::VectorWriter<>& buffer, u32 current_row, di::Optional<u32> current_col, u32 desired_row,
                        u32 desired_col) {
    // Optmizations: we want to move the cursor using the fewest number of bytes.
    // We have the following escapes available to us:
    //   \n  (C0) : row += 1
    //   \r  (C0) : col = 0
    //   CS  (C0) : col -= 1
    //   RI  (C1) : row -= 1
    //   CUU (CSI): row -= N
    //   CUD (CSI): row += N
    //   CUF (CSI): col += N
    //   CUB (CSI): col -= N
    //   CPL (CSI): row -= N, col = 0
    //   CNL (CSI): row += N, col = 0
    //   CUP (CSI): row = N+1, col = M+1
    //   CHA (CSI): col = N+1
    //   VPA (CSI): row = N+1

    // Instead of brute forcing the best sequence, which is possible but overly complex, we can some basic rules.
    // In particular, use \n and RI only when the row if off by 1. And use relative whenever possible, but falling
    // back on CUP.

    if (current_row == desired_row && current_col == desired_col) {
        return;
    }

    if (current_row == desired_row) {
        // Column only: Use \r if possible, otherwise relative positioning.
        if (desired_col == 0) {
            di::writer_print<di::String::Encoding>(buffer, "\r"_sv);
        } else if (!current_col) {
            // CHA
            di::writer_print<di::String::Encoding>(buffer, "\033[{}G"_sv, desired_col + 1);
        } else if (desired_col + 1 == current_col.value()) {
            // BS
            di::writer_print<di::String::Encoding>(buffer, "\x08"_sv);
        } else if (desired_col < current_col.value()) {
            // CUB
            di::writer_print<di::String::Encoding>(buffer, "\033[{}D"_sv, current_col.value() - desired_col);
        } else {
            // CUF
            di::writer_print<di::String::Encoding>(buffer, "\033[{}C"_sv, desired_col - current_col.value());
        }
        return;
    }

    if (current_col == desired_col) {
        // Row only: Use \n or RI if possible, otherwise relative movement.
        if (desired_row == current_row + 1) {
            di::writer_print<di::String::Encoding>(buffer, "\n"_sv);
        } else if (desired_row + 1 == current_row) {
            di::writer_print<di::String::Encoding>(buffer, "\033M"_sv);
        } else if (desired_row < current_row) {
            // CUU
            di::writer_print<di::String::Encoding>(buffer, "\033[{}A"_sv, current_row - desired_row);
        } else {
            // CUD
            di::writer_print<di::String::Encoding>(buffer, "\033[{}B"_sv, desired_row - current_row);
        }
        return;
    }

    if (desired_col == 0) {
        // Desired col = 0: Use CPL or CNL for relative movement.
        if (desired_row == 0) {
            di::writer_print<di::String::Encoding>(buffer, "\033[H"_sv);
        } else if (desired_row == current_row + 1) {
            di::writer_print<di::String::Encoding>(buffer, "\r\n"_sv);
        } else if (desired_row + 1 == current_row) {
            di::writer_print<di::String::Encoding>(buffer, "\r\033M"_sv);
        } else if (desired_row < current_row) {
            // CPL
            di::writer_print<di::String::Encoding>(buffer, "\033[{}F"_sv, current_row - desired_row);
        } else {
            // CNL
            di::writer_print<di::String::Encoding>(buffer, "\033[{}E"_sv, desired_row - current_row);
        }
        return;
    }

    // Row movement by 1: adjust the row and then adjust the column
    if (desired_row == current_row + 1) {
        move_cursor(buffer, desired_row, current_col, desired_row, desired_col);
        di::writer_print<di::String::Encoding>(buffer, "\n"_sv);
        return;
    }
    if (desired_row + 1 == current_row) {
        move_cursor(buffer, desired_row, current_col, desired_row, desired_col);
        di::writer_print<di::String::Encoding>(buffer, "\033M"_sv);
        return;
    }

    // Fallback:
    // CPA
    di::writer_print<di::String::Encoding>(buffer, "\033[{};{}H"_sv, desired_row + 1, desired_col + 1);
}

// Compute an upper bound on the width of the text to be rendered. Because different
// terminals render text at different widths, we need to be conservative and consider parts
// of the screen invalid after rendering the text.
static auto compute_text_upper_bound(di::StringView text) -> usize {
    // To be conservative, only consider Mn or Me characters as zero-width, instead of the usual
    // algorithm which considers all marks and Cf (format control) characters. This is to account
    // of variation in which Cf characters terminals consider to have width 0. This additionally
    // accounts for variations in how default non-presentable characters are handled.
    auto conservative_width = [](c32 code_point) -> u8 {
        auto width = dius::unicode::code_point_width(code_point).value_or(1);
        if (width == 0) {
            auto general_category = dius::unicode::general_category(code_point);
            if (general_category != dius::unicode::GeneralCategory::EnclosingMark &&
                general_category != dius::unicode::GeneralCategory::NonspacingMark) {
                width = 1;
            }
        }
        return width;
    };

    // Surprisingly, the width of text can be larger when measuring using graphemes instead of
    // computing the text width naively. This is because most terminals implementing grapheme
    // clustering on a grapheme boundary, even if the character has width 0. This palces a
    // width 0 character in a width 1 cell. Most width 0 characters are in the "Extend/SpacingMark"
    // class, so this wouldn't matter. But it does matter for other classes like "Prepend".
    // Additionally, some terminals assume all ASCII characters form boundaries (which mean
    // they mishandle "Prepend" code points followed by ASCII).
    //
    // This means the following text has larger width in grapheme mode, assuming the terminal
    // considers U+0600 a 0 width character.
    // "a<U+0600>b"
    //
    // In legacy mode, this should have width 2, by summing the width of all code points.
    // In kitty and our behavior, this has width 1, because <U+0600> has width and so combines
    // with a even though there is no grapheme boundary.
    // In a hypothetical terminal, this would have width 3, by placing each character in a different
    // cell. In practice, this terminals appear to differ in width here because of diagreement over
    // the width of <U+0600>, but this issue can affect more characters.
    //
    // For that reason, we compute a width using clustering and a width going code point by code
    // point. Additionally, we need to treat any character followed by variation selector 16 as
    // width 2, instead of extended pictographic characters with non-emoji default presentation.
    auto legacy_width = 0_usize;
    auto grapheme_width = 0_usize;
    auto clusterer = dius::unicode::GraphemeClusterer {};
    auto prev = 0;
    auto cluster_width = 0_u8;
    for (auto c : text) {
        auto w = conservative_width(c);
        if (c <= 127 || clusterer.is_boundary(c)) {
            grapheme_width += cluster_width;
            cluster_width = w;
            legacy_width += w;
            continue;
        }
        if (c == dius::unicode::VariationSelector_16) {
            legacy_width += 2 - prev;
            prev = 2;
            cluster_width = 2;
            continue;
        }
        legacy_width += w;
        cluster_width = di::max(cluster_width, w);
        prev = w;
    }
    grapheme_width += cluster_width;

    return di::max(legacy_width, grapheme_width);
}

static auto render_graphics_rendition(GraphicsRendition const& desired, Feature features,
                                      GraphicsRendition const& current) -> di::String {
    auto as_sgr = [](Params const& params) -> di::String {
        return *di::present("\033[{}m"_sv, params);
    };

    // For optimization, try both a delta graphics rendition as well as clearing the graphics rendition
    // completely.
    auto from_scratch = desired.as_csi_params(features) | di::transform(as_sgr) | di::join | di::to<di::String>();
    auto from_current =
        desired.as_csi_params(features, current) | di::transform(as_sgr) | di::join | di::to<di::String>();
    if (from_scratch.size_bytes() < from_current.size_bytes()) {
        return from_scratch;
    }
    return from_current;
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
auto Renderer::finish(dius::SyncFile& output, RenderedCursor const& cursor) -> di::Result<> {
    // List of changes which are used to determine what updates to the screen are needed. We
    // render changes in 2 phases to account for specific edge cases around cell sizing. Imagine
    // we have a terminal cell with text "🐈‍⬛". We will think this emoji has width 2, but its
    // possible the outer terminal will give it width 4. To handle these scenarios,
    // we need to always render the cells immediately following the potentially problematic
    // text so it truncated properly. If we fail to do this we may end up with stale cells
    // on subsequent renders, because we'll assume the cell if already up-to-date when in
    // reality it was overwritten by another cell.
    //
    // This is only needed in specific cases where the text requires explicit sizing and the
    // outer terminal doesn't support the text sizing protocol (OSC 66). However, when it is
    // needed, we need to establish an upper bound on the length of text. This is done using
    // the legacy algorithm of summing the code points widths across all code points. This can
    // actually still under count the text if the outer terminals thinks some code point is
    // larger than we do, but that's extremely unlikely. This is crucial for rendering in
    // older terminals as we unconditionally perform grapheme clustering ourselves.
    auto changes = di::TreeSet<Change> {};

    ASSERT_EQ(m_current_screen.absolute_row_start(), 0);
    ASSERT_EQ(m_desired_screen.absolute_row_start(), 0);
    ASSERT_EQ(m_desired_screen.size(), m_current_screen.size());
    auto [_, current_row_group] = m_current_screen.find_row(0);
    auto [_, desired_row_group] = m_desired_screen.find_row(0);
    for (auto row_index : di::range(size().rows)) {
        u32 force_change = 0;
        for (auto [current, desired] :
             di::zip(current_row_group.iterate_row(row_index), desired_row_group.iterate_row(row_index))) {
            auto _ = di::ScopeExit([&] {
                if (force_change > 0) {
                    force_change--;
                }
            });
            auto [col, current_cell, current_text, current_gfx, current_hyperlink, current_multi_cell_info] = current;
            auto [_, desired_cell, desired_text, desired_gfx, desired_hyperlink, desired_multi_cell_info] = desired;
            if (desired_cell.is_nonprimary_in_multi_cell()) {
                continue;
            }

            // Now detect a change. Order comparisons in order of likeliness.
            if (force_change > 0 || desired_text != current_text || desired_gfx != current_gfx ||
                desired_hyperlink != current_hyperlink || desired_multi_cell_info != current_multi_cell_info) {
                auto need_explicit_sizing =
                    desired_cell.explicitly_sized ||
                    (!(m_features & Feature::FullGraphemeClustering) && desired_cell.complex_grapheme_cluster);
                auto use_phase_0 = need_explicit_sizing && !(m_features & Feature::TextSizingWidth);
                changes.emplace(use_phase_0 ? 0 : 1, desired_hyperlink, desired_gfx, row_index, col, desired_text,
                                desired_multi_cell_info, desired_cell.explicitly_sized,
                                desired_cell.complex_grapheme_cluster);
                if (use_phase_0) {
                    // Force changes for the next N - M cells, where N is the correct width and M is
                    // the upper bound on the width.
                    auto upper_bound = compute_text_upper_bound(desired_text);
                    if (upper_bound > desired_multi_cell_info.compute_width()) {
                        force_change += upper_bound;
                    }
                }
            }
        }
    }

    // Start sequence: hide the cursor, begin synchronized updaes, and reset graphics/hyperlink state.
    auto buffer = di::VectorWriter<> {};
    if (!changes.empty()) {
        di::writer_print<di::String::Encoding>(buffer, "\033[?2026h"_sv);
    }
    if (m_current_cursor.transform(&RenderedCursor::hidden) != true) {
        di::writer_print<di::String::Encoding>(buffer, "\033[?25l"_sv);
    }
    if (di::exchange(m_size_changed, false)) {
        di::writer_print<di::String::Encoding>(buffer, "\033[H"_sv);
        m_current_screen.set_cursor(0, 0);

        di::writer_print<di::String::Encoding>(buffer, "\033[m"_sv);
        m_current_screen.set_current_graphics_rendition({});

        di::writer_print<di::String::Encoding>(buffer, terminal::OSC8().serialize());
        m_current_screen.set_current_hyperlink({});

        di::writer_print<di::String::Encoding>(buffer, "\033[2J"_sv);
    } else if (changes.empty() && cursor == m_current_cursor) {
        // No updates, so do nothing.
        return {};
    }

    // Now apply the changes. While we're iterating over all the changes,
    // also update the current terminal configuration.
    auto current_hyperlink = m_current_screen.current_hyperlink();
    auto current_gfx = m_current_screen.current_graphics_rendition();
    auto current_cursor_row = m_current_screen.cursor().row;
    auto current_cursor_col = di::Optional<u32>(m_current_screen.cursor().col);
    for (auto [_, hyperlink, gfx, row, col, text, multi_cell_info, explicitly_sized, complex_grapheme_cluster] :
         changes) {
        if (current_hyperlink != hyperlink) {
            m_current_screen.set_current_hyperlink(hyperlink);
            di::writer_print<di::String::Encoding>(buffer, terminal::OSC8::from_hyperlink(hyperlink).serialize());
            current_hyperlink = hyperlink;
        }
        if (current_gfx != gfx) {
            m_current_screen.set_current_graphics_rendition(gfx);
            di::writer_print<di::String::Encoding>(buffer, "{}"_sv,
                                                   render_graphics_rendition(gfx, m_features, current_gfx));
            current_gfx = gfx;
        }
        if (current_cursor_row != row || current_cursor_col != col) {
            m_current_screen.set_cursor(row, col);
            move_cursor(buffer, current_cursor_row, current_cursor_col, row, col);
            current_cursor_row = row;
            current_cursor_col = col;
        }

        // Update current screen with the new cell.
        m_current_screen.put_cell(text, multi_cell_info, terminal::AutoWrapMode::Disabled, explicitly_sized,
                                  complex_grapheme_cluster);

        // Write out the cell to the actual terminal
        // TODO: use erase character if there is no background color.
        if (text == ""_sv) {
            text = " "_sv;
        }

        // Time for actually rendering the text. The behavior varies greatly depending on
        // what features the terminal supports. We need to use explicit sizing if required
        // or the terminal disagrees with our grapheme clustering algorithm, and the text involved
        // a grapheme cluster with multiple non-zero width code points.
        auto need_explicit_sizing =
            explicitly_sized || (!(m_features & Feature::FullGraphemeClustering) && complex_grapheme_cluster);
        if (!!(m_features & Feature::TextSizingWidth)) {
            // This is the best case scenario, as we can control things precisely.
            // However, there are still a few different cases to consider.
            if (multi_cell_info != terminal::narrow_multi_cell_info &&
                multi_cell_info != terminal::wide_multi_cell_info && !!(m_features & Feature::TextSizingPresentation)) {
                // The outer terminal supports the fractional scale and alignment properties. We therefore can
                // actually use an OSC 66 sequence (meaning we don't need to drop the other attributes).
                //
                // We use explicit sizing if either the text itself was sized explicitly or full grapheme clustering
                // is not handled in the terminal and the text involved a grapheme break.
                auto info = multi_cell_info;
                if (!need_explicit_sizing) {
                    info.width = 0;
                }
                auto osc66 = terminal::OSC66(info, text);
                di::writer_print<di::String::Encoding>(buffer, "{}"_sv, osc66.serialize());
            } else if (need_explicit_sizing) {
                // We need to explicitly size the cell.
                auto osc66 = terminal::OSC66({ .width = multi_cell_info.compute_width() }, text);
                di::writer_print<di::String::Encoding>(buffer, "{}"_sv, osc66.serialize());
            } else {
                // We can rely on normal text writing behavior.
                di::writer_print<di::String::Encoding>(buffer, "{}"_sv, text);
            }
        } else if (need_explicit_sizing) {
            // We need explicit sizing but are getting no help from the terminal.
            // In this case we first render space characters to ensure the graphics
            // attributes and previous cell contents are cleared, and then render the
            // text after moving the cursor back. At this point, we also forget the
            // current cursor position, as we have no idea how wide the text will
            // actually be.
            for (auto _ : di::range(multi_cell_info.compute_width())) {
                di::writer_print<di::String::Encoding>(buffer, " "_sv);
            }
            move_cursor(buffer, row, di::min(col + multi_cell_info.compute_width(), size().cols - 1), row, col);
            di::writer_print<di::String::Encoding>(buffer, "{}"_sv, text);
            current_cursor_col = {};
        } else {
            // Just render the text. The text should be simple and the terminal will understand it properly.
            di::writer_print<di::String::Encoding>(buffer, "{}"_sv, text);
        }

        if (current_cursor_col.has_value()) {
            current_cursor_col.value() += multi_cell_info.compute_width();

            // Forget cursor column on overflow. For some reason not doing this causes issues with kitty.
            // This implies relative cursor movement with kitty counts the pending overflow flag as a column?
            if (current_cursor_col.value() >= size().cols) {
                current_cursor_col = {};
            }
        }
    }

    // End sequence: Move cursor to the correct location, maybe show the cursor,
    // as well as end the synchronized output.
    m_current_screen.set_cursor(cursor.cursor_row, cursor.cursor_col);
    move_cursor(buffer, current_cursor_row, current_cursor_col, cursor.cursor_row, cursor.cursor_col);

    if (m_current_cursor.transform(&RenderedCursor::style) != cursor.style) {
        di::writer_print<di::String::Encoding>(buffer, "\033[{} q"_sv, i32(cursor.style));
    }

    if (!cursor.hidden) {
        di::writer_print<di::String::Encoding>(buffer, "\033[?25h"_sv);
    }
    m_current_cursor = cursor;
    if (!changes.empty()) {
        di::writer_print<di::String::Encoding>(buffer, "\033[?2026l"_sv);
    }

    auto text = di::move(buffer).vector();
    return output.write_exactly(di::as_bytes(text.span()));
}

void Renderer::put_text(di::StringView text, u32 row, u32 col, GraphicsRendition const& rendition,
                        di::Optional<terminal::Hyperlink const&> hyperlink) {
    if (col >= m_bound_width || row >= m_bound_height) {
        return;
    }

    // TODO: have sane truncation behavior.
    row += m_row_offset;
    col += m_col_offset;

    m_desired_screen.set_cursor(row, col);
    m_desired_screen.set_current_graphics_rendition(rendition);
    m_desired_screen.set_current_hyperlink(hyperlink);
    for (auto ch : text) {
        m_desired_screen.put_code_point(ch, terminal::AutoWrapMode::Disabled);
        if (m_desired_screen.cursor().col >= m_col_offset + m_bound_width) {
            break;
        }
    }
}

void Renderer::put_text(c32 text, u32 row, u32 col, GraphicsRendition const& rendition,
                        di::Optional<terminal::Hyperlink const&> hyperlink) {
    auto string = di::container::string::StringImpl<di::String::Encoding, di::StaticVector<c8, di::Constexpr<4zu>>> {};
    (void) string.push_back(text);
    put_text(string.view(), row, col, rendition, hyperlink);
}

void Renderer::put_cell(di::StringView text, u32 row, u32 col, GraphicsRendition const& rendition,
                        di::Optional<terminal::Hyperlink const&> hyperlink,
                        terminal::MultiCellInfo const& multi_cell_info, bool explicitly_sized,
                        bool complex_grapheme_cluster) {
    if (col >= m_bound_width || row >= m_bound_height) {
        return;
    }

    // If the entire multi-cell doesn't fit, replace it with blanks.
    if (col + multi_cell_info.compute_width() > m_bound_width) {
        m_desired_screen.set_cursor(row + m_row_offset, col + m_col_offset);
        m_desired_screen.erase_characters(m_bound_width - col);
        return;
    }

    row += m_row_offset;
    col += m_col_offset;

    m_desired_screen.set_cursor(row, col);
    m_desired_screen.set_current_graphics_rendition(rendition);
    m_desired_screen.set_current_hyperlink(hyperlink);
    m_desired_screen.put_cell(text, multi_cell_info, terminal::AutoWrapMode::Disabled, explicitly_sized,
                              complex_grapheme_cluster);
}

void Renderer::clear_row(u32 row, GraphicsRendition const& rendition,
                         di::Optional<terminal::Hyperlink const&> hyperlink) {
    if (row >= m_bound_height) {
        return;
    }

    for (auto c : di::range(m_bound_width)) {
        put_text(U' ', row, c, rendition, hyperlink);
    }
}

void Renderer::set_bound(u32 row, u32 col, u32 width, u32 height) {
    m_row_offset = row;
    m_col_offset = col;
    m_bound_width = width;
    m_bound_height = height;
}
}
