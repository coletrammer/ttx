#include "di/io/vector_writer.h"
#include "di/io/writer_print.h"
#include "ttx/features.h"
#include "ttx/focus_event.h"
#include "ttx/key_event.h"
#include "ttx/mouse_event.h"
#include "ttx/paste_event.h"
#include "ttx/terminal/escapes/device_attributes.h"
#include "ttx/terminal/escapes/device_status.h"
#include "ttx/terminal/escapes/mode.h"
#include "ttx/terminal/escapes/osc_66.h"
#include "ttx/terminal_input.h"
#include "ttx/utf8_stream_decoder.h"

namespace ttx {
class FeatureDetector {
public:
    auto done() const -> bool { return m_done; }
    auto result() const -> Feature { return m_result; }

    void handle_event(KeyEvent const&) {}
    void handle_event(MouseEvent const&) {}
    void handle_event(FocusEvent const&) {}
    void handle_event(PasteEvent const&) {}

    void handle_event(terminal::PrimaryDeviceAttributes const&) { m_done = true; }

    void handle_event(terminal::ModeQueryReply const& reply) {
        switch (reply.dec_mode) {
            case terminal::DecMode::SynchronizedOutput:
                if (reply.support == terminal::ModeSupport::Set || reply.support == terminal::ModeSupport::Unset) {
                    m_result |= Feature::SyncronizedOutput;
                }
                break;
            default:
                break;
        }
    }

    void handle_event(terminal::CursorPositionReport const& report) {
        if (report != m_prev_cursor) {
            // Upgrade kitty text sizing support
            if (m_prev_cursor) {
                if (!(m_result & Feature::TextSizingWidth)) {
                    m_result |= Feature::TextSizingWidth;
                } else {
                    m_result |= Feature::TextSizingFull;
                }
            }

            m_prev_cursor = report;
        }
    }

private:
    Feature m_result = Feature::None;
    bool m_done = false;

    // State for detecting text sizing protocol.
    di::Optional<terminal::CursorPositionReport> m_prev_cursor;
};

auto detect_features(dius::SyncFile& terminal) -> di::Result<Feature> {
    // For feature detection, the general strategy is to write a byte series to the host
    // terminal, ending with a query for device attributes. All terminals will respond to
    // the device attributes, so we can determine when a terminal igonres a specific request.
    auto request_buffer = di::VectorWriter<> {};

    // Synchronoized output query
    di::writer_print<di::String::Encoding>(request_buffer, "\033[?{}$p"_sv, u32(terminal::DecMode::SynchronizedOutput));

    // Text sizing protocol query
    di::writer_print<di::String::Encoding>(request_buffer, "\033[6n"_sv);
    di::writer_print<di::String::Encoding>(request_buffer, "{}"_sv,
                                           terminal::OSC66 { .info = { .width = 2 }, .text = "a"_sv }.serialize());
    di::writer_print<di::String::Encoding>(request_buffer, "\033[6n"_sv);
    di::writer_print<di::String::Encoding>(request_buffer, "{}"_sv,
                                           terminal::OSC66 { .info = { .scale = 2 }, .text = "a"_sv }.serialize());
    di::writer_print<di::String::Encoding>(request_buffer, "\033[6n"_sv);

    // DA1 request.
    di::writer_print<di::String::Encoding>(request_buffer, "\033[c"_sv);

    // Clear the line, as we have been writing text:
    di::writer_print<di::String::Encoding>(request_buffer, "\r\033[K"_sv);

    auto _ = TRY(terminal.enter_raw_mode());
    auto text = di::move(request_buffer).vector();
    TRY(terminal.write_exactly(di::as_bytes(text.span())));

    // Read responses out.
    auto buffer = di::Vector<byte> {};
    buffer.resize(4096);

    auto detector = FeatureDetector {};
    auto parser = TerminalInputParser {};
    auto utf8_decoder = Utf8StreamDecoder {};
    while (!detector.done()) {
        auto nread = TRY(terminal.read_some(buffer.span()));

        auto utf8_string = utf8_decoder.decode(buffer | di::take(nread));
        auto events = parser.parse(utf8_string);
        for (auto const& event : events) {
            di::visit(
                [&](auto const& ev) {
                    detector.handle_event(ev);
                },
                event);
        }
    }

    return detector.result();
}
}
