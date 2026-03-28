#include "config_json.h"
#include "ttx/terminal/color.h"

namespace ttx::config_json::v1 {
// We can't simply use the data from iTerm2 Color Schemes for Catppuccin because there are way more colors
// available than present in the theme. Also catppuccin-tmux is by far the most popular tmux status bar theme
// and when using only ANSI catppuccin colors our status bar looks significantly worse.
//
// We get the color palette from https://catppuccin.com/palette/
namespace catppuccin {
    struct Ansi {
        di::Array<terminal::Color, 16> colors;
    };

    struct Palette {
        terminal::Color rosewater;
        terminal::Color flamingo;
        terminal::Color pink;
        terminal::Color mauve;
        terminal::Color red;
        terminal::Color maroon;
        terminal::Color peach;
        terminal::Color yellow;
        terminal::Color green;
        terminal::Color teal;
        terminal::Color sky;
        terminal::Color sapphire;
        terminal::Color blue;
        terminal::Color lavender;
        terminal::Color text;
        terminal::Color subtext1;
        terminal::Color subtext0;
        terminal::Color overlay2;
        terminal::Color overlay1;
        terminal::Color overlay0;
        terminal::Color surface2;
        terminal::Color surface1;
        terminal::Color surface0;
        terminal::Color base;
        terminal::Color mantle;
        terminal::Color crust;
    };

    struct Theme {
        Ansi ansi;
        Palette palette;
    };

