#include "di/test/prelude.h"
#include "ttx/terminal/escapes/osc_133.h"
#include "ttx/terminal/multi_cell_info.h"

namespace osc_133 {
using namespace ttx;
using namespace ttx::terminal;

static void test_parse() {
    struct Case {
        di::StringView input {};
        di::Optional<OSC133> expected {};
    };

    auto cases = di::Array {
        // Begin.
        Case {
            "A"_sv,
            OSC133 { BeginPrompt {} },
        },
        Case {
            "A;aid=asdf"_sv,
            OSC133 { BeginPrompt { .application_id = "asdf"_s } },
        },
        Case {
            "A;aid=asdf;cl=m;k=r;redraw=0"_sv,
            OSC133 {
                BeginPrompt {
                    .application_id = "asdf"_s,
                    .click_mode = PromptClickMode::MultipleLeftRight,
                    .kind = PromptKind::Right,
                    .redraw = false,
                },
            },
        },
        // End prompt
        Case {
            "B"_sv,
            OSC133 { EndPrompt {} },
        },
        Case {
            "B;aid=asdf"_sv,
            OSC133 { EndPrompt {} },
        },
        // End input
        Case {
            "C"_sv,
            OSC133 { EndInput {} },
        },
        Case {
            "C;aid=asdf"_sv,
            OSC133 { EndInput {} },
        },
        // End command
        Case {
            "D"_sv,
            OSC133 { EndCommand {} },
        },
        Case {
            "D;2"_sv,
            OSC133 { EndCommand { .exit_code = 2 } },
        },
        Case {
            "D;2;err=;aid=asdf"_sv,
            OSC133 { EndCommand { .application_id = "asdf"_s, .exit_code = 2 } },
        },
        Case {
            "D;2;err=CANCEL;aid=asdf"_sv,
            OSC133 { EndCommand { .application_id = "asdf"_s, .exit_code = 2, .error = "CANCEL"_s } },
        },
        // Invalid.
        Case {
            "Ab"_sv,
        },
        Case {
            "E"_sv,
        },
        Case {
            "E;aid=asdf"_sv,
        },
    };

    for (auto const& [input, expected] : cases) {
        auto result = OSC133::parse(input);
        ASSERT_EQ(expected, result);
    }
}

static void test_serialize() {
    struct Case {
        OSC133 input {};
        di::StringView expected {};
    };

    auto cases = di::Array {
        // Begin
        Case {
            OSC133 { BeginPrompt {} },
            "\033]133;A;k=i\033\\"_sv,
        },
        Case {
            OSC133 { BeginPrompt { .application_id = "asdf"_s } },
            "\033]133;A;k=i;aid=asdf\033\\"_sv,
        },
        Case {
            OSC133 {
                BeginPrompt {
                    .application_id = "asdf"_s,
                    .click_mode = PromptClickMode::MultipleLeftRight,
                    .kind = PromptKind::Right,
                    .redraw = false,
                },
            },
            "\033]133;A;k=r;aid=asdf;cl=m;redraw=0\033\\"_sv,
        },
        // End prompt
        Case {
            OSC133 { EndPrompt {} },
            "\033]133;B\033\\"_sv,
        },
        // End input
        Case {
            OSC133 { EndInput {} },
            "\033]133;C\033\\"_sv,
        },
        // End command
        Case {
            OSC133 { EndCommand {} },
            "\033]133;D;0\033\\"_sv,
        },
        Case {
            OSC133 { EndCommand { .exit_code = 2 } },
            "\033]133;D;2\033\\"_sv,
        },
        Case {
            OSC133 { EndCommand { .application_id = "asdf"_s, .exit_code = 2 } },
            "\033]133;D;2;aid=asdf\033\\"_sv,
        },
        Case {
            OSC133 { EndCommand { .application_id = "asdf"_s, .exit_code = 2, .error = "CANCEL"_s } },
            "\033]133;D;2;err=CANCEL;aid=asdf\033\\"_sv,
        },
    };

    for (auto const& [input, expected] : cases) {
        auto result = input.serialize();
        ASSERT_EQ(expected, result);
    }
}

TEST(osc_133, test_parse)
TEST(osc_133, test_serialize)
}
