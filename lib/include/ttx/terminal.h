#pragma once

#include "di/container/ring/ring.h"
#include "di/io/vector_writer.h"
#include "dius/sync_file.h"
#include "ttx/cursor_style.h"
#include "ttx/escape_sequence_parser.h"
#include "ttx/focus_event_io.h"
#include "ttx/key_event_io.h"
#include "ttx/mouse_event_io.h"
#include "ttx/params.h"
#include "ttx/paste_event_io.h"
#include "ttx/size.h"
#include "ttx/terminal/screen.h"

namespace ttx {
struct SetClipboard {
    di::Vector<byte> data;
};

using TerminalEvent = di::Variant<SetClipboard>;

class Terminal {
    struct ScreenState {
        explicit ScreenState(Size const& size, terminal::Screen::ScrollBackEnabled scroll_back_enabled)
            : screen(size, scroll_back_enabled) {}

        terminal::Screen screen;
        di::Optional<terminal::SavedCursor> saved_cursor;
        CursorStyle cursor_style { CursorStyle::SteadyBar };
    };

public:
    explicit Terminal(dius::SyncFile& psuedo_terminal, Size const& size);

    // Return a string which when replayed will result in the terminal
    // having state identical to the current state.
    auto state_as_escape_sequences() const -> di::String;

    void on_parser_results(di::Span<ParserResult const> results);

    auto active_screen() const -> ScreenState const&;
    auto active_screen() -> ScreenState& {
        return const_cast<ScreenState&>(const_cast<Terminal const&>(*this).active_screen());
    }

    auto cursor_row() const -> u32 { return active_screen().screen.cursor().row; }
    auto cursor_col() const -> u32 { return active_screen().screen.cursor().col; }
    auto cursor_hidden() const -> bool { return m_cursor_hidden; }
    auto cursor_style() const -> CursorStyle { return active_screen().cursor_style; }
    auto reverse_video() const -> bool { return m_reverse_video; }

    auto allowed_to_draw() const -> bool { return !m_disable_drawing; }

    void scroll_to_bottom();
    void scroll_up();
    void scroll_down();

    // TODO: scroll back
    auto total_rows() const -> u32 { return row_count(); }
    auto row_offset() const -> u32 { return 0; }

    auto row_count() const -> u32 { return active_screen().screen.max_height(); }
    auto col_count() const -> u32 { return active_screen().screen.max_width(); }
    auto size() const -> Size { return active_screen().screen.size(); }

    void set_visible_size(Size const& size);
    auto visible_size() const -> Size { return m_available_size; }

    auto application_cursor_keys_mode() const -> ApplicationCursorKeysMode { return m_application_cursor_keys_mode; }
    auto key_reporting_flags() const -> KeyReportingFlags { return m_key_reporting_flags; }

    auto alternate_scroll_mode() const -> AlternateScrollMode { return m_alternate_scroll_mode; }
    auto mouse_protocol() const -> MouseProtocol { return m_mouse_protocol; }
    auto mouse_encoding() const -> MouseEncoding { return m_mouse_encoding; }
    auto in_alternate_screen_buffer() const -> bool { return !!m_alternate_screen; }
    auto focus_event_mode() const -> FocusEventMode { return m_focus_event_mode; }

    void reset_mouse_reporting() { m_mouse_protocol = MouseProtocol::None; }

    auto bracked_paste_mode() const -> BracketedPasteMode { return m_bracketed_paste_mode; }

    void invalidate_all();

    auto outgoing_events() -> di::Vector<TerminalEvent> { return di::move(m_outgoing_events); }

    void set_allow_force_terminal_size(bool b = true) { m_allow_force_terminal_size = b; }

private:
    void on_parser_result(PrintableCharacter const& printable_character);
    void on_parser_result(DCS const& dcs);
    void on_parser_result(OSC const& osc);
    void on_parser_result(APC const& apc);
    void on_parser_result(CSI const& csi);
    void on_parser_result(Escape const& escape);
    void on_parser_result(ControlCharacter const& control);

