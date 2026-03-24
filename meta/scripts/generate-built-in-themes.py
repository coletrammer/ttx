#!/usr/bin/env python

import argparse
from pathlib import Path
from typing import Dict

parser = argparse.ArgumentParser(
    description="Generates C++ source files from the Unicode Database for the dius library",
)
parser.add_argument("-i", "--input", required=True)
parser.add_argument("-o", "--output", required=True)


def gen_color(color: str) -> str:
    color = color.removeprefix("#")
    r = int(color[0:2], 16)
    g = int(color[2:4], 16)
    b = int(color[4:6], 16)
    return f"terminal::Color({r}, {g}, {b})"


def gen_palette(palette: Dict[str, str]) -> str:
    initializers = []

    colors = []
    for i in range(16):
        colors.append(gen_color(palette[f"color{i}"]))
    initializers.append(
        gen_initializer("palette", f"di::Vector {{{ ", ".join(colors) }}}")
    )
    initializers.append(gen_initializer("foreground", gen_color(palette["foreground"])))
    initializers.append(gen_initializer("background", gen_color(palette["background"])))
    initializers.append(
        gen_initializer(
            "selection_background", gen_color(palette["selection_background"])
        )
    )
    initializers.append(
        gen_initializer(
            "selection_foreground", gen_color(palette["selection_foreground"])
        )
    )
    initializers.append(gen_initializer("cursor", gen_color(palette["cursor"])))
    initializers.append(
        gen_initializer("cursor_text", gen_color(palette["cursor_text_color"]))
    )

    return f"config_json::v1::Colors {{ {", ".join(initializers)} }}"


def gen_initializer(name: str, color: str | int) -> str:
    if isinstance(color, int):
        return f".{name} = terminal::Color(terminal::Color::Palette({color}))"
    return f".{name} = {color}"


def gen_fzf() -> str:
    initializers = []
    return f"config_json::v1::FzfColors {{ {", ".join(initializers)} }}"


def gen_status_bar() -> str:
    initializers = []
    return f"config_json::v1::StatusBarColors {{ {", ".join(initializers)} }}"


def gen_scheme(palette: Dict[str, str]) -> str:
    return f"{{ .colors = {gen_palette(palette)}, .fzf = {{ .colors = fzf_theme }}, .status_bar = {{ .colors = status_bar_theme }}, }}"


def gen(palette: Dict[str, str]) -> str:
    return f'    Entry {{ "{palette['name']}"_tsv, {gen_scheme(palette)} }},\n'


def allowed(name: str) -> bool:
    # Apparently this is not supposed to be redistributed
    if name.startswith("Monokai Pro"):
        return False
    return True


def main():
    args = parser.parse_args()

    # We're scraping the kitty themes for convience. Hopefully upstream
    # will generate JSON for us some day.
    palettes = {}
    for theme_file in Path(args.input).glob("*.conf"):
        if not theme_file.is_file():
            continue
        content = theme_file.read_text(encoding="utf-8")
        palette = {}
        for line in content.splitlines():
            if len(line) == 0 or line.startswith("#"):
                continue
            try:
                color, value = line.split(" ")
                palette[color] = value
            except:
                pass
        name = theme_file.name.removesuffix(".conf")
        palette["name"] = name.removeprefix("base24-").removeprefix("base16-")
        palettes[name] = palette

    with open(args.output, "w") as file:
        file.writelines(
            [
                '#include "config_json.h"\n',
                "\n",
                "namespace ttx::config_json::v1 {\n",
                "struct Entry {\n",
                "    di::TransparentStringView name;\n",
                "    Config theme;\n",
                "};\n",
                "\n",
                f"constexpr inline auto fzf_theme = {gen_fzf()};\n",
                f"constexpr inline auto status_bar_theme = {gen_status_bar()};\n",
                "\n",
                "static auto entries = di::Array {\n",
                *[
                    gen(palette)
                    for palette in palettes.values()
                    if allowed(palette["name"])
                ],
                "};\n",
                "\n",
                "auto build_map() -> di::TreeMap<di::TransparentString, Config> {\n"
                "    auto result = di::TreeMap<di::TransparentString, Config> {};\n",
                "    for (auto const& [name, theme] : entries) {\n"
                "        result[name] = di::clone(theme);\n"
                "    }\n",
                "    return result;\n",
                "}\n",
                "\n",
                "auto built_in_themes() -> di::TreeMap<di::TransparentString, Config> const& {\n",
                "    static auto map = build_map();\n",
                "    return map;\n",
                "}\n",
                "}\n",
            ]
        )


if __name__ == "__main__":
    main()
