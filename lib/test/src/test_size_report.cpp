#include "di/test/prelude.h"
#include "ttx/escape_sequence_parser.h"
#include "ttx/key_event_io.h"
#include "ttx/terminal/escapes/size_report.h"

namespace size_report {
using namespace ttx;
using namespace ttx::terminal;

static void test_parse_text_area_pixel_size_report() {
    struct Case {
        CSI input {};
        di::Optional<TextAreaPixelSizeReport> expected {};
    };

    auto cases = di::Array {
        // Valid.
        Case {
            CSI { .params = { { 4 }, { 12 }, { 13 } }, .terminator = 't' },
            TextAreaPixelSizeReport { .xpixels = 13, .ypixels = 12 },
        },
        // Invalid.
        Case {
            CSI(),
        },
        Case {
            CSI { .intermediate = "?"_s, .params = { { 4 }, { 1 }, { 2 } }, .terminator = 't' },
        },
        Case {
            CSI { .params = { { 4 }, { 1 }, { 2 } }, .terminator = 'T' },
        },
        Case {
            CSI { .params = { { 5 }, { 1 }, { 2 } }, .terminator = 't' },
        },
        Case {
            CSI { .params = { { 4 } }, .terminator = 't' },
        },
        Case {
            CSI { .params = { { 4 }, { 0 } }, .terminator = 't' },
        },
    };

    for (auto const& [input, expected] : cases) {
        auto result = TextAreaPixelSizeReport::from_csi(input);
        ASSERT_EQ(expected, result);
    }
}

static void test_serialize_text_area_pixel_size_report() {
    struct Case {
        TextAreaPixelSizeReport input {};
        di::StringView expected {};
    };

    auto cases = di::Array {
        Case {
            TextAreaPixelSizeReport { .xpixels = 13, .ypixels = 12 },
            "\033[4;12;13t"_sv,
        },
    };

    for (auto const& [input, expected] : cases) {
        auto result = input.serialize();
        ASSERT_EQ(expected, result);
    }
}

static void test_parse_cell_pixel_size_report() {
    struct Case {
        CSI input {};
        di::Optional<CellPixelSizeReport> expected {};
    };

    auto cases = di::Array {
        // Valid.
        Case {
            CSI { .params = { { 6 }, { 12 }, { 13 } }, .terminator = 't' },
            CellPixelSizeReport { .xpixels = 13, .ypixels = 12 },
        },
        // Invalid.
        Case {
            CSI(),
        },
        Case {
            CSI { .intermediate = "?"_s, .params = { { 6 }, { 1 }, { 2 } }, .terminator = 't' },
        },
        Case {
            CSI { .params = { { 6 }, { 1 }, { 2 } }, .terminator = 'T' },
        },
        Case {
            CSI { .params = { { 5 }, { 1 }, { 2 } }, .terminator = 't' },
        },
        Case {
            CSI { .params = { { 6 } }, .terminator = 't' },
        },
        Case {
            CSI { .params = { { 6 }, { 0 } }, .terminator = 't' },
        },
    };

    for (auto const& [input, expected] : cases) {
        auto result = CellPixelSizeReport::from_csi(input);
        ASSERT_EQ(expected, result);
    }
}

static void test_serialize_cell_pixel_size_report() {
    struct Case {
        CellPixelSizeReport input {};
        di::StringView expected {};
    };

    auto cases = di::Array {
        // ANSI
        Case {
            CellPixelSizeReport { .xpixels = 2, .ypixels = 3 },
            "\033[6;3;2t"_sv,
        },
    };

    for (auto const& [input, expected] : cases) {
        auto result = input.serialize();
        ASSERT_EQ(expected, result);
    }
}

static void test_parse_text_area_size_report() {
    struct Case {
        CSI input {};
        di::Optional<TextAreaSizeReport> expected {};
    };

    auto cases = di::Array {
        // Valid.
        // Valid.
        Case {
            CSI { .params = { { 8 }, { 12 }, { 13 } }, .terminator = 't' },
            TextAreaSizeReport { .cols = 13, .rows = 12 },
        },
        // Invalid.
        Case {
            CSI(),
        },
        Case {
            CSI { .intermediate = "?"_s, .params = { { 8 }, { 1 }, { 2 } }, .terminator = 't' },
        },
        Case {
            CSI { .params = { { 8 }, { 1 }, { 2 } }, .terminator = 'T' },
        },
        Case {
            CSI { .params = { { 5 }, { 1 }, { 2 } }, .terminator = 't' },
        },
        Case {
            CSI { .params = { { 8 } }, .terminator = 't' },
        },
        Case {
            CSI { .params = { { 8 }, { 0 } }, .terminator = 't' },
        },
    };

    for (auto const& [input, expected] : cases) {
        auto result = TextAreaSizeReport::from_csi(input);
        ASSERT_EQ(expected, result);
    }
}

static void test_serialize_text_area_size_report() {
    struct Case {
        TextAreaSizeReport input {};
        di::StringView expected {};
    };

    auto cases = di::Array {
        Case {
            TextAreaSizeReport { .cols = 12, .rows = 13 },
            "\033[8;13;12t"_sv,
        },
    };

    for (auto const& [input, expected] : cases) {
        auto result = input.serialize();
        ASSERT_EQ(expected, result);
    }
}

static void test_parse_in_band_size_report() {
    struct Case {
        CSI input {};
        di::Optional<InBandSizeReport> expected {};
    };

    auto cases = di::Array {
        // Valid.
        Case {
            CSI { .params = { { 48 }, { 12 }, { 13 }, { 23 }, { 22 } }, .terminator = 't' },
            InBandSizeReport { .size = { .rows = 12, .cols = 13, .xpixels = 22, .ypixels = 23 } },
        },
        // Invalid.
        Case {
            CSI(),
        },
        Case {
            CSI { .intermediate = "?"_s, .params = { { 48 }, { 1 }, { 2 }, { 3 }, { 4 } }, .terminator = 't' },
        },
        Case {
            CSI { .params = { { 48 }, { 1 }, { 2 }, { 3 }, { 4 } }, .terminator = 'T' },
        },
        Case {
            CSI { .params = { { 5 }, { 1 }, { 2 }, { 3 }, { 4 } }, .terminator = 't' },
        },
        Case {
            CSI { .params = { { 48 } }, .terminator = 't' },
        },
        Case {
            CSI { .params = { { 48 }, { 0 }, { 0 }, { 0 } }, .terminator = 't' },
        },
    };

    for (auto const& [input, expected] : cases) {
        auto result = InBandSizeReport::from_csi(input);
        ASSERT_EQ(expected, result);
    }
}

static void test_serialize_in_band_size_report() {
    struct Case {
        InBandSizeReport input {};
        di::StringView expected {};
    };

    auto cases = di::Array {
        Case {
            InBandSizeReport { .size = { .rows = 2, .cols = 3, .xpixels = 4, .ypixels = 5 } },
            "\033[48;2;3;5;4t"_sv,
        },
    };

    for (auto const& [input, expected] : cases) {
        auto result = input.serialize();
        ASSERT_EQ(expected, result);
    }
}

TEST(size_report, test_parse_text_area_pixel_size_report)
TEST(size_report, test_serialize_text_area_pixel_size_report)
TEST(size_report, test_parse_cell_pixel_size_report)
TEST(size_report, test_serialize_cell_pixel_size_report)
TEST(size_report, test_parse_text_area_size_report)
TEST(size_report, test_serialize_text_area_size_report)
TEST(size_report, test_parse_in_band_size_report)
TEST(size_report, test_serialize_in_band_size_report)
}
