#include "di/test/prelude.h"
#include "ttx/escape_sequence_parser.h"
#include "ttx/terminal/escapes/device_status.h"

namespace device_status {
using namespace ttx;
using namespace ttx::terminal;

static void test_parse_operating_status_report() {
    struct Case {
        CSI input {};
        di::Optional<OperatingStatusReport> expected {};
    };

    auto cases = di::Array {
        // Valid.
        Case {
            CSI { .params = { { 0 } }, .terminator = 'n' },
            OperatingStatusReport { .malfunction = false },
        },
        Case {
            CSI { .params = { { 3 } }, .terminator = 'n' },
            OperatingStatusReport { .malfunction = true },
        },
        // Invalid.
        Case {
            CSI(),
        },
        Case {
            CSI { .intermediate = "?"_s, .params = { { 3 } }, .terminator = 'n' },
        },
        Case {
            CSI { .params = { { 3 } }, .terminator = 'N' },
        },
        Case {
            CSI { .params = { { 2 } }, .terminator = 'n' },
        },
        Case {
            CSI { .params = { { 0 }, { 0 } }, .terminator = 'n' },
        },
    };

    for (auto const& [input, expected] : cases) {
        auto result = OperatingStatusReport::from_csi(input);
        ASSERT_EQ(expected, result);
    }
}

static void test_serialize_operating_status_report() {
    struct Case {
        OperatingStatusReport input {};
        di::StringView expected {};
    };

    auto cases = di::Array {
        Case {
            OperatingStatusReport { .malfunction = false },
            "\033[0n"_sv,
        },
        Case {
            OperatingStatusReport { .malfunction = true },
            "\033[3n"_sv,
        },
    };

    for (auto const& [input, expected] : cases) {
        auto result = input.serialize();
        ASSERT_EQ(expected, result);
    }
}

static void test_parse_cursor_position_report() {
    struct Case {
        CSI input {};
        di::Optional<CursorPositionReport> expected {};
    };

    auto cases = di::Array {
        // Valid.
        Case {
            CSI { .params = { { 2 }, { 5 } }, .terminator = 'R' },
            CursorPositionReport { .row = 1, .col = 4 },
        },
        // Invalid.
        Case {
            CSI(),
        },
        Case {
            CSI { .intermediate = "?"_s, .params = { { 2 }, { 5 } }, .terminator = 'R' },
        },
        Case {
            CSI { .params = { { 2 }, { 5 } }, .terminator = 'r' },
        },
    };

    for (auto const& [input, expected] : cases) {
        auto result = CursorPositionReport::from_csi(input);
        ASSERT_EQ(expected, result);
    }
}

static void test_serialize_cursor_position_report() {
    struct Case {
        CursorPositionReport input {};
        di::StringView expected {};
    };

    auto cases = di::Array {
        // ANSI
        Case {
            CursorPositionReport { 2, 3 },
            "\033[3;4R"_sv,
        },
    };

    for (auto const& [input, expected] : cases) {
        auto result = input.serialize();
        ASSERT_EQ(expected, result);
    }
}

TEST(device_status, test_parse_operating_status_report)
TEST(device_status, test_serialize_operating_status_report)
TEST(device_status, test_parse_cursor_position_report)
TEST(device_status, test_serialize_cursor_position_report)
}
