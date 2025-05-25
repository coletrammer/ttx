#pragma once

#include "di/container/string/string_view.h"
#include "di/container/vector/vector.h"
#include "di/vocab/variant/variant.h"
#include "ttx/escape_sequence_parser.h"
#include "ttx/features.h"
#include "ttx/focus_event.h"
#include "ttx/key_event.h"
#include "ttx/mouse_event.h"
#include "ttx/paste_event.h"
#include "ttx/terminal/escapes/device_attributes.h"
#include "ttx/terminal/escapes/device_status.h"
#include "ttx/terminal/escapes/mode.h"
#include "ttx/terminal/escapes/osc_52.h"
#include "ttx/terminal/escapes/terminfo_string.h"

namespace ttx {
using Event = di::Variant<KeyEvent, MouseEvent, FocusEvent, PasteEvent, terminal::PrimaryDeviceAttributes,
                          terminal::ModeQueryReply, terminal::CursorPositionReport, terminal::KittyKeyReport,
                          terminal::StatusStringResponse, terminal::TerminfoString, terminal::OSC52>;

class TerminalInputParser {
public:
    auto parse(di::StringView input, Feature features) -> di::Vector<Event>;

private:
    void handle(PrintableCharacter const& printable_character);
    void handle(DCS const& dcs);
    void handle(OSC const& osc);
    void handle(APC const& apc);
    void handle(CSI const& csi);
    void handle(Escape const& escape);
    void handle(ControlCharacter const& control_character);

    EscapeSequenceParser m_parser;
    di::Vector<Event> m_events;
    bool m_in_bracketed_paste { false };
    di::String m_paste_buffer;
};
}
