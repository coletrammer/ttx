#include "di/test/prelude.h"
#include "ttx/terminal/escapes/osc_7.h"

namespace osc_7 {
using namespace ttx;
using namespace ttx::terminal;

static void test_parse() {
    struct Case {
        di::TransparentStringView input {};
        di::Optional<OSC7> expected {};
    };

    auto cases = di::Array {
        // Empty.
        Case {
            "file:///"_tsv,
            { OSC7 { ""_ts, "/"_p } },
        },
        // Missing hostname
        Case {
            "file:///dev/null"_tsv,
            { OSC7 { ""_ts, "/dev/null"_p } },
        },
        // Normal
        Case {
            "file://host/dev/null"_tsv,
            { OSC7 { "host"_ts, "/dev/null"_p } },
        },
        // Percent encoded
        Case {
            "file://host/dev/null%20test"_tsv,
            { OSC7 { "host"_ts, "/dev/null test"_p } },
        },
        // Kitty variant
        Case {
            "kitty-shell-cwd://host/dev/null%20test"_tsv,
            { OSC7 { "host"_ts, "/dev/null%20test"_p } },
        },
        // Invalid.
        Case {
            ""_tsv,
            {},
        },
        Case {
            "file://"_tsv,
            {},
        },
        Case {
            "asdf://host/dev/null"_tsv,
            {},
        },
        Case {
            "file://host/dev/%A"_tsv,
            {},
        },
    };

    for (auto const& [input, expected] : cases) {
        auto result = OSC7::parse(input);
        ASSERT_EQ(expected, result);
    }
}

static void test_serialize() {
    struct Case {
        OSC7 input {};
        di::StringView expected {};
    };

    auto cases = di::Array {
        // Empty
        Case {
            {},
            "\033]7;file://\033\\"_sv,
        },
        // Normal
        Case {
            OSC7 { "host"_ts, "/dev/null test"_p },
            "\033]7;file://host/dev/null%20test\033\\"_sv,
        },
    };

    for (auto const& [input, expected] : cases) {
        auto result = input.serialize();
        ASSERT_EQ(expected, result);
    }
}

TEST(osc_7, test_parse)
TEST(osc_7, test_serialize)
}
