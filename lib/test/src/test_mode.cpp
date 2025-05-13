#include "di/test/prelude.h"
#include "ttx/escape_sequence_parser.h"
#include "ttx/terminal/escapes/mode.h"

namespace mode {
using namespace ttx;
using namespace ttx::terminal;

static void test_parse_mode_query_reply() {
    struct Case {
        CSI input {};
        di::Optional<ModeQueryReply> expected {};
    };

    auto cases = di::Array {
        // ANSI
        Case {
            CSI { .intermediate = "$"_s, .params = { { 4 }, { 2 } }, .terminator = 'y' },
            ModeQueryReply { .support = ModeSupport::Unset, .ansi_mode = AnsiMode(4) },
        },
        // DEC
        Case {
            CSI { .intermediate = "?$"_s, .params = { { 2026 }, { 1 } }, .terminator = 'y' },
            ModeQueryReply { .support = ModeSupport::Set, .dec_mode = DecMode::SynchronizedOutput },
        },
        // Invalid.
        Case {
            CSI(),
        },
        Case {
            CSI { .intermediate = "?$"_s },
        },
        Case {
            CSI { .intermediate = "$"_s },
        },
        Case {
            CSI { .terminator = 'y' },
        },
    };

    for (auto const& [input, expected] : cases) {
        auto result = ModeQueryReply::from_csi(input);
        ASSERT_EQ(expected, result);
    }
}

static void test_serialize_mode_query_reply() {
    struct Case {
        ModeQueryReply input {};
        di::StringView expected {};
    };

    auto cases = di::Array {
        // ANSI
        Case {
            ModeQueryReply { .support = ModeSupport::Unset, .ansi_mode = AnsiMode(4) },
            "\033[4;2$y"_sv,
        },
        // DEC
        Case {
            ModeQueryReply { .support = ModeSupport::Set, .dec_mode = DecMode::SynchronizedOutput },
            "\033[?2026;1$y"_sv,
        },
    };

    for (auto const& [input, expected] : cases) {
        auto result = input.serialize();
        ASSERT_EQ(expected, result);
    }
}

TEST(mode, test_parse_mode_query_reply)
TEST(mode, test_serialize_mode_query_reply)
}
