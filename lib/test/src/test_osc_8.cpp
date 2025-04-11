#include "di/test/prelude.h"
#include "ttx/escape_sequence_parser.h"
#include "ttx/terminal/escapes/osc_8.h"
#include "ttx/terminal/hyperlink.h"

namespace osc_8 {
using namespace ttx;
using namespace ttx::terminal;

static void test_parse() {
    struct Case {
        di::StringView input {};
        di::Optional<OSC8> expected {};
    };

    auto cases = di::Array {
        // Normal clear.
        Case {
            ";"_sv,
            { OSC8 {} },
        },
        // Invalid.
        Case {
            ""_sv,
            {},
        },
        Case {
            ";https://example.com;extra"_sv,
            {},
        },
        // Implicit id.
        Case {
            ";https://example.com"_sv,
            { OSC8 { .params = {}, .uri = "https://example.com"_s } },
        },
        // Extra params
        Case {
            "id=h1:foo=bar;https://example.com"_sv,
            { OSC8 { .params = di::Array { di::Tuple { "id"_s, "h1"_s }, di::Tuple { "foo"_s, "bar"_s } } |
                               di::as_rvalue | di::to<di::TreeMap>(),
                     .uri = "https://example.com"_s } },
        },
    };

    for (auto const& [input, expected] : cases) {
        auto result = OSC8::parse(input);
        ASSERT_EQ(expected, result);
    }
}

static void test_from_hyperlink() {
    struct Case {
        di::Optional<Hyperlink> input {};
        OSC8 expected {};
    };

    auto cases = di::Array {
        // Normal clear.
        Case {
            {},
            {},
        },
        // Normal
        Case {
            Hyperlink { .uri = "https://example.com"_s, .id = "h1"_s },
            { OSC8 { .params = di::Array { di::Tuple { "id"_s, "h1"_s } } | di::as_rvalue | di::to<di::TreeMap>(),
                     .uri = "https://example.com"_s } },
        },
    };

    for (auto const& [input, expected] : cases) {
        auto result = OSC8::from_hyperlink(input.transform(di::cref));
        ASSERT_EQ(expected, result);
    }
}

static void test_serialize() {
    struct Case {
        OSC8 input {};
        di::StringView expected {};
    };

    auto cases = di::Array {
        // Normal clear.
        Case {
            {},
            "\033]8;;\033\\"_sv,
        },
        // Normal
        Case {
            OSC8 { .params = di::Array { di::Tuple { "id"_s, "h1"_s } } | di::as_rvalue | di::to<di::TreeMap>(),
                   .uri = "https://example.com"_s },
            "\033]8;id=h1;https://example.com\033\\"_sv,
        },
    };

    for (auto const& [input, expected] : cases) {
        auto result = input.serialize();
        ASSERT_EQ(expected, result);
    }
}

static void test_make_hyperlink() {
    struct Case {
        OSC8 input {};
        di::Optional<Hyperlink> expected {};
    };

    auto cases = di::Array {
        // Normal clear.
        Case { {}, {} },
        // Normal Implicit
        Case {
            OSC8 { .params = {}, .uri = "https://example.com"_s },
            Hyperlink { .uri = "https://example.com"_s, .id = "0"_s },
        },
        // Normal explicit
        Case {
            OSC8 { .params = di::Array { di::Tuple { "id"_s, "h1"_s } } | di::as_rvalue | di::to<di::TreeMap>(),
                   .uri = "https://example.com"_s },
            Hyperlink { .uri = "https://example.com"_s, .id = "h1"_s },
        },
        // Overly long id
        Case {
            OSC8 { .params = di::Array { di::Tuple { "id"_s, di::repeat(U'A', Hyperlink::max_id_length + 100) |
                                                                 di::to<di::String>() } } |
                             di::as_rvalue | di::to<di::TreeMap>(),
                   .uri = "https://example.com"_s },
            Hyperlink { .uri = "https://example.com"_s,
                        .id = di::repeat(U'A', Hyperlink::max_id_length) | di::to<di::String>() },
        },
    };

    auto f = [i = 0](di::Optional<di::StringView> id) mutable -> di::String {
        if (!id) {
            return di::to_string(i);
        }
        return id.value().to_owned();
    };

    for (auto const& [input, expected] : cases) {
        auto result = input.to_hyperlink(f);
        ASSERT_EQ(expected, result);
    }
}

TEST(osc_8, test_parse)
TEST(osc_8, test_from_hyperlink)
TEST(osc_8, test_serialize)
TEST(osc_8, test_make_hyperlink)
}