    void resize(Size const& size);

    void put_char(c32 c);

    // TODO: vertical and horizontal scrolling regions.
    auto min_row_inclusive() const -> u32 { return 0; }
    auto min_col_inclusive() const -> u32 { return 0; }

    auto max_row_inclusive() const -> u32 { return row_count() - 1; }
    auto max_col_inclusive() const -> u32 { return col_count() - 1; }

    void clear();

    void set_use_alternate_screen_buffer(bool b);

    void esc_decaln();
    void esc_decsc();
    void esc_decrc();

    void c0_bs();
    void c0_ht();
    void c0_lf();
    void c0_vt();
    void c0_ff();
    void c0_cr();

    void c1_ind();
    void c1_nel();
    void c1_hts();
    void c1_ri();

    void dcs_decrqss(Params const& params, di::StringView data);

    void osc_52(di::StringView data);

    void csi_ich(Params const& params);
    void csi_cuu(Params const& params);
    void csi_cud(Params const& params);
    void csi_cuf(Params const& params);
    void csi_cub(Params const& params);
    void csi_cup(Params const& params);
    void csi_cha(Params const& params);
    void csi_ed(Params const& params);
    void csi_el(Params const& params);
    void csi_il(Params const& params);
    void csi_dl(Params const& params);
    void csi_dch(Params const& params);
    void csi_su(Params const& params);
    void csi_sd(Params const& params);
    void csi_ech(Params const& params);
    void csi_rep(Params const& params);
    void csi_da1(Params const& params);
    void csi_da2(Params const& params);
    void csi_da3(Params const& params);
    void csi_vpa(Params const& params);
    void csi_hvp(Params const& params);
    void csi_tbc(Params const& params);
    void csi_decset(Params const& params);
    void csi_decrst(Params const& params);
    void csi_decrqm(Params const& params);
    void csi_decscusr(Params const& params);
    void csi_sgr(Params const& params);
    void csi_dsr(Params const& params);
    void csi_decstbm(Params const& params);
    void csi_scosc(Params const& params);
    void csi_scorc(Params const& params);
    void csi_xtwinops(Params const& params);

    void csi_set_key_reporting_flags(Params const& params);
    void csi_get_key_reporting_flags(Params const& params);
    void csi_push_key_reporting_flags(Params const& params);
    void csi_pop_key_reporting_flags(Params const& params);

    ScreenState m_primary_screen;
    di::Box<ScreenState> m_alternate_screen;

    Size m_available_size;
    bool m_80_col_mode { false };
    bool m_132_col_mode { false };
    bool m_allow_80_132_col_mode { false };
    bool m_force_terminal_size { false };
    bool m_allow_force_terminal_size { false };

    di::Vector<u32> m_tab_stops;
    bool m_cursor_hidden { false };
    bool m_disable_drawing { false };
    terminal::AutoWrapMode m_auto_wrap_mode { terminal::AutoWrapMode::Enabled };
    bool m_reverse_video { false };

    ApplicationCursorKeysMode m_application_cursor_keys_mode { ApplicationCursorKeysMode::Disabled };
    KeyReportingFlags m_key_reporting_flags { KeyReportingFlags::None };
    di::Ring<KeyReportingFlags> m_key_reporting_flags_stack;

    AlternateScrollMode m_alternate_scroll_mode { AlternateScrollMode::Disabled };
    MouseProtocol m_mouse_protocol { MouseProtocol::None };
    MouseEncoding m_mouse_encoding { MouseEncoding::X10 };
    FocusEventMode m_focus_event_mode { FocusEventMode::Disabled };

    BracketedPasteMode m_bracketed_paste_mode { false };

    di::Vector<TerminalEvent> m_outgoing_events;

    dius::SyncFile& m_psuedo_terminal;
};
}
