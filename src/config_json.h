#pragma once

#include "config.h"
#include "di/container/path/prelude.h"
#include "di/container/vector/prelude.h"
#include "di/reflect/prelude.h"
#include "di/util/construct.h"
#include "di/vocab/optional/prelude.h"
#include "ttx/key.h"

namespace ttx::config_json::v1 {
struct Input {
    di::Optional<Key> prefix {};
    di::Optional<bool> disable_default_keybinds {};
    di::Optional<di::String> save_state_path {};

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<Input>) {
        return di::make_fields<"Input">(di::field<"prefix", &Input::prefix>,
                                        di::field<"disable_default_keybinds", &Input::disable_default_keybinds>,
                                        di::field<"save_state_path", &Input::save_state_path>);
    }
};

struct Layout {
    di::Optional<bool> hide_status_bar {};

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<Layout>) {
        return di::make_fields<"Layout">(di::field<"hide_status_bar", &Layout::hide_status_bar>);
    }
};

struct Clipboard {
    di::Optional<ClipboardMode> mode {};

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<Clipboard>) {
        return di::make_fields<"Clipboard">(di::field<"mode", &Clipboard::mode>);
    }
};

struct Session {
    di::Optional<bool> restore_layout {};
    di::Optional<bool> save_layout {};
    di::Optional<di::String> layout_name {};

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<Session>) {
        return di::make_fields<"Session">(di::field<"restore_layout", &Session::restore_layout>,
                                          di::field<"save_layout", &Session::save_layout>,
                                          di::field<"layout_name", &Session::layout_name>);
    }
};

struct Shell {
    di::Optional<di::Vector<di::String>> command {};

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<Shell>) {
        return di::make_fields<"Shell">(di::field<"command", &Shell::command>);
    }
};

struct Terminfo {
    di::Optional<di::String> term {};
    di::Optional<bool> force_local_terminfo {};

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<Terminfo>) {
        return di::make_fields<"Terminfo">(di::field<"term", &Terminfo::term>,
                                           di::field<"force_local_terminfo", &Terminfo::force_local_terminfo>);
    }
};

struct Config {
    di::String schema { "https://github.com/coletrammer/ttx/raw/refs/heads/main/meta/schema/config.json"_s };
    u32 version { 1 };
    di::Vector<di::String> extends {};
    Input input {};
    Layout layout {};
    Clipboard clipboard {};
    Session session {};
    Shell shell {};
    Terminfo terminfo {};

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<Config>) {
        return di::make_fields<"Config">(di::field<"schema", &Config::schema>, di::field<"version", &Config::version>,
                                         di::field<"extends", &Config::extends>, di::field<"input", &Config::input>,
                                         di::field<"layout", &Config::layout>,
                                         di::field<"clipboard", &Config::clipboard>,
                                         di::field<"session", &Config::session>, di::field<"shell", &Config::shell>,
                                         di::field<"terminfo", &Config::terminfo>);
    }
};

struct ToUtf8String {
    static auto operator()(di::TransparentStringView s) -> di::String {
        return s | di::transform(di::construct<c32>) | di::to<di::String>();
    }

    static auto operator()(di::PathView s) -> di::String {
        return s.data() | di::transform(di::construct<c32>) | di::to<di::String>();
    }
};

constexpr inline auto to_utf8_string = ToUtf8String {};

auto resolve_profile(di::TransparentStringView profile, Config&& base_config) -> di::Result<ttx::Config>;
}
