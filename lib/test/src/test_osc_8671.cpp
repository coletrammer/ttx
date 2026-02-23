#include "di/test/prelude.h"
#include "ttx/terminal/escapes/osc_8671.h"
#include "ttx/terminal/navigation_direction.h"

namespace osc_8671 {
using namespace ttx;
using namespace ttx::terminal;

static void test_parse_and_serialize() {
    struct Case {
        di::StringView input {};
        di::Optional<OSC8671> expected {};
        bool doesnt_roundtrip { false };
    };

    auto cases = di::Array {
        Case {
            "t=supported"_sv,
            OSC8671 {
                .type = SeamlessNavigationRequestType::Supported,
            },
        },
        Case {
            "t=register"_sv,
            OSC8671 {
                .type = SeamlessNavigationRequestType::Register,
            },
        },
        Case {
            "t=register:h=true"_sv,
            OSC8671 {
                .type = SeamlessNavigationRequestType::Register,
                .hide_cursor_on_enter = true,
            },
        },
        Case {
            "t=unregister"_sv,
            OSC8671 {
                .type = SeamlessNavigationRequestType::Unregister,
            },
        },
        Case {
            "t=navigate;left"_sv,
            OSC8671 {
                .type = SeamlessNavigationRequestType::Navigate,
                .direction = NavigateDirection::Left,
            },
        },
        Case {
            "t=navigate;right"_sv,
            OSC8671 {
                .type = SeamlessNavigationRequestType::Navigate,
                .direction = NavigateDirection::Right,
            },
        },
        Case {
            "t=navigate;up"_sv,
            OSC8671 {
                .type = SeamlessNavigationRequestType::Navigate,
                .direction = NavigateDirection::Up,
            },
        },
        Case {
            "t=navigate;down"_sv,
            OSC8671 {
                .type = SeamlessNavigationRequestType::Navigate,
                .direction = NavigateDirection::Down,
            },
        },
        Case {
            "t=navigate:w=true;down"_sv,
            OSC8671 {
                .type = SeamlessNavigationRequestType::Navigate,
                .direction = NavigateDirection::Down,
                .wrap_mode = NavigateWrapMode::Allow,
            },
        },
        Case {
            "t=navigate:w=true:id=asdf;down"_sv,
            OSC8671 {
                .type = SeamlessNavigationRequestType::Navigate,
                .direction = NavigateDirection::Down,
                .id = "asdf"_s,
                .wrap_mode = NavigateWrapMode::Allow,
            },
        },
        Case {
            "t=acknowledge:w=true:id=asdf;down"_sv,
            OSC8671 {
                .type = SeamlessNavigationRequestType::Acknowledge,
                .direction = NavigateDirection::Down,
                .id = "asdf"_s,
                .wrap_mode = NavigateWrapMode::Allow,
            },
        },
        Case {
            "t=navigate:w=false;down"_sv,
            OSC8671 {
                .type = SeamlessNavigationRequestType::Navigate,
                .direction = NavigateDirection::Down,
            },
            true,
        },
        Case {
            "t=enter:r=1,100;down"_sv,
            OSC8671 {
                .type = SeamlessNavigationRequestType::Enter,
                .direction = NavigateDirection::Down,
                .range = di::Tuple { 1u, 100u },
            },
        },
        // Invalid
        Case {
            ";t=supported"_sv,
        },
        Case {
            "t=supported:r=1,100"_sv,
        },
        Case {
            "t=supported:id=AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"_sv,
        },
        Case {
            "t=supported:invalid=bad"_sv,
        },
        Case {
            "t=enter:r=100,1;down"_sv,
        },
        Case {
            "t=enter:h=true;down"_sv,
        },
        Case {
            "t=enter:w=true;down"_sv,
        },
        Case {
            "t=enter:r=-1,5;down"_sv,
        },
        Case {
            "t=navigate:r=-1,5:id=asdf:w=true;down"_sv,
        },
        Case {
            "t=bad"_sv,
        },
        Case {
            "t=navigation"_sv,
        },
        Case {
            "t=navigation;bad"_sv,
        },
        Case {
            "t=navigation:w=bad;left"_sv,
        },
        Case {
            ""_sv,
        },
        Case {
            ";"_sv,
        },
    };

    for (auto const& [input, expected, doesnt_roundtrip] : cases) {
        auto result = OSC8671::parse(input);
        ASSERT_EQ(expected, result);

        if (result.has_value() && !doesnt_roundtrip) {
            auto serialized = result.value().serialize();
            ASSERT_EQ(di::format("\033]8671;{}\033\\"_sv, input), serialized);
        }
    }
}

TEST(osc_8671, test_parse_and_serialize)
}
