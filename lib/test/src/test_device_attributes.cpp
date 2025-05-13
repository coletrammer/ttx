#include "di/test/prelude.h"
#include "ttx/escape_sequence_parser.h"
#include "ttx/terminal/escapes/device_attributes.h"

namespace device_attributes {
using namespace ttx;
using namespace ttx::terminal;

static void test_parse_primary() {
    struct Case {
        CSI input {};
        di::Optional<PrimaryDeviceAttributes> expected {};
    };

    auto cases = di::Array {
        // Empty.
        Case {
            CSI { .intermediate = "?"_s, .terminator = 'c' },
            { PrimaryDeviceAttributes {} },
        },
        // Normal
        Case {
            CSI { .intermediate = "?"_s, .params = { { 1 }, { 0 } }, .terminator = 'c' },
            { PrimaryDeviceAttributes { .attributes = { 1, 0 } } },
        },
        // Invalid.
        Case {
            CSI(),
        },
        Case {
            CSI { .intermediate = "?"_s },
        },
        Case {
            CSI { .terminator = 'c' },
        },
    };

    for (auto const& [input, expected] : cases) {
        auto result = PrimaryDeviceAttributes::from_csi(input);
        ASSERT_EQ(expected, result);
    }
}

static void test_serialize_primary() {
    struct Case {
        PrimaryDeviceAttributes input {};
        di::StringView expected {};
    };

    auto cases = di::Array {
        // Empty
        Case {
            {},
            "\033[?c"_sv,
        },
        // Normal
        Case {
            PrimaryDeviceAttributes { .attributes = { 1, 0 } },
            "\033[?1;0c"_sv,
        },
    };

    for (auto const& [input, expected] : cases) {
        auto result = input.serialize();
        ASSERT_EQ(expected, result);
    }
}

TEST(device_attributes, test_parse_primary)
TEST(device_attributes, test_serialize_primary)
}
