#include "di/test/prelude.h"
#include "ttx/terminal/capability.h"
#include "ttx/terminal/escapes/terminfo_string.h"

namespace capability {
using namespace ttx;
using namespace ttx::terminal;

static void serialize() {
    auto names = di::Array {
        "ttx"_tsv,
        "ttx Multiplexer"_tsv,
    };

    auto capabilities = di::Array {
        Capability {
            .long_name = {},
            .short_name = "ccc"_tsv,
            .description = {},
            .enabled = false,
        },
        Capability {
            .long_name = {},
            .short_name = "am"_tsv,
            .description = {},
        },
        Capability {
            .long_name = {},
            .short_name = "colors"_tsv,
            .value = 256u,
            .description = {},
        },
        Capability {

            .long_name = {},
            .short_name = "smxx"_tsv,
            .value = "\\E[9m"_tsv,
            .description = {},
        },
    };

    auto terminfo = Terminfo(names, capabilities);

    auto result = terminfo.serialize();
    ASSERT_EQ(result, "ttx|ttx Multiplexer,\n"
                      "\tam,\n"
                      "\tcolors#256,\n"
                      "\tsmxx=\\E[9m,\n"_sv);
}

static void lookup() {
    struct Case {
        di::StringView input {};
        TerminfoString expected {};
    };

    auto cases = di::Array {
        // Valid
        Case {
            "436F"_sv,
            TerminfoString("Co"_ts, "256"_ts),
        },
        Case {
            "524742"_sv,
            TerminfoString("RGB"_ts),
        },
        Case {
            "62656C"_sv,
            TerminfoString("bel"_ts, "\a"_ts),
        },
        // Invaild
        Case("525252"_sv),
        Case("invalid"_sv),
    };

    for (auto const& [input, expected] : cases) {
        ASSERT_EQ(lookup_terminfo_string(input), expected);
    }
}

TEST(capability, serialize)
TEST(capability, lookup)
}
