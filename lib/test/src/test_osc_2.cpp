#include "di/test/prelude.h"
#include "ttx/terminal/escapes/osc_2.h"

namespace osc_2 {
using namespace ttx;
using namespace ttx::terminal;

static void test_parse() {
    struct Case {
        di::TransparentStringView input {};
        di::Optional<OSC2> expected {};
    };

    auto cases = di::Array {
        Case {
            ""_tsv,
            OSC2 {},
        },
        Case {
            "title"_tsv,
            OSC2 { .window_title = "title"_s },
        },
    };

    for (auto const& [input, expected] : cases) {
        auto result = OSC2::parse(input);
        ASSERT_EQ(expected, result);
    }
}

static void test_serialize() {
    struct Case {
        OSC2 input {};
        di::StringView expected {};
    };

    auto cases = di::Array {
        // Empty
        Case {
            {},
            "\033]2;\033\\"_sv,
        },
        // Normal
        Case {
            OSC2 { "title"_s },
            "\033]2;title\033\\"_sv,
        },
    };

    for (auto const& [input, expected] : cases) {
        auto result = input.serialize();
        ASSERT_EQ(expected, result);
    }
}

TEST(osc_2, test_parse)
TEST(osc_2, test_serialize)
}
