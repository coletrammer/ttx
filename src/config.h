#pragma once

#include "colors.h"
#include "di/reflect/prelude.h"
#include "tab_name.h"
#include "ttx/clipboard.h"
#include "ttx/key.h"
#include "ttx/terminal/palette.h"

namespace ttx {
struct InputConfig {
    Key prefix { Key::B };
    bool disable_default_keybinds { false };
    di::Path save_state_path { "/tmp/ttx-save-state.ansi"_p };

    auto operator==(InputConfig const&) const -> bool = default;

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<InputConfig>) {
        return di::make_fields<"InputConfig">(
            di::field<"prefix", &InputConfig::prefix>,
            di::field<"disable_default_keybinds", &InputConfig::disable_default_keybinds>,
            di::field<"save_state_path", &InputConfig::save_state_path>);
    }
};

struct ClipboardConfig {
    ClipboardMode mode { ClipboardMode::System };

    auto operator==(ClipboardConfig const&) const -> bool = default;

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<ClipboardConfig>) {
        return di::make_fields<"ClipboardConfig">(di::field<"mode", &ClipboardConfig::mode>);
    }
};

struct SessionConfig {
    bool restore_layout { true };
    bool save_layout { true };
    di::TransparentString layout_name;

    auto operator==(SessionConfig const&) const -> bool = default;

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<SessionConfig>) {
        return di::make_fields<"SessionConfig">(di::field<"restore_layout", &SessionConfig::restore_layout>,
                                                di::field<"save_layout", &SessionConfig::save_layout>,
                                                di::field<"layout_name", &SessionConfig::layout_name>);
    }
};

struct ShellConfig {
    di::Vector<di::TransparentString> command {};

    auto operator==(ShellConfig const&) const -> bool = default;

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<ShellConfig>) {
        return di::make_fields<"ShellConfig">(di::field<"command", &ShellConfig::command>);
    }
};

struct TerminfoConfig {
    di::TransparentString term { "xterm-ttx"_ts };
    bool force_local_terminfo { false };

    auto operator==(TerminfoConfig const&) const -> bool = default;

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<TerminfoConfig>) {
        return di::make_fields<"TerminfoConfig">(
            di::field<"term", &TerminfoConfig::term>,
            di::field<"force_local_terminfo", &TerminfoConfig::force_local_terminfo>);
    }
};

struct ThemeConfig {
    di::TransparentString name { "auto"_ts };
    di::TransparentString dark {};
    di::TransparentString light {};

    auto operator==(ThemeConfig const&) const -> bool = default;

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<ThemeConfig>) {
        return di::make_fields<"ThemeConfig">(di::field<"name", &ThemeConfig::name>,
                                              di::field<"dark", &ThemeConfig::dark>,
                                              di::field<"light", &ThemeConfig::light>);
    }
};

struct FzfConfig {
    FzfColors colors {};

    auto operator==(FzfConfig const&) const -> bool = default;

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<FzfConfig>) {
        return di::make_fields<"FzfConfig">(di::field<"colors", &FzfConfig::colors>);
    }
};

enum class StatusBarPosition {
    Top,
    Bottom,
};

constexpr static auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<StatusBarPosition>) {
    using enum StatusBarPosition;
    return di::make_enumerators<"StatusBarPosition">(
        di::enumerator<"top", Top, "Show the status bar at the top of the screen">,
        di::enumerator<"bottom", Bottom, "Show the status bar at the botton of the screen">);
}

struct StatusBarConfig {
    bool hide { false };
    StatusBarPosition position { StatusBarPosition::Top };
    di::Vector<TabNameSource> tab_name_sources { TabNameSource::Manual, TabNameSource::WindowTitle,
                                                 TabNameSource::CurrentWorkingDirectory };
    StatusBarColors colors {};

    auto operator==(StatusBarConfig const&) const -> bool = default;

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<StatusBarConfig>) {
        return di::make_fields<"StatusBarConfig">(di::field<"hide", &StatusBarConfig::hide>,
                                                  di::field<"position", &StatusBarConfig::position>,
                                                  di::field<"tab_name_sources", &StatusBarConfig::tab_name_sources>,
                                                  di::field<"colors", &StatusBarConfig::colors>);
    }
};

struct RenderConfig {
    u32 inactive_dim_factor { 0 };
    u32 popup_dim_factor { 20 };

    auto operator==(RenderConfig const&) const -> bool = default;

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<RenderConfig>) {
        return di::make_fields<"RenderConfig">(di::field<"inactive_dim_factor", &RenderConfig::inactive_dim_factor>,
                                               di::field<"popup_dim_factor", &RenderConfig::popup_dim_factor>);
    }
};

struct Config {
    InputConfig input {};
    ThemeConfig theme {};
    terminal::Palette colors { terminal::Palette::default_global() };
    ClipboardConfig clipboard {};
    SessionConfig session {};
    ShellConfig shell {};
    StatusBarConfig status_bar {};
    FzfConfig fzf {};
    RenderConfig render {};
    TerminfoConfig terminfo {};

    auto operator==(Config const&) const -> bool = default;

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<Config>) {
        return di::make_fields<"Config">(
            di::field<"input", &Config::input>, di::field<"theme", &Config::theme>,
            di::field<"colors", &Config::colors>, di::field<"clipboard", &Config::clipboard>,
            di::field<"session", &Config::session>, di::field<"shell", &Config::shell>,
            di::field<"terminfo", &Config::terminfo>, di::field<"fzf", &Config::fzf>,
            di::field<"render", &Config::render>, di::field<"status_bar", &Config::status_bar>);
    }
};
}
