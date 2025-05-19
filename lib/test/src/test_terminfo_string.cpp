#include "di/test/prelude.h"
#include "ttx/escape_sequence_parser.h"
#include "ttx/terminal/capability.h"
#include "ttx/terminal/escapes/terminfo_string.h"

namespace terminfo_string {
using namespace ttx;
using namespace ttx::terminal;

static void test_parse() {
    struct Case {
        DCS input {};
        di::Optional<TerminfoString> expected {};
    };

    auto cases = di::Array {
        // Valid.
        Case {
            DCS { .intermediate = "+r"_s, .params = { { 0 } }, .data = ""_s },
            TerminfoString {},
        },
        Case {
            DCS { .intermediate = "+r"_s, .params = { { 1 } }, .data = "6162"_s },
            TerminfoString { "ab"_ts },
        },
        Case {
            DCS { .intermediate = "+r"_s, .params = { { 1 } }, .data = "6162=4142"_s },
            TerminfoString { "ab"_ts, "AB"_ts },
        },
        // Invalid.
        Case {
            DCS(),
        },
        Case {
            DCS { .intermediate = "$r"_s, .params = { { 1 } }, .data = "6162=4142"_s },
        },
        Case {
            DCS { .intermediate = "+r"_s, .params = { { 2 } }, .data = "6162=4142"_s },
        },
        Case {
            DCS { .intermediate = "+r"_s, .params = { { 2 } }, .data = "6162="_s },
        },
        Case {
            DCS { .intermediate = "+r"_s, .params = { { 2 } }, .data = ""_s },
        },
    };

    for (auto const& [input, expected] : cases) {
        auto result = TerminfoString::from_dcs(input);
        ASSERT_EQ(expected, result);
    }
}

static void test_from_capability() {
    struct Case {
        Capability input {};
        TerminfoString expected {};
    };

    auto cases = di::Array {
        Case {
            Capability {
                .long_name = {},
                .short_name = "ab"_tsv,
                .description = {},
            },
            TerminfoString {
                .name = "ab"_ts,
            },
        },
        Case {
            Capability {
                .long_name = {},
                .short_name = "ab"_tsv,
                .value = 2u,
                .description = {},
            },
            TerminfoString {
                .name = "ab"_ts,
                .value = "2"_ts,
            },
        },
        Case {
            Capability {
                .long_name = {},
                .short_name = "xm"_tsv,
                .value = "\\E[<%i%p3%d;%p1%d;%p2%d;%?%p4%tM%em%;"_tsv,
                .description = {},
            },
            TerminfoString {
                .name = "xm"_ts,
                .value = "\\E[<%i%p3%d;%p1%d;%p2%d;%?%p4%tM%em%;"_ts,
            },
        },
        // Unescaping
        Case {
            Capability {
                .long_name = {},
                .short_name = "smxx"_tsv,
                .value = "\\E[9m"_tsv,
                .description = {},
            },
            TerminfoString {
                .name = "smxx"_ts,
                .value = "\x1b[9m"_ts,
            },
        },
        Case {
            Capability {
                .long_name = {},
                .short_name = "cub1"_tsv,
                .value = "^H"_tsv,
                .description = {},
            },
            TerminfoString {
                .name = "cub1"_ts,
                .value = "\b"_ts,
            },
        },
    };

    for (auto const& [input, expected] : cases) {
        auto result = TerminfoString::from_capability(input);
        ASSERT_EQ(expected, result);
    }
}

static void test_serialize() {
    struct Case {
        TerminfoString input {};
        di::StringView expected {};
    };

    auto cases = di::Array {
        Case {
            TerminfoString {},
            "\033P0+r\033\\"_sv,
        },
        Case {
            TerminfoString { "AB"_ts },
            "\033P1+r4142\033\\"_sv,
        },
        Case {
            TerminfoString { "\x05"_ts },
            "\033P1+r05\033\\"_sv,
        },
        Case {
            TerminfoString { "AB"_ts, "ab"_ts },
            "\033P1+r4142=6162\033\\"_sv,
        },
    };

    for (auto const& [input, expected] : cases) {
        auto result = input.serialize();
        ASSERT_EQ(expected, result);
    }
}

TEST(terminfo_string, test_parse)
TEST(terminfo_string, test_from_capability)
TEST(terminfo_string, test_serialize)
}
