#include "di/test/prelude.h"
#include "ttx/features.h"
#include "ttx/palette.h"
#include "ttx/terminal/escapes/osc_21.h"

namespace osc_21 {
using namespace ttx;
using namespace ttx::terminal;

static void test_parse_color() {
    struct Case {
        di::StringView input {};
        Color expected;
    };

    auto cases = di::Array {
        // rgb
        Case {
            "rgb:a/11/345"_sv,
            Color(0xaa, 0x11, 0x34),
        },
        Case {
            "rgb:a/11/2340"_sv,
            Color(0xaa, 0x11, 0x23),
        },
        // hex
        Case {
            "#ABC"_sv,
            Color(0xa0, 0xb0, 0xc0),
        },
        Case {
            "#aabbcc"_sv,
            Color(0xaa, 0xbb, 0xcc),
        },
        Case {
            "#123abcDEF"_sv,
            Color(0x12, 0xab, 0xde),
        },
        Case {
            "#ABdc1234CDef"_sv,
            Color(0xab, 0x12, 0xcd),
        },
        // named
        Case {
            "dynamic"_sv,
            Color(Color::Type::Dynamic),
        },
        Case {
            "rED"_sv,
            Color(0xff, 0x00, 0x00),
        },
        Case {
            "SADDLE brown"_sv,
            Color(0x8b, 0x45, 0x13),
        },
        // TODO: rbgi
    };

    for (auto const& [input, expected] : cases) {
        auto input_osc = di::format("21;cursor={}"_sv, input);
        auto result = OSC21::parse(input_osc);
        ASSERT(result.has_value());
        ASSERT(!result.value().requests.empty());
        ASSERT_EQ(result.value().requests[0].color, expected);
    }
}

static void test_parse() {
    struct Case {
        di::StringView input {};
        di::Optional<OSC21> expected {};
    };

    auto cases = di::Array {
        // Kitty
        Case {
            .input = "21;0=red;256=blue;cursor=dynamic;3=;4=?;unknown=?;0=unknown"_sv,
            .expected =
                OSC21 {
                    .requests =
                        di::Array {
                            OSC21::Request {
                                .palette = PaletteIndex(0),
                                .color = Color(255, 0, 0),
                            },
                            OSC21::Request {
                                .palette = PaletteIndex::SpecialBold,
                                .color = Color(0, 0, 255),
                            },
                            OSC21::Request {
                                .palette = PaletteIndex::Cursor,
                                .color = Color(Color::Type::Dynamic),
                            },
                            OSC21::Request {
                                .palette = PaletteIndex(3),
                            },
                            OSC21::Request {
                                .query = true,
                                .palette = PaletteIndex(4),
                            },
                            OSC21::Request {
                                .query = true,
                                .kitty_color_name = "unknown"_s,
                            },
                        } |
                        di::as_rvalue | di::to<di::Vector>(),
                },
        },
        // OSC 4
        Case {
            .input = "4;0;red;2;?;256;blue;1000;red;0;invalid;partial"_sv,
            .expected =
                OSC21 {
                    .requests =
                        di::Array {
                            OSC21::Request {
                                .palette = PaletteIndex(0),
                                .color = Color(255, 0, 0),
                            },
                            OSC21::Request {
                                .query = true,
                                .palette = PaletteIndex(2),
                            },
                            OSC21::Request {
                                .palette = PaletteIndex::SpecialBold,
                                .color = Color(0, 0, 255),
                            },
                        } |
                        di::as_rvalue | di::to<di::Vector>(),
                },
        },
        // OSC 5
        Case {
            .input = "5;0;red;2;?;256;blue;1000;red;0;invalid;partial"_sv,
            .expected =
                OSC21 {
                    .requests =
                        di::Array {
                            OSC21::Request {
                                .palette = PaletteIndex(256),
                                .color = Color(255, 0, 0),
                            },
                            OSC21::Request {
                                .query = true,
                                .palette = PaletteIndex(258),
                            },
                        } |
                        di::as_rvalue | di::to<di::Vector>(),
                },
        },
        // OSC 10-19
        Case {
            .input = "12;red;;;;invalid;?;;blue"_sv,
            .expected =
                OSC21 {
                    .requests =
                        di::Array {
                            OSC21::Request {
                                .palette = PaletteIndex::Cursor,
                                .color = Color(255, 0, 0),
                            },
                            OSC21::Request {
                                .query = true,
                                .palette = PaletteIndex::SelectionBackground,
                            },
                            OSC21::Request {
                                .palette = PaletteIndex::SelectionForeground,
                                .color = Color(0, 0, 255),
                            },
                        } |
                        di::as_rvalue | di::to<di::Vector>(),
                },
        },
        // OSC 104
        Case {
            .input = "104;0;256;1000"_sv,
            .expected =
                OSC21 {
                    .requests =
                        di::Array {
                            OSC21::Request {
                                .palette = PaletteIndex(0),
                            },
                            OSC21::Request {
                                .palette = PaletteIndex(256),
                            },
                        } |
                        di::as_rvalue | di::to<di::Vector>(),
                },
        },
        // OSC 105
        Case {
            .input = "105;0;256;1000"_sv,
            .expected =
                OSC21 {
                    .requests =
                        di::Array {
                            OSC21::Request {
                                .palette = PaletteIndex(256),
                            },
                        } |
                        di::as_rvalue | di::to<di::Vector>(),
                },
        },
        // OSC 110-119
        Case {
            .input = "110"_sv,
            .expected =
                OSC21 {
                    .requests =
                        di::Array {
                            OSC21::Request {
                                .palette = PaletteIndex::Foreground,
                            },
                        } |
                        di::as_rvalue | di::to<di::Vector>(),
                },
        },
        Case {
            // Accpet trailing semicolon
            .input = "110;"_sv,
            .expected =
                OSC21 {
                    .requests =
                        di::Array {
                            OSC21::Request {
                                .palette = PaletteIndex::Foreground,
                            },
                        } |
                        di::as_rvalue | di::to<di::Vector>(),
                },
        },
    };

    for (auto const& [input, expected] : cases) {
        auto result = OSC21::parse(input);
        ASSERT_EQ(expected, result);
    }
}

static void test_serialize() {
    struct Case {
        OSC21 input {};
        di::StringView expected {};
        Feature features {};
    };

    auto cases = di::Array {
        Case {
            {},
            ""_sv,
        },
        Case {
            {},
            ""_sv,
            Feature::DynamicPaletteKitty,
        },
        // Kitty
        Case {
            .input =
                OSC21 {
                    .requests =
                        di::Array {
                            OSC21::Request {
                                .palette = PaletteIndex(0),
                                .color = Color(0x12, 0x34, 0x56),
                            },
                            OSC21::Request {
                                .palette = PaletteIndex(256),
                                .color = Color(0x78, 0x90, 0xab),
                            },
                            OSC21::Request {
                                .palette = PaletteIndex::Cursor,
                                .color = Color(0xcd, 0xef, 0xaa),
                            },
                            OSC21::Request {
                                .palette = PaletteIndex::CursorText,
                                .color = Color(0x00, 0x00, 0x00),
                            },
                            OSC21::Request {
                                .palette = PaletteIndex::Foreground,
                                .color = Color(Color::Type::Dynamic),
                            },
                            OSC21::Request {
                                .palette = PaletteIndex::Background,
                                .color = Color(Color::Type::Dynamic),
                            },
                            OSC21::Request {
                                .palette = PaletteIndex::SelectionForeground,
                                .color = Color(Color::Type::Dynamic),
                            },
                            OSC21::Request {
                                .palette = PaletteIndex::SelectionBackground,
                                .color = Color(Color::Type::Dynamic),
                            },
                            OSC21::Request {
                                .palette = PaletteIndex::Unknown,
                                .color = Color(Color::Type::Dynamic),
                                .kitty_color_name = "unknown_cursor_color"_s,
                            },
                        } |
                        di::as_rvalue | di::to<di::Vector>(),
                },
            .expected = "\x1b]21;0=rgb:12/34/56;256=rgb:78/90/ab;cursor=rgb:cd/ef/aa;cursor_text=rgb:00/00/00;foreground=dynamic;background=dynamic;selection_foreground=dynamic;selection_background=dynamic;unknown_cursor_color=dynamic\x1b\\"_sv,
            .features = Feature::DynamicPaletteKitty,
        },
        Case {
            .input =
                OSC21 {
                    .requests =
                        di::Array {
                            OSC21::Request {
                                .query = true,
                                .palette = PaletteIndex(0),
                            },
                            OSC21::Request {
                                .query = true,
                                .palette = PaletteIndex(256),
                            },
                            OSC21::Request {
                                .query = true,
                                .palette = PaletteIndex::Cursor,
                            },
                            OSC21::Request {
                                .query = true,
                                .palette = PaletteIndex::CursorText,
                            },
                            OSC21::Request {
                                .query = true,
                                .palette = PaletteIndex::Foreground,
                            },
                            OSC21::Request {
                                .query = true,
                                .palette = PaletteIndex::Background,
                            },
                            OSC21::Request {
                                .query = true,
                                .palette = PaletteIndex::SelectionForeground,
                            },
                            OSC21::Request {
                                .query = true,
                                .palette = PaletteIndex::SelectionBackground,
                            },
                        } |
                        di::as_rvalue | di::to<di::Vector>(),
                },
            .expected = "\x1b]21;0=?;256=?;cursor=?;cursor_text=?;foreground=?;background=?;selection_foreground=?;selection_background=?\x1b\\"_sv,
            .features = Feature::DynamicPaletteKitty,
        },
        Case {
            .input =
                OSC21 {
                    .requests =
                        di::Array {
                            OSC21::Request {
                                .palette = PaletteIndex(0),
                            },
                            OSC21::Request {
                                .palette = PaletteIndex(256),
                            },
                            OSC21::Request {
                                .palette = PaletteIndex::Cursor,
                            },
                            OSC21::Request {
                                .palette = PaletteIndex::CursorText,
                            },
                            OSC21::Request {
                                .palette = PaletteIndex::Foreground,
                            },
                            OSC21::Request {
                                .palette = PaletteIndex::Background,
                            },
                            OSC21::Request {
                                .palette = PaletteIndex::SelectionForeground,
                            },
                            OSC21::Request {
                                .palette = PaletteIndex::SelectionBackground,
                            },
                        } |
                        di::as_rvalue | di::to<di::Vector>(),
                },
            .expected = "\x1b]21;0=;256=;cursor=;cursor_text=;foreground=;background=;selection_foreground=;selection_background=\x1b\\"_sv,
            .features = Feature::DynamicPaletteKitty,
        },
        // Legacy
        Case {
            .input =
                OSC21 {
                    .requests =
                        di::Array {
                            OSC21::Request {
                                .palette = PaletteIndex(0),
                                .color = Color(0x12, 0x34, 0x56),
                            },
                            OSC21::Request {
                                .palette = PaletteIndex(256),
                                .color = Color(0x78, 0x90, 0xab),
                            },
                            OSC21::Request {
                                .palette = PaletteIndex::Cursor,
                                .color = Color(0xcd, 0xef, 0xaa),
                            },
                            OSC21::Request {
                                .palette = PaletteIndex::CursorText,
                                .color = Color(0x00, 0x00, 0x00),
                            },
                            OSC21::Request {
                                .palette = PaletteIndex::Foreground,
                                .color = Color(0x00, 0x00, 0x00),
                            },
                            OSC21::Request {
                                .palette = PaletteIndex::Background,
                                .color = Color(Color::Type::Dynamic),
                            },
                            OSC21::Request {
                                .palette = PaletteIndex::SelectionForeground,
                                .color = Color(Color::Type::Dynamic),
                            },
                            OSC21::Request {
                                .palette = PaletteIndex::SelectionBackground,
                                .color = Color(Color::Type::Dynamic),
                            },
                        } |
                        di::as_rvalue | di::to<di::Vector>(),
                },
            .expected = "\x1b]4;0;rgb:1212/3434/5656\x1b\\\x1b]5;0;rgb:7878/9090/abab\x1b\\\x1b]12;rgb:cdcd/efef/aaaa\x1b\\\x1b]10;rgb:0000/0000/0000\x1b\\\x1b]11;dynamic\x1b\\\x1b]19;dynamic\x1b\\\x1b]17;dynamic\x1b\\"_sv,
        },
        Case {
            .input =
                OSC21 {
                    .requests =
                        di::Array {
                            OSC21::Request {
                                .query = true,
                                .palette = PaletteIndex(0),
                            },
                            OSC21::Request {
                                .query = true,
                                .palette = PaletteIndex(256),
                            },
                            OSC21::Request {
                                .query = true,
                                .palette = PaletteIndex::Cursor,
                            },
                            OSC21::Request {
                                .query = true,
                                .palette = PaletteIndex::CursorText,
                            },
                            OSC21::Request {
                                .query = true,
                                .palette = PaletteIndex::Foreground,
                            },
                            OSC21::Request {
                                .query = true,
                                .palette = PaletteIndex::Background,
                            },
                            OSC21::Request {
                                .query = true,
                                .palette = PaletteIndex::SelectionForeground,
                            },
                            OSC21::Request {
                                .query = true,
                                .palette = PaletteIndex::SelectionBackground,
                            },
                        } |
                        di::as_rvalue | di::to<di::Vector>(),
                },
            .expected = "\x1b]4;0;?\x1b\\\x1b]5;0;?\x1b\\\x1b]12;?\x1b\\\x1b]10;?\x1b\\\x1b]11;?\x1b\\\x1b]19;?\x1b\\\x1b]17;?\x1b\\"_sv,
        },
        Case {
            .input =
                OSC21 {
                    .requests =
                        di::Array {
                            OSC21::Request {
                                .palette = PaletteIndex(0),
                            },
                            OSC21::Request {
                                .palette = PaletteIndex(256),
                            },
                            OSC21::Request {
                                .palette = PaletteIndex::Cursor,
                            },
                            OSC21::Request {
                                .palette = PaletteIndex::CursorText,
                            },
                            OSC21::Request {
                                .palette = PaletteIndex::Foreground,
                            },
                            OSC21::Request {
                                .palette = PaletteIndex::Background,
                            },
                            OSC21::Request {
                                .palette = PaletteIndex::SelectionForeground,
                            },
                            OSC21::Request {
                                .palette = PaletteIndex::SelectionBackground,
                            },
                        } |
                        di::as_rvalue | di::to<di::Vector>(),
                },
            .expected = "\x1b]104;0\x1b\\\x1b]105;0\x1b\\\x1b]112\x1b\\\x1b]110\x1b\\\x1b]111\x1b\\\x1b]119\x1b\\\x1b]117\x1b\\"_sv,
        },
    };

    for (auto const& [input, expected, features] : cases) {
        auto result = input.serialize(features);
        ASSERT_EQ(expected, result);
    }
}

TEST(osc_21, test_parse_color)
TEST(osc_21, test_parse)
TEST(osc_21, test_serialize)
}
