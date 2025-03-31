#include "ttx/terminal.h"

#include "di/container/algorithm/contains.h"
#include "di/serialization/base64.h"
#include "di/util/construct.h"
#include "dius/print.h"
#include "ttx/cursor_style.h"
#include "ttx/escape_sequence_parser.h"
#include "ttx/focus_event_io.h"
#include "ttx/graphics_rendition.h"
#include "ttx/key_event_io.h"
#include "ttx/mouse_event_io.h"
#include "ttx/params.h"
#include "ttx/paste_event_io.h"

namespace ttx {
Terminal::Terminal(dius::SyncFile& psuedo_terminal, Size const& size)
    : m_primary_screen(size, terminal::Screen::ScrollBackEnabled::No), m_psuedo_terminal(psuedo_terminal) {}

void Terminal::on_parser_results(di::Span<ParserResult const> results) {
    for (auto const& result : results) {
        di::visit(
            [&](auto const& r) {
                this->on_parser_result(r);
            },
            result);
    }
}

void Terminal::on_parser_result(PrintableCharacter const& Printable_character) {
    if (Printable_character.code_point < 0x7F || Printable_character.code_point > 0x9F) {
        put_char(Printable_character.code_point);
    }
}

void Terminal::on_parser_result(DCS const& dcs) {
    if (dcs.intermediate == "$q"_sv) {
        dcs_decrqss(dcs.params, dcs.data);
        return;
    }
}

void Terminal::on_parser_result(OSC const& osc) {
    auto ps_end = osc.data.find(';');
    if (!ps_end) {
        return;
    }

    auto ps = osc.data.substr(osc.data.begin(), ps_end.begin());
    if (ps == "52"_sv) {
        osc_52(osc.data.substr(ps_end.end()));
        return;
    }
}

void Terminal::on_parser_result(APC const&) {}

void Terminal::on_parser_result(ControlCharacter const& control_character) {
    switch (control_character.code_point) {
        case 8: {
            c0_bs();
            return;
        }
        case '\a':
            return;
        case '\t': {
            c0_ht();
            return;
        }
        case '\n': {
            c0_lf();
            return;
        }
        case '\v': {
            c0_vt();
            return;
        }
        case '\f': {
            c0_ff();
            return;
        }
        case '\r': {
            c0_cr();
            return;
        }
        default:
            return;
    }
}

void Terminal::on_parser_result(CSI const& csi) {
    if (csi.intermediate == "?$"_sv) {
        switch (csi.terminator) {
            case 'p': {
                csi_decrqm(csi.params);
                return;
            }
            default:
                return;
        }
    }

    if (csi.intermediate == "="_sv) {
        switch (csi.terminator) {
            case 'c': {
                csi_da3(csi.params);
                return;
            }
            case 'u': {
                csi_set_key_reporting_flags(csi.params);
                return;
            }
            default:
                return;
        }
    }

    if (csi.intermediate == ">"_sv) {
        switch (csi.terminator) {
            case 'c': {
                csi_da2(csi.params);
                return;
            }
            case 'u': {
                csi_push_key_reporting_flags(csi.params);
                return;
            }
            default:
                return;
        }
    }

    if (csi.intermediate == "<"_sv) {
        switch (csi.terminator) {
            case 'u': {
                csi_pop_key_reporting_flags(csi.params);
                return;
            }
            default:
                return;
        }
    }

    if (csi.intermediate == "?"_sv) {
        switch (csi.terminator) {
            case 'h': {
                csi_decset(csi.params);
                return;
            }
            case 'l': {
                csi_decrst(csi.params);
                return;
            }
            case 'u': {
                csi_get_key_reporting_flags(csi.params);
                return;
            }
            default:
                return;
        }
    }

    if (csi.intermediate == " "_sv) {
        switch (csi.terminator) {
            case 'q': {
                csi_decscusr(csi.params);
                return;
            }
            default:
                return;
        }
    }

    if (!csi.intermediate.empty()) {
        return;
    }

    switch (csi.terminator) {
        case '@': {
            csi_ich(csi.params);
            return;
        }
        case 'A': {
            csi_cuu(csi.params);
            return;
        }
        case 'B': {
            csi_cud(csi.params);
            return;
        }
        case 'C': {
            csi_cuf(csi.params);
            return;
        }
        case 'D': {
            csi_cub(csi.params);
            return;
        }
        case 'G': {
            csi_cha(csi.params);
            return;
        }
        case 'H': {
            csi_cup(csi.params);
            return;
        }
        case 'J': {
            csi_ed(csi.params);
            return;
        }
        case 'K': {
            csi_el(csi.params);
            return;
        }
        case 'L': {
            csi_il(csi.params);
            return;
        }
        case 'M': {
            csi_dl(csi.params);
            return;
        }
        case 'P': {
            csi_dch(csi.params);
            return;
        }
        case 'S': {
            csi_su(csi.params);
            return;
        }
        case 'T': {
            csi_sd(csi.params);
            return;
        }
        case 'X': {
            csi_ech(csi.params);
            return;
        }
        case 'b': {
            csi_rep(csi.params);
            return;
        }
        case 'c': {
            csi_da1(csi.params);
            return;
        }
        case 'd': {
            csi_vpa(csi.params);
            return;
        }
        case 'f': {
            csi_hvp(csi.params);
            return;
        }
        case 'g': {
            csi_tbc(csi.params);
            return;
        }
        case 'm': {
            csi_sgr(csi.params);
            return;
        }
        case 'n': {
            csi_dsr(csi.params);
            return;
        }
        case 'r': {
            csi_decstbm(csi.params);
            return;
        }
        case 's': {
            csi_scosc(csi.params);
            return;
        }
        case 't': {
            csi_xtwinops(csi.params);
            return;
        }
        case 'u': {
            csi_scorc(csi.params);
            return;
        }
        default:
            return;
    }
}

void Terminal::on_parser_result(Escape const& escape) {
    if (escape.intermediate == "#"_sv) {
        switch (escape.terminator) {
            case '8': {
                esc_decaln();
                return;
            }
            default:
                return;
        }
    }

    if (!escape.intermediate.empty()) {
        return;
    }

    switch (escape.terminator) {
        case '7': {
            esc_decsc();
            return;
        }
        case '8': {
            esc_decrc();
            return;
        }
        // 8 bit control characters
        case 'D': {
            c1_ind();
            return;
        }
        case 'E': {
            c1_nel();
            return;
        }
        case 'H': {
            c1_hts();
            return;
        }
        case 'M': {
            c1_ri();
            return;
        }
        default:
            return;
    }
}

// Backspace - https://vt100.net/docs/vt510-rm/chapter4.html#T4-1
void Terminal::c0_bs() {
    auto col = cursor_col();
    if (cursor_col() > 0) {
        active_screen().screen.set_cursor_col(col - 1);
    }
}

// Horizontal Tab - https://vt100.net/docs/vt510-rm/chapter4.html#T4-1
void Terminal::c0_ht() {
    for (auto tab_stop : m_tab_stops) {
        if (tab_stop > cursor_col()) {
            active_screen().screen.set_cursor_col(tab_stop);
            return;
        }
    }

    active_screen().screen.set_cursor_col(max_col_inclusive());
}

// Line Feed - https://vt100.net/docs/vt510-rm/chapter4.html#T4-1
void Terminal::c0_lf() {
    if (cursor_row() + 1 == row_count()) {
        active_screen().screen.scroll_down();
    } else {
        active_screen().screen.set_cursor_row(cursor_row() + 1);
    }
}

// Vertical Tab - https://vt100.net/docs/vt510-rm/chapter4.html#T4-1
void Terminal::c0_vt() {
    c0_lf();
}

// Form Feed - https://vt100.net/docs/vt510-rm/chapter4.html#T4-1
void Terminal::c0_ff() {
    c0_lf();
}

// Carriage Return - https://vt100.net/docs/vt510-rm/chapter4.html#T4-1
void Terminal::c0_cr() {
    active_screen().screen.set_cursor_col(0);
}

// Index - https://vt100.net/docs/vt510-rm/IND.html
void Terminal::c1_ind() {
    c0_lf();
}

// Next Line - https://vt100.net/docs/vt510-rm/NEL.html
void Terminal::c1_nel() {
    c0_cr();
    c0_lf();
}

// Horizontal Tab Set - https://vt100.net/docs/vt510-rm/HTS.html
void Terminal::c1_hts() {
    if (di::contains(m_tab_stops, cursor_col())) {
        return;
    }

    auto index = 0_usize;
    for (; index < m_tab_stops.size(); index++) {
        if (cursor_col() < m_tab_stops[index]) {
            break;
        }
    }

    m_tab_stops.insert(m_tab_stops.begin() + index, cursor_col());
}

// Reverse Index - https://www.vt100.net/docs/vt100-ug/chapter3.html#RI
void Terminal::c1_ri() {
    if (cursor_row() == 0) {
        // Scroll down.
        csi_sd({});
        return;
    }

    active_screen().screen.set_cursor_row(cursor_row() - 1);
}

// Request Status String - https://vt100.net/docs/vt510-rm/DECRQSS.html
void Terminal::dcs_decrqss(Params const&, di::StringView data) {
    // Set graphics rendition
    if (data == "m"_sv) {
        auto sgr_string = active_screen().screen.current_graphics_rendition().as_csi_params() |
                          di::transform(di::to_string) | di::join_with(U';') | di::to<di::String>();
        (void) m_psuedo_terminal.write_exactly(di::as_bytes(di::present("\033P1$r{}m\033\\"_sv, sgr_string)->span()));
    } else {
        (void) m_psuedo_terminal.write_exactly(di::as_bytes("\033P0$r\033\\"_sv.span()));
    }
}

// OSC 52 - https://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h3-Operating-System-Commands
void Terminal::osc_52(di::StringView data) {
    // Data is of the form: Pc ; Pd
    auto pc_end = data.find(';');
    if (!pc_end) {
        return;
    }

    // For now, just ignore which selection is asked for (the Pc field).
    auto pd = data.substr(pc_end.end());
    if (pd == "?"_sv) {
        // TODO: respond with the actual clipboard contents.
        return;
    }

    auto clipboard_data = di::parse<di::Base64<>>(pd);
    if (!clipboard_data) {
        return;
    }

    m_outgoing_events.emplace_back(SetClipboard(di::move(clipboard_data).value().container()));
}

// DEC Screen Alignment Pattern - https://vt100.net/docs/vt510-rm/DECALN.html
void Terminal::esc_decaln() {
    // TODO: fill the screen with the character 'E'
    // clear('E');
    active_screen().screen.set_cursor(0, 0);
}

// DEC Save Cursor - https://vt100.net/docs/vt510-rm/DECSC.html
void Terminal::esc_decsc() {
    auto& screen_state = active_screen();
    screen_state.saved_cursor = screen_state.screen.save_cursor();
}

// DEC Restore Cursor - https://vt100.net/docs/vt510-rm/DECRC.html
void Terminal::esc_decrc() {
    auto& screen_state = active_screen();
    if (screen_state.saved_cursor) {
        screen_state.screen.restore_cursor(screen_state.saved_cursor.value());
        screen_state.saved_cursor = {};
    }
}

// Insert Character - https://vt100.net/docs/vt510-rm/ICH.html
void Terminal::csi_ich(Params const& params) {
    auto chars = di::max(1u, params.get(0, 1));
    active_screen().screen.insert_blank_characters(chars);
}

// Cursor Up - https://www.vt100.net/docs/vt100-ug/chapter3.html#CUU
void Terminal::csi_cuu(Params const& params) {
    auto delta_row = di::max(1u, params.get(0, 1));
    auto new_row = delta_row > cursor_row() ? 0 : cursor_row() - delta_row;
    active_screen().screen.set_cursor_row(new_row);
}

// Cursor Down - https://www.vt100.net/docs/vt100-ug/chapter3.html#CUD
void Terminal::csi_cud(Params const& params) {
    auto delta_row = di::max(1u, params.get(0, 1));
    auto new_row = di::Checked(cursor_row()) + delta_row;
    active_screen().screen.set_cursor_row(new_row.value().value_or(di::NumericLimits<u32>::max));
}

// Cursor Forward - https://www.vt100.net/docs/vt100-ug/chapter3.html#CUF
void Terminal::csi_cuf(Params const& params) {
    auto delta_col = di::max(1u, params.get(0, 1));
    auto new_col = di::Checked(cursor_col()) + delta_col;
    active_screen().screen.set_cursor_col(new_col.value().value_or(di::NumericLimits<u32>::max));
}

// Cursor Backward - https://www.vt100.net/docs/vt100-ug/chapter3.html#CUB
void Terminal::csi_cub(Params const& params) {
    auto delta_col = di::max(1u, params.get(0, 1));
    auto new_col = delta_col > cursor_col() ? 0 : cursor_col() - delta_col;
    active_screen().screen.set_cursor_col(new_col);
}

// Cursor Position - https://www.vt100.net/docs/vt100-ug/chapter3.html#CUP
void Terminal::csi_cup(Params const& params) {
    auto row = di::max(1u, params.get(0, 1)) - 1;
    auto col = di::max(1u, params.get(1, 1)) - 1;
    active_screen().screen.set_cursor(row, col);
}

// Cursor Horizontal Absolute - https://vt100.net/docs/vt510-rm/CHA.html
void Terminal::csi_cha(Params const& params) {
    auto col = di::max(1u, params.get(0, 1)) - 1;
    active_screen().screen.set_cursor_col(col);
}

// Erase in Display - https://vt100.net/docs/vt510-rm/ED.html
void Terminal::csi_ed(Params const& params) {
    switch (params.get(0, 0)) {
        case 0: {
            active_screen().screen.clear_after_cursor();
            return;
        }
        case 1: {
            active_screen().screen.clear_before_cursor();
            return;
        }
        case 2: {
            clear();
            return;
        }
        case 3:
            // XTerm extension to clear scoll buffer
            clear();
            return;
        default:
            return;
    }
}

// Erase in Line - https://vt100.net/docs/vt510-rm/EL.html
void Terminal::csi_el(Params const& params) {
    switch (params.get(0, 0)) {
        case 0: {
            active_screen().screen.clear_row_after_cursor();
            return;
        }
        case 1: {
            active_screen().screen.clear_row_before_cursor();
            return;
        }
        case 2: {
            active_screen().screen.clear_row();
            return;
        }
        default:
            return;
    }
}

// Insert Line - https://vt100.net/docs/vt510-rm/IL.html
void Terminal::csi_il(Params const& params) {
    u32 lines_to_insert = params.get(0, 1);
    active_screen().screen.insert_blank_lines(lines_to_insert);
}

// Delete Line - https://vt100.net/docs/vt510-rm/DL.html
void Terminal::csi_dl(Params const& params) {
    u32 lines_to_delete = params.get(0, 1);
    active_screen().screen.delete_lines(lines_to_delete);
}

// Delete Character - https://vt100.net/docs/vt510-rm/DCH.html
void Terminal::csi_dch(Params const& params) {
    u32 chars_to_delete = params.get(0, 1);
    active_screen().screen.delete_characters(chars_to_delete);
}

// Pan Down - https://vt100.net/docs/vt510-rm/SU.html
void Terminal::csi_su(Params const& params) {
    // Pan down (scroll up) is equivalent to delete lines when the cursor is at the top
    // left of the scroll region, and the cursor is fully preserved.
    u32 to_scroll = params.get(0, 1);

    auto& screen = active_screen().screen;
    auto save = screen.save_cursor();
    auto _ = di::ScopeExit([&] {
        screen.restore_cursor(save);
    });

    // TODO: scroll regions.
    screen.set_cursor(0, 0);
    screen.delete_lines(to_scroll);
}

// Pan Up - https://vt100.net/docs/vt510-rm/SD.html
void Terminal::csi_sd(Params const& params) {
    // Pan up (scroll down) is equivalent to insert lines when the cursor is at the top
    // left of the scroll region, and the cursor is fully preserved.
    u32 to_scroll = params.get(0, 1);

    auto& screen = active_screen().screen;
    auto save = screen.save_cursor();
    auto _ = di::ScopeExit([&] {
        screen.restore_cursor(save);
    });

    // TODO: scroll regions.
    screen.set_cursor(0, 0);
    screen.insert_blank_lines(to_scroll);
}

// Erase Character - https://vt100.net/docs/vt510-rm/ECH.html
void Terminal::csi_ech(Params const& params) {
    u32 chars_to_erase = di::max(1u, params.get(0, 1));
    // TODO: this overwrites characters unlike dch.
}

// Repeat Preceding Graphic Character - https://invisible-island.net/xterm/ctlseqs/ctlseqs.html
void Terminal::csi_rep(Params const& params) {
    // c32 preceding_character = ' ';
    // if (m_cursor_col == 0) {
    //     if (m_cursor_row != 0) {
    //         preceding_character = m_rows[m_cursor_row - 1][m_col_count - 1].ch;
    //     }
    // } else {
    //     preceding_character = m_rows[m_cursor_row][m_cursor_col - 1].ch;
    // }
    // TODO: find the previous character - seems like we should use the last
    // argument passed to put_char().
    // for (auto i = 0_u32; i < params.get(0, 0); i++) {
    //     put_char(preceding_character);
    // }
}

// Primary Device Attributes - https://vt100.net/docs/vt510-rm/DA1.html
void Terminal::csi_da1(Params const& params) {
    if (params.get(0, 0) != 0) {
        return;
    }
    (void) m_psuedo_terminal.write_exactly(di::as_bytes("\033[?1;0c"_sv.span()));
}

// Secondary Device Attributes - https://vt100.net/docs/vt510-rm/DA2.html
void Terminal::csi_da2(Params const& params) {
    if (params.get(0, 0) != 0) {
        return;
    }
    (void) m_psuedo_terminal.write_exactly(di::as_bytes("\033[>010;0c"_sv.span()));
}

// Tertiary Device Attributes - https://vt100.net/docs/vt510-rm/DA3.html
void Terminal::csi_da3(Params const& params) {
    if (params.get(0, 0) != 0) {
        return;
    }
    (void) m_psuedo_terminal.write_exactly(di::as_bytes("\033P!|00000000\033\\"_sv.span()));
}

// Vertical Line Position Absolute - https://vt100.net/docs/vt510-rm/VPA.html
void Terminal::csi_vpa(Params const& params) {
    auto row = di::max(1u, params.get(0, 1)) - 1;
    active_screen().screen.set_cursor_row(row);
}

// Horizontal and Vertical Position - https://vt100.net/docs/vt510-rm/HVP.html
void Terminal::csi_hvp(Params const& params) {
    csi_cup(params);
}

// Tab Clear - https://vt100.net/docs/vt510-rm/TBC.html
void Terminal::csi_tbc(Params const& params) {
    switch (params.get(0, 0)) {
        case 0:
            di::erase_if(m_tab_stops, [this](auto x) {
                return x == cursor_col();
            });
            return;
        case 3:
            m_tab_stops.clear();
            return;
        default:
            return;
    }
}

// DEC Private Mode Set - https://invisible-island.net/xterm/ctlseqs/ctlseqs.html
void Terminal::csi_decset(Params const& params) {
    switch (params.get(0, 0)) {
        case 1:
            // Cursor Keys Mode - https://vt100.net/docs/vt510-rm/DECCKM.html
            m_application_cursor_keys_mode = ApplicationCursorKeysMode::Enabled;
            break;
        case 3:
            // Select 80 or 132 Columns per Page - https://vt100.net/docs/vt510-rm/DECCOLM.html
            if (m_allow_80_132_col_mode) {
                m_80_col_mode = false;
                m_132_col_mode = true;
                resize({ row_count(), 132, m_available_size.xpixels * 132 / m_available_size.cols, size().ypixels });
                clear();
                csi_decstbm({});
            }
            break;
        case 6:
            // Origin Mode - https://vt100.net/docs/vt510-rm/DECOM.html
            m_origin_mode = true;
            // TODO: set cursor and handle origin mode.
            // set_cursor(m_cursor_row, m_cursor_col);
            break;
        case 7:
            // Autowrap mode - https://vt100.net/docs/vt510-rm/DECAWM.html
            m_autowrap_mode = true;
            break;
        case 9:
            m_mouse_protocol = MouseProtocol::X10;
            break;
        case 25:
            // Text Cursor Enable Mode - https://vt100.net/docs/vt510-rm/DECTCEM.html
            m_cursor_hidden = false;
            break;
        case 40:
            m_allow_80_132_col_mode = true;
            break;
        case 1000:
            m_mouse_protocol = MouseProtocol::VT200;
            break;
        case 1002:
            m_mouse_protocol = MouseProtocol::BtnEvent;
            break;
        case 1003:
            m_mouse_protocol = MouseProtocol::AnyEvent;
            break;
        case 1004:
            m_focus_event_mode = FocusEventMode::Enabled;
            break;
        case 1005:
            m_mouse_encoding = MouseEncoding::UTF8;
            break;
        case 1006:
            m_mouse_encoding = MouseEncoding::SGR;
            break;
        case 1015:
            m_mouse_encoding = MouseEncoding::URXVT;
            break;
        case 1016:
            m_mouse_encoding = MouseEncoding::SGRPixels;
            break;
        case 1049:
            set_use_alternate_screen_buffer(true);
            break;
        case 2004:
            m_bracketed_paste_mode = BracketedPasteMode::Enabled;
            break;
        case 2026:
            m_disable_drawing = true;
            break;
        default:
            break;
    }
}

// DEC Private Mode Reset - https://invisible-island.net/xterm/ctlseqs/ctlseqs.html
void Terminal::csi_decrst(Params const& params) {
    switch (params.get(0, 0)) {
        case 1:
            // Cursor Keys Mode - https://vt100.net/docs/vt510-rm/DECCKM.html
            m_application_cursor_keys_mode = ApplicationCursorKeysMode::Disabled;
            break;
        case 3:
            // Select 80 or 132 Columns per Page - https://vt100.net/docs/vt510-rm/DECCOLM.html
            if (m_allow_80_132_col_mode) {
                m_80_col_mode = true;
                m_132_col_mode = false;
                resize({ row_count(), 80, m_available_size.xpixels * 80 / m_available_size.cols, size().ypixels });
                clear();
                csi_decstbm({});
            }
            break;
        case 6:
            // Origin Mode - https://vt100.net/docs/vt510-rm/DECOM.html
            m_origin_mode = false;
            break;
        case 7:
            // Autowrap mode - https://vt100.net/docs/vt510-rm/DECAWM.html
            m_autowrap_mode = false;
            break;
        case 9:
            m_mouse_protocol = MouseProtocol::None;
            break;
        case 25:
            // Text Cursor Enable Mode - https://vt100.net/docs/vt510-rm/DECTCEM.html
            m_cursor_hidden = true;
            break;
        case 40:
            m_allow_80_132_col_mode = false;
            if (m_80_col_mode || m_132_col_mode) {
                m_80_col_mode = false;
                m_132_col_mode = false;
                resize(visible_size());
            }
            break;
        case 1000:
        case 1002:
        case 1003:
            m_mouse_protocol = MouseProtocol::None;
            break;
        case 1004:
            m_focus_event_mode = FocusEventMode::Disabled;
            break;
        case 1005:
        case 1006:
        case 1015:
        case 1016:
            m_mouse_encoding = MouseEncoding::X10;
            break;
        case 1049:
            set_use_alternate_screen_buffer(false);
            break;
        case 2004:
            m_bracketed_paste_mode = BracketedPasteMode::Disabled;
            break;
        case 2026:
            m_disable_drawing = false;
            break;
        default:
            break;
    }
}

// Request Mode - Host to Terminal - https://vt100.net/docs/vt510-rm/DECRQM.html
void Terminal::csi_decrqm(Params const& params) {
    auto param = params.get(0, 0);
    switch (param) {
        // Synchronized output - https://gist.github.com/christianparpart/d8a62cc1ab659194337d73e399004036
        case 2026:
            (void) m_psuedo_terminal.write_exactly(
                di::as_bytes(di::present("\033[?{};{}$y"_sv, param, m_disable_drawing ? 1 : 2)->span()));
            break;
        default:
            (void) m_psuedo_terminal.write_exactly(di::as_bytes(di::present("\033[?{};0$y"_sv, param)->span()));
    }
}

// Set Cursor Style - https://vt100.net/docs/vt510-rm/DECSCUSR.html
void Terminal::csi_decscusr(Params const& params) {
    auto param = params.get(0, 0);
    // 0 and 1 indicate the same style.
    if (param == 0) {
        param = 1;
    }
    if (param >= u32(CursorStyle::Max)) {
        return;
    }
    active_screen().cursor_style = CursorStyle(param);
}

// Select Graphics Rendition - https://vt100.net/docs/vt510-rm/SGR.html
void Terminal::csi_sgr(Params const& params) {
    // Delegate to graphics rendition class.
    auto rendition = active_screen().screen.current_graphics_rendition();
    rendition.update_with_csi_params(params);
    active_screen().screen.set_current_graphics_rendition(rendition);
}

// Device Status Report - https://vt100.net/docs/vt510-rm/DSR.html
void Terminal::csi_dsr(Params const& params) {
    switch (params.get(0, 0)) {
        case 5:
            // Operating Status - https://vt100.net/docs/vt510-rm/DSR-OS.html
            (void) m_psuedo_terminal.write_exactly(di::as_bytes("\033[0n"_sv.span()));
            break;
        case 6:
            // Cursor Position Report - https://vt100.net/docs/vt510-rm/DSR-CPR.html
            (void) m_psuedo_terminal.write_exactly(
                di::as_bytes(di::present("\033[{};{}R"_sv, cursor_row() + 1, cursor_col() + 1).value().span()));
            break;
        default:
            break;
    }
}

// DEC Set Top and Bottom Margins - https://www.vt100.net/docs/vt100-ug/chapter3.html#DECSTBM
void Terminal::csi_decstbm(Params const& params) {
    u32 new_scroll_start = di::min(params.get(0, 1) - 1, row_count() - 1);
    u32 new_scroll_end = di::min(params.get(1, row_count()) - 1, row_count() - 1);
    if (new_scroll_end - new_scroll_start < 1) {
        return;
    }
    // TODO: vertical scroll margins
    // m_scroll_start = new_scroll_start;
    // m_scroll_end = new_scroll_end;
    // set_cursor(0, 0);
}

// Save Current Cursor Position - https://vt100.net/docs/vt510-rm/SCOSC.html
void Terminal::csi_scosc(Params const&) {
    // Equivalent to DECSC (set cursor)
    esc_decsc();
}

// Restore Saved Cursor Position - https://vt100.net/docs/vt510-rm/SCORC.html
void Terminal::csi_scorc(Params const&) {
    // Equivalent to DECRC (restore cursor)
    esc_decrc();
}

// Window manipulation -
// https://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h4-Functions-using-CSI-_-ordered-by-the-final-character-lparen-s-rparen:CSI-Ps;Ps;Ps-t.1EB0
void Terminal::csi_xtwinops(Params const& params) {
    auto command = params.get(0);
    switch (command) {
        case 4: {
            if (!m_allow_force_terminal_size) {
                break;
            }

            // This could also set the width and height as based on the ratio of pixels to cells,
            // but we skip this for now. This command is used for testing ttx (forcing a specific
            // size), but does not change the visible size of the terminal itself, which is already
            // constrained by the layout.
            auto height = di::min(params.get(1, size().ypixels), 100000u);
            auto width = di::min(params.get(2, size().xpixels), 100000u);
            if (height == 0) {
                height = m_available_size.ypixels;
            }
            if (width == 0) {
                width = m_available_size.xpixels;
            }
            auto new_size = size();
            new_size.ypixels = height;
            new_size.xpixels = width;
            resize(new_size);
            break;
        }
        case 8: {
            if (!m_allow_force_terminal_size) {
                break;
            }

            // This logic is similar to DECSET 3 - 80/132 column mode, in that we don't actually resize the terminal's
            // visible area. This only resizes the terminal's internal size, which is useful for facilitating testing
            // or if the application requires the terminal to be a certain size.
            auto rows = di::min(params.get(1, row_count()), 1000u);
            auto cols = di::min(params.get(2, col_count()), 1000u);
            m_force_terminal_size = rows != 0 || cols != 0;
            if (rows == 0) {
                rows = m_available_size.rows;
            }
            if (cols == 0) {
                cols = m_available_size.cols;
            }
            auto new_size = size();
            new_size.rows = rows;
            new_size.cols = cols;
            resize(new_size);
            clear();
            csi_decstbm({});
            break;
        }
        default:
            break;
    }
}

// https://sw.kovidgoyal.net/kitty/keyboard-protocol/#progressive-enhancement
void Terminal::csi_set_key_reporting_flags(Params const& params) {
    auto flags_u32 = params.get(0);
    auto mode = params.get(1, 1);

    auto flags = KeyReportingFlags(flags_u32) & KeyReportingFlags::All;
    switch (mode) {
        case 1:
            m_key_reporting_flags = flags;
            break;
        case 2:
            m_key_reporting_flags |= flags;
            break;
        case 3:
            m_key_reporting_flags &= ~flags;
            break;
        default:
            break;
    }
}

// https://sw.kovidgoyal.net/kitty/keyboard-protocol/#progressive-enhancement
void Terminal::csi_get_key_reporting_flags(Params const&) {
    (void) m_psuedo_terminal.write_exactly(
        di::as_bytes(di::present("\033[?{}u"_sv, u32(m_key_reporting_flags)).value().span()));
}

// https://sw.kovidgoyal.net/kitty/keyboard-protocol/#progressive-enhancement
void Terminal::csi_push_key_reporting_flags(Params const& params) {
    auto flags_u32 = params.get(0);
    auto flags = KeyReportingFlags(flags_u32) & KeyReportingFlags::All;

    if (m_key_reporting_flags_stack.size() >= 100) {
        m_key_reporting_flags_stack.pop_front();
    }
    m_key_reporting_flags_stack.push_back(m_key_reporting_flags);
    m_key_reporting_flags = flags;
}

// https://sw.kovidgoyal.net/kitty/keyboard-protocol/#progressive-enhancement
void Terminal::csi_pop_key_reporting_flags(Params const& params) {
    auto n = params.get(0, 1);
    if (n == 0) {
        return;
    }
    if (n >= m_key_reporting_flags_stack.size()) {
        m_key_reporting_flags_stack.clear();
        m_key_reporting_flags = KeyReportingFlags::None;
        return;
    }

    auto new_stack_size = m_key_reporting_flags_stack.size() - n;
    m_key_reporting_flags = m_key_reporting_flags_stack[new_stack_size];
    m_key_reporting_flags_stack.erase(m_key_reporting_flags_stack.begin() + isize(new_stack_size));
}

void Terminal::set_visible_size(Size const& size) {
    if (m_available_size == size) {
        return;
    }

    m_available_size = size;
    if (!m_80_col_mode && !m_132_col_mode && !m_force_terminal_size) {
        resize(size);
    }
}

void Terminal::resize(Size const& size) {
    if (active_screen().screen.size() == size) {
        return;
    }
    active_screen().screen.resize(size);

    // Send size update to client.
    // TODO: support in-band resize notifications: https://gist.github.com/rockorager/e695fb2924d36b2bcf1fff4a3704bd83
    (void) m_psuedo_terminal.set_tty_window_size(size.as_window_size());
}

void Terminal::invalidate_all() {
    active_screen().screen.invalidate_all();
}

void Terminal::clear() {
    active_screen().screen.clear();
}

void Terminal::put_char(c32 c) {
    active_screen().screen.put_code_point(c);
}

void Terminal::set_use_alternate_screen_buffer(bool b) {
    if (in_alternate_screen_buffer() == b) {
        return;
    }

    if (b) {
        m_alternate_screen = di::make_box<ScreenState>(size(), terminal::Screen::ScrollBackEnabled::No);
    } else {
        ASSERT(m_alternate_screen);
        m_alternate_screen = {};
    }
    invalidate_all();
}

void Terminal::scroll_up() {
    // TODO: visual scrolling
}

void Terminal::scroll_down() {
    // TODO: visual scrolling
}

void Terminal::scroll_to_bottom() {
    // TODO: visual scrolling
}

auto Terminal::active_screen() const -> ScreenState const& {
    if (m_alternate_screen) {
        return *m_alternate_screen;
    }
    return m_primary_screen;
}

// auto Terminal::state_as_escape_sequences_internal(di::VectorWriter<>& writer) const {
// // 1. Terminal size. (note that the visibile size is not reported in any way).
// di::writer_print<di::String::Encoding>(writer, "\033[4;{};{}t"_sv, m_ypixels, m_xpixels);
// di::writer_print<di::String::Encoding>(writer, "\033[8;{};{}t"_sv, m_row_count, m_col_count);
// if (m_80_col_mode || m_132_col_mode) {
//     // When writing the mode, first ensure we enable setting the mode.
//     di::writer_print<di::String::Encoding>(writer, "\033[?40h"_sv);
//     auto _ = di::ScopeExit([&] {
//         di::writer_print<di::String::Encoding>(writer, "\033[?40l"_sv);
//     });
//
//     if (m_80_col_mode) {
//         di::writer_print<di::String::Encoding>(writer, "\033[?3l"_sv);
//     } else {
//         di::writer_print<di::String::Encoding>(writer, "\033[?3h"_sv);
//     }
// }
//
// // 2. Terminal cell contents.
// {
//     // When printing terminal cell contents, ensure auto-wrap is disabled, to prevent accidently scrolling the
//     // screen.
//     di::writer_print<di::String::Encoding>(writer, "\033[?7l"_sv);
//     auto _ = di::ScopeExit([&] {
//         di::writer_print<di::String::Encoding>(writer, "\033[?7h"_sv);
//     });
//
//     auto first = true;
//     auto last_sgr = GraphicsRendition {};
//     auto output_row = [&](Row const& row) {
//         // Move to the next line (for any row other than the first).
//         if (!first) {
//             di::writer_print<di::String::Encoding>(writer, "\r\n"_sv);
//         }
//         first = false;
//
//         for (auto const& cell : row) {
//             // Write graphics rendition if needed.
//             if (cell.graphics_rendition != last_sgr) {
//                 for (auto& params : cell.graphics_rendition.as_csi_params()) {
//                     di::writer_print<di::String::Encoding>(writer, "\033[{}m"_sv, params);
//                 }
//                 last_sgr = cell.graphics_rendition;
//             }
//
//             // Write cell text.
//             di::writer_print<di::String::Encoding>(writer, "{}"_sv, cell.ch);
//         }
//     };
//
//     // Write out all cell contents.
//     auto all_rows = di::concat(m_rows_above, m_rows, di::reverse(m_rows_below));
//     di::container::for_each(all_rows, output_row);
//
//     // Pan up so that the active region is correct.
//     if (!m_rows_below.empty()) {
//         di::writer_print<di::String::Encoding>(writer, "\033[H\033[{}T"_sv, m_rows_below.size());
//     }
// }
//
// // 3. Tab stops (this is done before setting the cursor position, as it requires moving the cursor)
// for (auto col : m_tab_stops) {
//     di::writer_print<di::String::Encoding>(writer, "\033[0;{}H\033H"_sv, col + 1);
// }
//
// // 4. Internal state.
// {
//     // NOTE: Disable drawing (DECSET 2026) is ignored as saving its state is not useful.
//
//     // Scroll margin.
//     di::writer_print<di::String::Encoding>(writer, "\033[{};{}r"_sv, m_scroll_start + 1, m_scroll_end + 1);
//
//     // Auto wrap.
//     if (m_autowrap_mode) {
//         di::writer_print<di::String::Encoding>(writer, "\033[?7h"_sv);
//     }
//
//     // Origin mode.
//     if (m_origin_mode) {
//         di::writer_print<di::String::Encoding>(writer, "\033[?6h"_sv);
//     }
// }
//
// // 5. Application state
// {
//     // Cursor keys mode
//     if (m_application_cursor_keys_mode == ApplicationCursorKeysMode::Enabled) {
//         di::writer_print<di::String::Encoding>(writer, "\033[?1h"_sv);
//     }
//
//     // Kitty key flags
//     auto first = true;
//     auto set_kitty_key_flags = [&](KeyReportingFlags flags) {
//         if (first) {
//             di::writer_print<di::String::Encoding>(writer, "\033[=1;{}u"_sv, i32(flags));
//             first = false;
//         } else {
//             di::writer_print<di::String::Encoding>(writer, "\033[>{}u"_sv, i32(flags));
//         }
//     };
//
//     for (auto flags : m_key_reporting_flags_stack) {
//         set_kitty_key_flags(flags);
//     }
//     set_kitty_key_flags(m_key_reporting_flags);
//
//     // Alternate scroll mode
//     if (m_alternate_scroll_mode == AlternateScrollMode::Enabled) {
//         di::writer_print<di::String::Encoding>(writer, "\033[?1007h"_sv);
//     }
//
//     // Mouse protocol
//     switch (m_mouse_protocol) {
//         case MouseProtocol::None:
//             break;
//         case MouseProtocol::X10:
//             di::writer_print<di::String::Encoding>(writer, "\033[?9h"_sv);
//             break;
//         case MouseProtocol::VT200:
//             di::writer_print<di::String::Encoding>(writer, "\033[?1000h"_sv);
//             break;
//         case MouseProtocol::BtnEvent:
//             di::writer_print<di::String::Encoding>(writer, "\033[?1002h"_sv);
//             break;
//         case MouseProtocol::AnyEvent:
//             di::writer_print<di::String::Encoding>(writer, "\033[?1003h"_sv);
//             break;
//     }
//
//     // Mouse encoding
//     switch (m_mouse_encoding) {
//         case MouseEncoding::X10:
//             break;
//         case MouseEncoding::UTF8:
//             di::writer_print<di::String::Encoding>(writer, "\033[?1005h"_sv);
//             break;
//         case MouseEncoding::SGR:
//             di::writer_print<di::String::Encoding>(writer, "\033[?1006h"_sv);
//             break;
//         case MouseEncoding::URXVT:
//             di::writer_print<di::String::Encoding>(writer, "\033[?1015h"_sv);
//             break;
//         case MouseEncoding::SGRPixels:
//             di::writer_print<di::String::Encoding>(writer, "\033[?1016h"_sv);
//             break;
//     }
//
//     // Focus event mode
//     if (m_focus_event_mode == FocusEventMode::Enabled) {
//         di::writer_print<di::String::Encoding>(writer, "\033[?1004h"_sv);
//     }
//
//     // Bracketed paste
//     if (m_bracketed_paste_mode == BracketedPasteMode::Enabled) {
//         di::writer_print<di::String::Encoding>(writer, "\033[?2004h"_sv);
//     }
// }
//
// // 6. Cursor
// {
//     // Cursor style
//     di::writer_print<di::String::Encoding>(writer, "\033[{} q"_sv, i32(m_cursor_style));
//
//     // Cursor position - when in origin mode we need to adjust our coordinates based on the scroll region's
//     // start.
//     if (m_origin_mode) {
//         di::writer_print<di::String::Encoding>(writer, "\033[{};{}H"_sv, m_cursor_row - m_scroll_start + 1,
//                                                m_cursor_col - m_scroll_start + 1);
//     } else {
//         di::writer_print<di::String::Encoding>(writer, "\033[{};{}H"_sv, m_cursor_row + 1, m_cursor_col + 1);
//     }
//
//     // Cursor visible
//     if (m_cursor_hidden) {
//         di::writer_print<di::String::Encoding>(writer, "\033[?25l"_sv);
//     }
// }
//
// // 7. X-overflow
// {
//     // If we're pending overflow, we need to emit the last visibile cell again.
//     if (m_x_overflow) {
//         auto const& cell = *di::back(m_rows[m_cursor_row]);
//         for (auto& params : cell.graphics_rendition.as_csi_params()) {
//             di::writer_print<di::String::Encoding>(writer, "\033[{}m"_sv, params);
//         }
//         di::writer_print<di::String::Encoding>(writer, "{}"_sv, cell.ch);
//     }
// }
//
// // 8. Current sgr
// for (auto& params : m_current_graphics_rendition.as_csi_params()) {
//     di::writer_print<di::String::Encoding>(writer, "\033[{}m"_sv, params);
// }
// }

auto Terminal::state_as_escape_sequences() const -> di::String {
    auto writer = di::VectorWriter<> {};

    // 1. Reset terminal
    di::writer_print<di::String::Encoding>(writer, "\033c"_sv);

    // if (m_save_state) {
    //     // 2. If in alternate screen buffer, write the main buffer first.
    //     m_save_state->state_as_escape_sequences_internal(writer);
    //
    //     // 3. Enter alternate screen buffer, if necssary.
    //     di::writer_print<di::String::Encoding>(writer, "\033[?1049h"_sv);
    // }
    //
    // // 4. Write current contents.
    // state_as_escape_sequences_internal(writer);

    // Return the resulting string.
    return writer.vector() | di::transform(di::construct<c8>) | di::to<di::String>(di::encoding::assume_valid);
}
}
