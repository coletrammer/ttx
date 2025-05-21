#include "di/test/prelude.h"
#include "ttx/terminal/escapes/osc_52.h"

namespace osc_52 {
using namespace ttx;
using namespace ttx::terminal;

static auto make_selections(di::SameAs<SelectionType> auto... args) {
    auto array = di::Array { args... };
    auto result = di::StaticVector<SelectionType, di::Constexpr<10zu>> {};
    for (auto x : array) {
        ASSERT(result.push_back(x));
    }
    return result;
}

static void test_parse() {
    struct Case {
        di::StringView input {};
        di::Optional<OSC52> expected {};
    };

    auto cases = di::Array {
        // Defaults.
        Case {
            ";"_sv,
            OSC52 { make_selections(SelectionType::Clipboard), {} },
        },
        // Query
        Case {
            "p;?"_sv,
            OSC52 { make_selections(SelectionType::Selection), {}, true },
        },
        // Data
        Case {
            "01s;abcd"_sv,
            OSC52 { make_selections(SelectionType::_0, SelectionType::_1, SelectionType::Selection), "abcd"_base64 },
        },
        // Duplicate selections
        Case {
            "01s000000000000000000000000000000;abcd"_sv,
            OSC52 { make_selections(SelectionType::_0, SelectionType::_1, SelectionType::Selection), "abcd"_base64 },
        },
        // Invalid base 64 yields empty
        Case {
            ";~~~~~~~~~~~~~~"_sv,
            OSC52 { make_selections(SelectionType::Clipboard), {} },
        },
        // Invalid
        Case {
            ""_sv,
        },
        Case {
            "q;"_sv,
        },
        Case {
            "c"_sv,
        },
    };

    for (auto const& [input, expected] : cases) {
        auto result = OSC52::parse(input);
        ASSERT_EQ(expected, result);
    }
}

static void test_serialize() {
    struct Case {
        OSC52 input {};
        di::StringView expected {};
    };

    auto cases = di::Array {
        // Empty
        Case {
            {},
            "\033]52;c;\033\\"_sv,
        },
        // Query
        Case {
            { make_selections(SelectionType::Selection), {}, true },
            "\033]52;p;?\033\\"_sv,
        },
        // Data
        Case {
            { make_selections(SelectionType::_0, SelectionType::_1), "YWJjZAo="_base64 },
            "\033]52;01;YWJjZAo=\033\\"_sv,
        },
    };

    for (auto const& [input, expected] : cases) {
        auto result = input.serialize();
        ASSERT_EQ(expected, result);
    }
}

TEST(osc_52, test_parse)
TEST(osc_52, test_serialize)
}