    constexpr inline auto frappe = Theme {
        Ansi {
            di::Array {
                terminal::Color(81, 87, 109),
                terminal::Color(231, 130, 132),
                terminal::Color(166, 209, 137),
                terminal::Color(229, 200, 144),
                terminal::Color(140, 170, 238),
                terminal::Color(244, 184, 228),
                terminal::Color(129, 200, 190),
                terminal::Color(165, 173, 206),
                terminal::Color(98, 104, 128),
                terminal::Color(230, 113, 114),
                terminal::Color(142, 199, 114),
                terminal::Color(217, 186, 115),
                terminal::Color(123, 158, 240),
                terminal::Color(242, 164, 219),
                terminal::Color(90, 191, 181),
                terminal::Color(181, 191, 226),
            },
        },
        Palette {
            .rosewater = terminal::Color(242, 213, 207),
            .flamingo = terminal::Color(238, 190, 190),
            .pink = terminal::Color(244, 184, 228),
            .mauve = terminal::Color(202, 158, 230),
            .red = terminal::Color(231, 130, 132),
            .maroon = terminal::Color(234, 153, 156),
            .peach = terminal::Color(239, 159, 118),
            .yellow = terminal::Color(229, 200, 144),
            .green = terminal::Color(166, 209, 137),
            .teal = terminal::Color(129, 200, 190),
            .sky = terminal::Color(153, 209, 219),
            .sapphire = terminal::Color(133, 193, 220),
            .blue = terminal::Color(140, 170, 238),
            .lavender = terminal::Color(186, 187, 241),
            .text = terminal::Color(198, 208, 245),
            .subtext1 = terminal::Color(181, 191, 226),
            .subtext0 = terminal::Color(165, 173, 206),
            .overlay2 = terminal::Color(148, 156, 187),
            .overlay1 = terminal::Color(131, 139, 167),
            .overlay0 = terminal::Color(115, 121, 148),
            .surface2 = terminal::Color(98, 104, 128),
            .surface1 = terminal::Color(81, 87, 109),
            .surface0 = terminal::Color(65, 69, 89),
            .base = terminal::Color(48, 52, 70),
            .mantle = terminal::Color(41, 44, 60),
            .crust = terminal::Color(35, 38, 52),
        },
    };
    constexpr inline auto latte = Theme {
        Ansi {
            di::Array {
                terminal::Color(92, 95, 119),
                terminal::Color(210, 15, 57),
                terminal::Color(64, 160, 43),
                terminal::Color(223, 142, 29),
                terminal::Color(30, 102, 245),
                terminal::Color(234, 118, 203),
                terminal::Color(23, 146, 153),
                terminal::Color(172, 176, 190),
                terminal::Color(108, 111, 133),
                terminal::Color(222, 41, 62),
                terminal::Color(73, 175, 61),
                terminal::Color(238, 160, 45),
                terminal::Color(69, 110, 255),
                terminal::Color(254, 133, 216),
                terminal::Color(45, 159, 168),
                terminal::Color(188, 192, 204),
            },
        },
        Palette {
            .rosewater = terminal::Color(220, 138, 120),
            .flamingo = terminal::Color(221, 120, 120),
            .pink = terminal::Color(234, 118, 203),
            .mauve = terminal::Color(136, 57, 239),
            .red = terminal::Color(210, 15, 57),
            .maroon = terminal::Color(230, 69, 83),
            .peach = terminal::Color(254, 100, 11),
            .yellow = terminal::Color(223, 142, 29),
            .green = terminal::Color(64, 160, 43),
            .teal = terminal::Color(23, 146, 153),
            .sky = terminal::Color(4, 165, 229),
            .sapphire = terminal::Color(32, 159, 181),
            .blue = terminal::Color(30, 102, 245),
            .lavender = terminal::Color(114, 135, 253),
            .text = terminal::Color(76, 79, 105),
            .subtext1 = terminal::Color(92, 95, 119),
            .subtext0 = terminal::Color(108, 111, 133),
            .overlay2 = terminal::Color(124, 127, 147),
            .overlay1 = terminal::Color(140, 143, 161),
            .overlay0 = terminal::Color(156, 160, 176),
            .surface2 = terminal::Color(172, 176, 190),
            .surface1 = terminal::Color(188, 192, 204),
            .surface0 = terminal::Color(204, 208, 218),
            .base = terminal::Color(239, 241, 245),
            .mantle = terminal::Color(230, 233, 239),
            .crust = terminal::Color(220, 224, 232),
        },
    };
    constexpr inline auto macchiato = Theme {
        Ansi {
            di::Array {
                terminal::Color(73, 77, 100),
                terminal::Color(237, 135, 150),
                terminal::Color(166, 218, 149),
                terminal::Color(238, 212, 159),
                terminal::Color(138, 173, 244),
                terminal::Color(245, 189, 230),
                terminal::Color(139, 213, 202),
                terminal::Color(165, 173, 203),
                terminal::Color(91, 96, 120),
                terminal::Color(236, 116, 134),
                terminal::Color(140, 207, 127),
                terminal::Color(225, 198, 130),
                terminal::Color(120, 161, 246),
                terminal::Color(242, 169, 221),
                terminal::Color(99, 203, 192),
                terminal::Color(184, 192, 224),
            },
        },
        Palette {
            .rosewater = terminal::Color(244, 219, 214),
            .flamingo = terminal::Color(240, 198, 198),
            .pink = terminal::Color(245, 189, 230),
            .mauve = terminal::Color(198, 160, 246),
            .red = terminal::Color(237, 135, 150),
            .maroon = terminal::Color(238, 153, 160),
            .peach = terminal::Color(245, 169, 127),
            .yellow = terminal::Color(238, 212, 159),
            .green = terminal::Color(166, 218, 149),
            .teal = terminal::Color(139, 213, 202),
            .sky = terminal::Color(145, 215, 227),
            .sapphire = terminal::Color(125, 196, 228),
            .blue = terminal::Color(138, 173, 244),
            .lavender = terminal::Color(183, 189, 248),
            .text = terminal::Color(202, 211, 245),
            .subtext1 = terminal::Color(184, 192, 224),
            .subtext0 = terminal::Color(165, 173, 203),
            .overlay2 = terminal::Color(147, 154, 183),
            .overlay1 = terminal::Color(128, 135, 162),
            .overlay0 = terminal::Color(110, 115, 141),
            .surface2 = terminal::Color(91, 96, 120),
            .surface1 = terminal::Color(73, 77, 100),
            .surface0 = terminal::Color(54, 58, 79),
            .base = terminal::Color(36, 39, 58),
            .mantle = terminal::Color(30, 32, 48),
            .crust = terminal::Color(24, 25, 38),
        },
    };
    constexpr inline auto mocha = Theme {
        Ansi {
            di::Array {
                terminal::Color(69, 71, 90),
                terminal::Color(243, 139, 168),
                terminal::Color(166, 227, 161),
                terminal::Color(249, 226, 175),
                terminal::Color(137, 180, 250),
                terminal::Color(245, 194, 231),
                terminal::Color(148, 226, 213),
                terminal::Color(166, 173, 200),
                terminal::Color(88, 91, 112),
                terminal::Color(243, 119, 153),
                terminal::Color(137, 216, 139),
                terminal::Color(235, 211, 145),
                terminal::Color(116, 168, 252),
                terminal::Color(242, 174, 222),
                terminal::Color(107, 215, 202),
                terminal::Color(186, 194, 222),
            },
        },
        Palette {
            .rosewater = terminal::Color(245, 224, 220),
            .flamingo = terminal::Color(242, 205, 205),
            .pink = terminal::Color(245, 194, 231),
            .mauve = terminal::Color(203, 166, 247),
            .red = terminal::Color(243, 139, 168),
            .maroon = terminal::Color(235, 160, 172),
            .peach = terminal::Color(250, 179, 135),
            .yellow = terminal::Color(249, 226, 175),
            .green = terminal::Color(166, 227, 161),
            .teal = terminal::Color(148, 226, 213),
            .sky = terminal::Color(137, 220, 235),
            .sapphire = terminal::Color(116, 199, 236),
            .blue = terminal::Color(137, 180, 250),
            .lavender = terminal::Color(180, 190, 254),
            .text = terminal::Color(205, 214, 244),
            .subtext1 = terminal::Color(186, 194, 222),
            .subtext0 = terminal::Color(166, 173, 200),
            .overlay2 = terminal::Color(147, 153, 178),
            .overlay1 = terminal::Color(127, 132, 156),
            .overlay0 = terminal::Color(108, 112, 134),
            .surface2 = terminal::Color(88, 91, 112),
            .surface1 = terminal::Color(69, 71, 90),
            .surface0 = terminal::Color(49, 50, 68),
            .base = terminal::Color(30, 30, 46),
            .mantle = terminal::Color(24, 24, 37),
            .crust = terminal::Color(17, 17, 27),
        },
    };

