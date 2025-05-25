#include "ttx/terminal_input.h"

#include "di/container/algorithm/for_each.h"
#include "di/container/iterator/distance.h"
#include "di/container/view/slide.h"
#include "ttx/features.h"
#include "ttx/focus_event_io.h"
#include "ttx/key_event.h"
#include "ttx/key_event_io.h"
#include "ttx/modifiers.h"
#include "ttx/mouse_event_io.h"
#include "ttx/paste_event.h"
#include "ttx/paste_event_io.h"
#include "ttx/terminal/escapes/device_attributes.h"
#include "ttx/terminal/escapes/device_status.h"
#include "ttx/terminal/escapes/mode.h"
#include "ttx/terminal/escapes/osc_52.h"
#include "ttx/terminal/escapes/terminfo_string.h"

namespace ttx {
auto TerminalInputParser::parse(di::StringView input, Feature features) -> di::Vector<Event> {
    // Go 1 character at a time, to ensure we can react to bracketed paste mode.
    for (auto ch : input | di::slide(1)) {
        if (m_in_bracketed_paste) {
            m_paste_buffer.append(ch);

            if (m_paste_buffer.ends_with(bracketed_paste_end)) {
                m_paste_buffer.erase(di::prev(m_paste_buffer.end(), di::distance(bracketed_paste_end)),
                                     m_paste_buffer.end());
                m_events.push_back(PasteEvent(di::move(m_paste_buffer)));
                m_in_bracketed_paste = false;
            }
            continue;
        }

        auto flush = ch.end() == input.end();
        auto event = m_parser.parse_input_escape_sequences(ch, features, flush);
        di::for_each(event, [&](auto const& ev) {
            di::visit(
                [&](auto const& e) {
                    this->handle(e);
                },
                ev);
        });
    }
    return di::move(m_events);
}

void TerminalInputParser::handle(PrintableCharacter const& printable_character) {
    m_events.emplace_back(key_event_from_legacy_code_point(printable_character.code_point));
}

void TerminalInputParser::handle(DCS const& dcs) {
    if (auto status_string_response = terminal::StatusStringResponse::from_dcs(dcs)) {
        m_events.emplace_back(di::move(status_string_response).value());
    }
    if (auto terminfo_string = terminal::TerminfoString::from_dcs(dcs)) {
        m_events.emplace_back(di::move(terminfo_string).value());
    }
}

void TerminalInputParser::handle(OSC const& osc) {
    if (auto osc52 = terminal::OSC52::parse(osc.data)) {
        m_events.emplace_back(di::move(osc52).value());
    }
}

void TerminalInputParser::handle(APC const&) {}

void TerminalInputParser::handle(CSI const& csi) {
    if (auto key_event = key_event_from_csi(csi)) {
        m_events.emplace_back(di::move(key_event).value());
    }
    if (auto mouse_event = mouse_event_from_csi(csi)) {
        m_events.emplace_back(di::move(mouse_event).value());
    }
    if (auto focus_event = focus_event_from_csi(csi)) {
        m_events.emplace_back(di::move(focus_event).value());
    }
    if (auto primary_device_attributes = terminal::PrimaryDeviceAttributes::from_csi(csi)) {
        m_events.emplace_back(di::move(primary_device_attributes).value());
    }
    if (auto mode_reply = terminal::ModeQueryReply::from_csi(csi)) {
        m_events.emplace_back(di::move(mode_reply).value());
    }
    if (auto cursor_position_report = terminal::CursorPositionReport::from_csi(csi)) {
        m_events.emplace_back(di::move(cursor_position_report).value());
    }
    if (auto kitty_key_report = terminal::KittyKeyReport::from_csi(csi)) {
        m_events.emplace_back(di::move(kitty_key_report).value());
    }
    if (is_bracketed_paste_begin(csi)) {
        m_in_bracketed_paste = true;
    }
}

void TerminalInputParser::handle(Escape const&) {}

void TerminalInputParser::handle(ControlCharacter const& control_character) {
    auto modifiers = control_character.was_in_escape ? Modifiers::Alt : Modifiers::None;
    m_events.emplace_back(key_event_from_legacy_code_point(control_character.code_point, modifiers));
}
}
