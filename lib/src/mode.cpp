#include "ttx/terminal/escapes/mode.h"

#include "di/container/algorithm/binary_search.h"
#include "di/container/algorithm/sort.h"
#include "ttx/focus_event_io.h"
#include "ttx/key_event_io.h"
#include "ttx/mouse_event_io.h"
#include "ttx/params.h"
#include "ttx/paste_event_io.h"
#include "ttx/terminal.h"
#include "ttx/terminal/screen.h"

namespace ttx {
using namespace terminal;

// None - sentinel for when a mode is not supported.
template<>
consteval auto make_mode_handler<DecMode::None>() -> ModeHandler {
    return {
        .query_mode = [](Terminal&) -> ModeSupport {
            return ModeSupport::Unknown;
        },
        .set_mode = [](Terminal&, bool) {},
    };
}

// Cursor Keys Mode - https://vt100.net/docs/vt510-rm/DECCKM.html
template<>
consteval auto make_mode_handler<DecMode::CursorKeysMode>() -> ModeHandler {
    return {
        .query_mode = [](Terminal& terminal) -> ModeSupport {
            return terminal.m_application_cursor_keys_mode == ApplicationCursorKeysMode::Enabled ? ModeSupport::Set
                                                                                                 : ModeSupport::Unset;
        },
        .set_mode =
            [](Terminal& terminal, bool is_set) {
                terminal.m_application_cursor_keys_mode =
                    is_set ? ApplicationCursorKeysMode::Enabled : ApplicationCursorKeysMode::Disabled;
            },
    };
}

// Select 80 or 132 Columns per Page - https://vt100.net/docs/vt510-rm/DECCOLM.html
template<>
consteval auto make_mode_handler<DecMode::Select80Or132ColumnMode>() -> ModeHandler {
    return {
        .query_mode = [](Terminal& terminal) -> ModeSupport {
            return terminal.m_132_col_mode ? ModeSupport::Set : ModeSupport::Unset;
        },
        .set_mode =
            [](Terminal& terminal, bool is_set) {
                if (!terminal.m_allow_80_132_col_mode) {
                    return;
                }

                if (is_set) {
                    // 132 columns
                    terminal.m_80_col_mode = false;
                    terminal.m_132_col_mode = true;
                    terminal.active_screen().screen.clear_scroll_back();
                    terminal.clear();
                    terminal.resize({ 24, 132, terminal.size().xpixels * 132 / terminal.size().cols,
                                      terminal.size().ypixels * 24 / terminal.size().rows });
                    terminal.csi_decstbm({});
                } else {
                    // 80 columns
                    terminal.m_80_col_mode = true;
                    terminal.m_132_col_mode = false;
                    terminal.active_screen().screen.clear_scroll_back();
                    terminal.resize({ 24, 80, terminal.size().xpixels * 80 / terminal.size().cols,
                                      terminal.size().ypixels * 24 / terminal.size().rows });
                    terminal.clear();
                    terminal.csi_decstbm({});
                }
            },
    };
}

// Reverse video - https://vt100.net/docs/vt510-rm/DECSCNM.html
template<>
consteval auto make_mode_handler<DecMode::ReverseVideo>() -> ModeHandler {
    return {
        .query_mode = [](Terminal& terminal) -> ModeSupport {
            return terminal.m_reverse_video ? ModeSupport::Set : ModeSupport::Unset;
        },
        .set_mode =
            [](Terminal& terminal, bool is_set) {
                terminal.m_reverse_video = is_set;
                terminal.invalidate_all();
            },
    };
}

// Origin Mode - https://vt100.net/docs/vt510-rm/DECOM.html
// Unlike other modes, this one is per-screen, as this mode
// is technically part of the cursor state.
template<>
consteval auto make_mode_handler<DecMode::OriginMode>() -> ModeHandler {
    return {
        .query_mode = [](Terminal& terminal) -> ModeSupport {
            return terminal.active_screen().screen.origin_mode() == OriginMode::Enabled ? ModeSupport::Set
                                                                                        : ModeSupport::Unset;
        },
        .set_mode =
            [](Terminal& terminal, bool is_set) {
                terminal.active_screen().screen.set_origin_mode(is_set ? OriginMode::Enabled : OriginMode::Disabled);
            },
    };
}

// Autowrap mode - https://vt100.net/docs/vt510-rm/DECAWM.html
template<>
consteval auto make_mode_handler<DecMode::AutoWrap>() -> ModeHandler {
    return {
        .query_mode = [](Terminal& terminal) -> ModeSupport {
            return terminal.m_auto_wrap_mode == AutoWrapMode::Enabled ? ModeSupport::Set : ModeSupport::Unset;
        },
        .set_mode =
            [](Terminal& terminal, bool is_set) {
                terminal.m_auto_wrap_mode = is_set ? AutoWrapMode::Enabled : AutoWrapMode ::Disabled;
            },
    };
}

template<>
consteval auto make_mode_handler<DecMode::X10Mouse>() -> ModeHandler {
    return {
        .query_mode = [](Terminal& terminal) -> ModeSupport {
            return terminal.m_mouse_protocol == MouseProtocol::X10 ? ModeSupport::Set : ModeSupport::Unset;
        },
        .set_mode =
            [](Terminal& terminal, bool is_set) {
                // Mouse encoding is weird because there is no "None" value, and the
                // default is X10. And this legacy mode controls both the protocol and
                // encoding.
                if (is_set) {
                    terminal.m_mouse_protocol = MouseProtocol::X10;
                    terminal.m_mouse_encoding = MouseEncoding::X10;
                } else {
                    terminal.m_mouse_protocol = MouseProtocol::None;
                    terminal.m_mouse_encoding = MouseEncoding::X10;
                }
            },
    };
}

// Text Cursor Enable Mode - https://vt100.net/docs/vt510-rm/DECTCEM.html
template<>
consteval auto make_mode_handler<DecMode::CursorEnable>() -> ModeHandler {
    return {
        .query_mode = [](Terminal& terminal) -> ModeSupport {
            return terminal.m_cursor_hidden ? ModeSupport::Set : ModeSupport::Unset;
        },
        .set_mode =
            [](Terminal& terminal, bool is_set) {
                terminal.m_cursor_hidden = !is_set;
            },
    };
}

template<>
consteval auto make_mode_handler<DecMode::Allow80Or132ColumnMode>() -> ModeHandler {
    return {
        .query_mode = [](Terminal& terminal) -> ModeSupport {
            return terminal.m_allow_80_132_col_mode ? ModeSupport::Set : ModeSupport::Unset;
        },
        .set_mode =
            [](Terminal& terminal, bool is_set) {
                if (is_set) {
                    terminal.m_allow_80_132_col_mode = true;
                } else {
                    terminal.m_allow_80_132_col_mode = false;
                    if (terminal.m_80_col_mode || terminal.m_132_col_mode) {
                        terminal.m_80_col_mode = false;
                        terminal.m_132_col_mode = false;
                        terminal.resize(terminal.visible_size());
                    }
                }
            },
    };
}

template<>
auto make_mode_handler<DecMode::HorizontalMargins>() -> ModeHandler = delete;

template<>
consteval auto make_mode_handler<DecMode::VT200Mouse>() -> ModeHandler {
    return {
        .query_mode = [](Terminal& terminal) -> ModeSupport {
            return terminal.m_mouse_protocol == MouseProtocol::VT200 ? ModeSupport::Set : ModeSupport::Unset;
        },
        .set_mode =
            [](Terminal& terminal, bool is_set) {
                if (is_set) {
                    terminal.m_mouse_protocol = MouseProtocol::VT200;
                } else {
                    terminal.m_mouse_protocol = MouseProtocol::None;
                }
            },
    };
}

template<>
consteval auto make_mode_handler<DecMode::CellMotionMouseTracking>() -> ModeHandler {
    return {
        .query_mode = [](Terminal& terminal) -> ModeSupport {
            return terminal.m_mouse_protocol == MouseProtocol::BtnEvent ? ModeSupport::Set : ModeSupport::Unset;
        },
        .set_mode =
            [](Terminal& terminal, bool is_set) {
                if (is_set) {
                    terminal.m_mouse_protocol = MouseProtocol::BtnEvent;
                } else {
                    terminal.m_mouse_protocol = MouseProtocol::None;
                }
            },
    };
}

template<>
consteval auto make_mode_handler<DecMode::AllMotionMouseTracking>() -> ModeHandler {
    return {
        .query_mode = [](Terminal& terminal) -> ModeSupport {
            return terminal.m_mouse_protocol == MouseProtocol::AnyEvent ? ModeSupport::Set : ModeSupport::Unset;
        },
        .set_mode =
            [](Terminal& terminal, bool is_set) {
                if (is_set) {
                    terminal.m_mouse_protocol = MouseProtocol::AnyEvent;
                } else {
                    terminal.m_mouse_protocol = MouseProtocol::None;
                }
            },
    };
}

template<>
consteval auto make_mode_handler<DecMode::FocusEvent>() -> ModeHandler {
    return {
        .query_mode = [](Terminal& terminal) -> ModeSupport {
            return terminal.m_focus_event_mode == FocusEventMode::Enabled ? ModeSupport::Set : ModeSupport::Unset;
        },
        .set_mode =
            [](Terminal& terminal, bool is_set) {
                if (is_set) {
                    terminal.m_focus_event_mode = FocusEventMode::Enabled;
                } else {
                    terminal.m_focus_event_mode = FocusEventMode::Disabled;
                }
            },
    };
}

template<>
consteval auto make_mode_handler<DecMode::UTF8Mouse>() -> ModeHandler {
    return {
        .query_mode = [](Terminal& terminal) -> ModeSupport {
            return terminal.m_mouse_encoding == MouseEncoding::UTF8 ? ModeSupport::Set : ModeSupport::Unset;
        },
        .set_mode =
            [](Terminal& terminal, bool is_set) {
                if (is_set) {
                    terminal.m_mouse_encoding = MouseEncoding::UTF8;
                } else {
                    terminal.m_mouse_encoding = MouseEncoding::X10;
                }
            },
    };
}

template<>
consteval auto make_mode_handler<DecMode::SGRMouse>() -> ModeHandler {
    return {
        .query_mode = [](Terminal& terminal) -> ModeSupport {
            return terminal.m_mouse_encoding == MouseEncoding::SGR ? ModeSupport::Set : ModeSupport::Unset;
        },
        .set_mode =
            [](Terminal& terminal, bool is_set) {
                if (is_set) {
                    terminal.m_mouse_encoding = MouseEncoding::SGR;
                } else {
                    terminal.m_mouse_encoding = MouseEncoding::X10;
                }
            },
    };
}

template<>
consteval auto make_mode_handler<DecMode::AlternateScroll>() -> ModeHandler {
    return {
        .query_mode = [](Terminal& terminal) -> ModeSupport {
            return terminal.m_alternate_scroll_mode == AlternateScrollMode::Enabled ? ModeSupport::Set
                                                                                    : ModeSupport::Unset;
        },
        .set_mode =
            [](Terminal& terminal, bool is_set) {
                if (is_set) {
                    terminal.m_alternate_scroll_mode = AlternateScrollMode::Enabled;
                } else {
                    terminal.m_alternate_scroll_mode = AlternateScrollMode::Disabled;
                }
            },
    };
}

template<>
consteval auto make_mode_handler<DecMode::URXVTMouse>() -> ModeHandler {
    return {
        .query_mode = [](Terminal& terminal) -> ModeSupport {
            return terminal.m_mouse_encoding == MouseEncoding::URXVT ? ModeSupport::Set : ModeSupport::Unset;
        },
        .set_mode =
            [](Terminal& terminal, bool is_set) {
                if (is_set) {
                    terminal.m_mouse_encoding = MouseEncoding::URXVT;
                } else {
                    terminal.m_mouse_encoding = MouseEncoding::X10;
                }
            },
    };
}

template<>
consteval auto make_mode_handler<DecMode::SGRPixelMouse>() -> ModeHandler {
    return {
        .query_mode = [](Terminal& terminal) -> ModeSupport {
            return terminal.m_mouse_encoding == MouseEncoding::SGRPixels ? ModeSupport::Set : ModeSupport::Unset;
        },
        .set_mode =
            [](Terminal& terminal, bool is_set) {
                if (is_set) {
                    terminal.m_mouse_encoding = MouseEncoding::SGRPixels;
                } else {
                    terminal.m_mouse_encoding = MouseEncoding::X10;
                }
            },
    };
}

template<>
consteval auto make_mode_handler<DecMode::AlternateScreenBuffer>() -> ModeHandler {
    return {
        .query_mode = [](Terminal& terminal) -> ModeSupport {
            return terminal.m_alternate_screen != nullptr ? ModeSupport::Set : ModeSupport::Unset;
        },
        .set_mode =
            [](Terminal& terminal, bool is_set) {
                terminal.set_use_alternate_screen_buffer(is_set);
            },
    };
}

template<>
consteval auto make_mode_handler<DecMode::BracketedPaste>() -> ModeHandler {
    return {
        .query_mode = [](Terminal& terminal) -> ModeSupport {
            return terminal.m_bracketed_paste_mode == BracketedPasteMode::Enabled ? ModeSupport::Set
                                                                                  : ModeSupport::Unset;
        },
        .set_mode =
            [](Terminal& terminal, bool is_set) {
                if (is_set) {
                    terminal.m_bracketed_paste_mode = BracketedPasteMode::Enabled;
                } else {
                    terminal.m_bracketed_paste_mode = BracketedPasteMode::Disabled;
                }
            },
    };
}

template<>
consteval auto make_mode_handler<DecMode::SynchronizedOutput>() -> ModeHandler {
    return {
        .query_mode = [](Terminal& terminal) -> ModeSupport {
            return terminal.m_disable_drawing ? ModeSupport::Set : ModeSupport::Unset;
        },
        .set_mode =
            [](Terminal& terminal, bool is_set) {
                terminal.m_disable_drawing = is_set;
            },
    };
}

template<>
consteval auto make_mode_handler<DecMode::GraphemeClustering>() -> ModeHandler {
    return {
        .query_mode = [](Terminal&) -> ModeSupport {
            return ModeSupport::AlwaysSet;
        },
        .set_mode = [](Terminal&, bool) {},
    };
}

template<>
auto make_mode_handler<DecMode::ThemeDetection>() -> ModeHandler = delete;

template<>
auto make_mode_handler<DecMode::InBandSizeReports>() -> ModeHandler = delete;

constexpr static auto all_modes_dynamic() -> di::Vector<ModeHandler> {
    constexpr auto possible_dec_modes = di::reflect(di::in_place_type<DecMode>);

    auto result = di::Vector<ModeHandler> {};
    di::tuple_for_each(
        [&](auto enumerator) {
            if constexpr (requires { make_mode_handler<enumerator.value>(); }) {
                auto handler = make_mode_handler<enumerator.value>();
                handler.mode = u32(enumerator.value);
                handler.is_private = true;
                result.push_back(handler);
            }
        },
        possible_dec_modes);
    return result;
}

consteval auto all_modes_static() {
    auto modes = all_modes_dynamic();

    auto result = di::Array<ModeHandler, all_modes_dynamic().size()> {};
    di::copy(modes, result.begin());
    di::sort(modes, di::compare, &ModeHandler::mode);

    return result;
}

constexpr auto all_modes = all_modes_static();

constexpr static auto lookup_mode(u32 mode) -> ModeHandler {
    auto result = di::binary_search(all_modes, mode, di::compare, &ModeHandler::mode);
    if (!result.found) {
        return all_modes[0];
    }
    return *result.in;
}

// DEC Private Mode Set - https://invisible-island.net/xterm/ctlseqs/ctlseqs.html
void Terminal::csi_decset(Params const& params) {
    if (params.size() != 1) {
        return;
    }
    auto mode = params.get(0);
    lookup_mode(mode).set_mode(*this, true);
}

// DEC Private Mode Reset - https://invisible-island.net/xterm/ctlseqs/ctlseqs.html
void Terminal::csi_decrst(Params const& params) {
    if (params.size() != 1) {
        return;
    }
    auto mode = params.get(0);
    lookup_mode(mode).set_mode(*this, false);
}

// Request Mode - Host to Terminal - https://vt100.net/docs/vt510-rm/DECRQM.html
void Terminal::csi_decrqm(Params const& params) {
    if (params.size() != 1) {
        return;
    }
    auto mode = params.get(0);
    auto reply = terminal::ModeQueryReply {
        .support = lookup_mode(mode).query_mode(*this),
        .dec_mode = terminal::DecMode(mode),
    };

    auto reply_string = reply.serialize();
    (void) m_psuedo_terminal.write_exactly(di::as_bytes(reply_string.span()));
}
}