    static auto build(Theme const& theme) -> Config {
        auto result = Config {};

        // Palette
        {
            auto& config_palette = result.colors.palette.emplace();
            for (auto [i, c] : theme.ansi.colors | di::enumerate) {
                config_palette.push_back(c);
            }
            result.colors.background = theme.palette.base;
            result.colors.selection_foreground = theme.palette.base;
            result.colors.cursor = theme.palette.rosewater;
            result.colors.cursor_text = theme.palette.base;
            result.colors.foreground = theme.palette.text;
            result.colors.selection_background = theme.palette.text;
        }

        // Status bar
        {
            auto& output = result.status_bar.colors;
            output.background_color = theme.palette.crust;
            output.label_background_color = theme.palette.surface0;
            output.label_text_color = theme.palette.text;
        }

        // Fzf
        {
            auto& output = result.fzf.colors;
            output.bg = theme.palette.base;
            output.bgplus = theme.palette.surface0;
            output.fg = theme.palette.text;
            output.fgplus = theme.palette.text;
            output.info = theme.palette.mauve;
            output.marker = theme.palette.mauve;
            output.pointer = theme.palette.rosewater;
            output.preview_border = theme.palette.overlay0;
            output.prompt = theme.palette.mauve;
            output.selected_bg = theme.palette.surface1;
            output.separator = theme.palette.overlay0;
            output.spinner = theme.palette.rosewater;
        }

        return result;
    }
}

struct Entry {
    di::TransparentStringView name;
    Config theme;
};

static auto const entries = di::Array {
    Entry { "Catppuccin Frappe"_tsv, catppuccin::build(catppuccin::frappe) },
    Entry { "Catppuccin Latte"_tsv, catppuccin::build(catppuccin::latte) },
    Entry { "Catppuccin Macchiato"_tsv, catppuccin::build(catppuccin::macchiato) },
    Entry { "Catppuccin Mocha"_tsv, catppuccin::build(catppuccin::mocha) },
};

static auto build_map() -> di::TreeMap<di::TransparentString, Config> {
    auto result = di::TreeMap<di::TransparentString, Config> {};
    for (auto const& [name, theme] : entries) {
        result[name] = di::clone(theme);
    }
    return result;
}

auto built_in_themes() -> di::TreeMap<di::TransparentString, config_json::v1::Config> const& {
    static auto map = build_map();
    return map;
}
}
