#pragma once

#include "di/reflect/prelude.h"
#include "ttx/clipboard.h"
#include "ttx/key.h"

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

struct LayoutConfig {
    bool hide_status_bar { false };

    auto operator==(LayoutConfig const&) const -> bool = default;

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<LayoutConfig>) {
        return di::make_fields<"LayoutConfig">(di::field<"hide_status_bar", &LayoutConfig::hide_status_bar>);
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
    bool restore_layout { false };
    bool save_layout { false };
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
            di::field<"force_local_TerminfoConfig", &TerminfoConfig::force_local_terminfo>);
    }
};

struct Config {
    InputConfig input;
    LayoutConfig layout;
    ClipboardConfig clipboard;
    SessionConfig session;
    ShellConfig shell;
    TerminfoConfig terminfo;

    auto operator==(Config const&) const -> bool = default;

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<Config>) {
        return di::make_fields<"Config">(di::field<"input", &Config::input>, di::field<"layout", &Config::layout>,
                                         di::field<"clipboard", &Config::clipboard>,
                                         di::field<"session", &Config::session>, di::field<"shell", &Config::shell>,
                                         di::field<"terminfo", &Config::terminfo>);
    }
};
}
