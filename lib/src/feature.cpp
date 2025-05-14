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
struct ModeQuery {
    Feature feature = Feature::None;
    terminal::DecMode mode { terminal::DecMode::None };
};

constexpr static auto dec_mode_queries = di::Array {
    ModeQuery(Feature::SyncronizedOutput, terminal::DecMode::SynchronizedOutput),
    ModeQuery(Feature::ThemeDetection, terminal::DecMode::ThemeDetection),
    ModeQuery(Feature::InBandSizeReports, terminal::DecMode::InBandSizeReports),
    ModeQuery(Feature::GraphemeClusteringMode, terminal::DecMode::GraphemeClustering),
};

class FeatureDetector {
public:
    auto done() const -> bool { return m_done; }
    auto result() const -> Feature { return m_result; }
    auto need_to_disable_mode_2027() const { return m_need_to_restore_mode_2027; }

    void handle_event(KeyEvent const&) {}
    void handle_event(MouseEvent const&) {}
    void handle_event(FocusEvent const&) {}
    void handle_event(PasteEvent const&) {}

    void handle_event(terminal::PrimaryDeviceAttributes const&) { m_done = true; }

    void handle_event(terminal::ModeQueryReply const& reply) {
        for (auto [feature, mode] : dec_mode_queries) {
            if (mode == reply.dec_mode) {
                auto is_supported =
                    reply.support == terminal::ModeSupport::Set || reply.support == terminal::ModeSupport::Unset;
                if (mode == terminal::DecMode::GraphemeClustering) {
                    is_supported |= reply.support == terminal::ModeSupport::AlwaysSet;
                    if (reply.support == terminal::ModeSupport::Unset) {
                        m_need_to_restore_mode_2027 = true;
                    }
                }
                if (is_supported) {
                    m_result |= feature;
                }
            }
        }
    }

    void handle_event(terminal::CursorPositionReport const& report) {
        m_cursor_reports++;
        if (m_cursor_reports == 1) {
            // Basic grapheme cluster check. The ZWJ emoji sequence
            // should have width 2.
            if (report.col == 2) {
                m_result |= Feature::BasicGraphemeClustering;
            }
            return;
        }
        if (m_cursor_reports == 2) {
            // The full grapheme cluster check expects width 1. See
            // below for a detailed explanation.
            if (report.col == 1) {
                m_result |= Feature::FullGraphemeClustering;
            }
            return;
        }
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

    void handle_event(terminal::KittyKeyReport const&) { m_result |= Feature::KittyKeyProtocol; }

    void handle_event(terminal::StatusStringResponse const& response) {
        if (response.response.has_value() && response.response.value().contains("4:3m"_sv)) {
            m_result |= Feature::Undercurl;
        }
    }

private:
    Feature m_result = Feature::None;
    bool m_done = false;

    // State for detecting text sizing protocol.
    di::Optional<terminal::CursorPositionReport> m_prev_cursor;
    u32 m_cursor_reports { 0 };
    bool m_need_to_restore_mode_2027 { false };
};

auto detect_features(dius::SyncFile& terminal) -> di::Result<Feature> {
    // For feature detection, the general strategy is to write a byte series to the host
    // terminal, ending with a query for device attributes. All terminals will respond to
    // the device attributes, so we can determine when a terminal igonres a specific request.
    auto request_buffer = di::VectorWriter<> {};

    // DEC mode queries
    for (auto [_, mode] : dec_mode_queries) {
        di::writer_print<di::String::Encoding>(request_buffer, "\033[?{}$p"_sv, u32(mode));
    }

    // Undercurl support query (this sets urderline mode=3 (undercurl) and then requests what the
    // terminal currently thinks the graphics attributes are).
    di::writer_print<di::String::Encoding>(request_buffer, "\x1b[0m\x1b[4:3m\x1bP$qm\x1b\\\x1b[0m"_sv);

    // Kitty keyboard protocol query
    di::writer_print<di::String::Encoding>(request_buffer, "\033[?u"_sv);

    // Grapheme clustering support query
    // Although DEC mode 2027 exists, it isn't all too helpful because as of this writing,
    // all known terminals which implement mode 2027 do not fully support Unicode 16 grapheme
    // clustering. Additionally, kitty 0.42.0 supports grapheme clustering but does not advertise
    // support for mode 2027 (and it behaves slightly different anyway, because of its handling
    // of variation selector 15).
    //
    // We implement the Unicode handling specified by
    // [Kitty](https://github.com/kovidgoyal/kitty/blob/master/docs/text-sizing-protocol.rst#the-algorithm-for-splitting-text-into-cells),
    // which is mostly aligned with the spec for mode 2027. However, if we split cells using
    // the kitty spec we need to adjust our rendering to handle edge cases in outer terminals
    // which "support" mode 2027 but don't fully agree with us. We also need to check for terminals
    // with legacy behavior which don't bother doing any grapheme clustering, as in those cases
    // its better to disable support for mode 2027 ourselves.
    //
    // As a side note: the explicit text sizing protocol by kitty completely fixes this issues,
    // and so it will be used by our rendering if possible.
    //
    // To detect basic support for grapheme clustering we first enable mode 2027, and then
    // send a black cat emoji (which has a ZWJ).
    //
    // To detect conformance with the kitty Unicode spec I found one of the automated tests
    // which fail on all known terminals.
    //
    // Test 815:
    // ÷ [0.2] LATIN SMALL LETTER A (Other) ÷ [999.0] ARABIC NUMBER SIGN (Prepend) × [9.2] LATIN SMALL LETTER B (Other)
    // According to the kitty spec, this only moves the cursor by 1 cells, because the arabic number sign has width 0
    // and thus is placed into the first cell, and 'b' gets placed into the same cell because there is no break between
    // it and the arabic number sign. This was tested in the following terminals which have grapheme clustering support
    // and all of them failed:
    // - kitty 0.40.1
    // - ghostty 1.1.3
    // - contour 0.5.1
    // - foot 1.21.0
    // - wezterm 0-unstable-2025-02-23
    //
    // Obviously, other terminals which don't support graphemes at all will fail. The following known terminals pass
    // this check:
    // - kitty 0.42.0
    // - ttx (when running in a terminal with some support for grapheme clustering)

    // Start by doing the basic test
    di::writer_print<di::String::Encoding>(request_buffer, "\033[?2027h"_sv);
    di::writer_print<di::String::Encoding>(request_buffer, u8"\r\033[K🐈‍⬛\033[6n"_sv);
    di::writer_print<di::String::Encoding>(request_buffer, u8"\r\033[Ka\u0600b\033[6n"_sv);

    // Text sizing protocol query
    di::writer_print<di::String::Encoding>(request_buffer, "\r\033[K\033[6n"_sv);
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

    auto result = detector.result();
    if (detector.need_to_disable_mode_2027()) {
        // Be a good citizen, and restore the mode. We need to enable it to properly
        // test the implementation.
        di::writer_print<di::String::Encoding>(terminal, "\033[?2027l"_sv);
    }
    return result;
}
}
