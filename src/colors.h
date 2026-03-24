#pragma once

#include "di/reflect/reflect.h"
#include "ttx/terminal/color.h"

namespace ttx {
struct FzfColors {
    terminal::Color fg {};
    terminal::Color fgplus { terminal::Color(terminal::Color::Palette::White) };
    terminal::Color bg {};
    terminal::Color bgplus { terminal::Color(terminal::Color::Palette::DarkGrey) };
    terminal::Color border { terminal::Color(terminal::Color::Palette::Blue) };
    terminal::Color header { terminal::Color(terminal::Color::Palette::Red) };
    terminal::Color hl { terminal::Color(terminal::Color::Palette::Blue) };
    terminal::Color hlplus { terminal::Color(terminal::Color::Palette::Blue) };
    terminal::Color info { terminal::Color(terminal::Color::Palette::Magenta) };
    terminal::Color label { terminal::Color(terminal::Color::Palette::Blue) };
    terminal::Color marker { terminal::Color(terminal::Color::Palette::White) };
    terminal::Color pointer { terminal::Color(terminal::Color::Palette::Yellow) };
    terminal::Color preview_border { terminal::Color(terminal::Color::Palette::LightGrey) };
    terminal::Color prompt { terminal::Color(terminal::Color::Palette::Magenta) };
    terminal::Color selected_bg { terminal::Color(terminal::Color::Palette::DarkGrey) };
    terminal::Color separator { terminal::Color(terminal::Color::Palette::LightGrey) };
    terminal::Color spinner { terminal::Color(terminal::Color::Palette::Yellow) };

    auto operator==(FzfColors const&) const -> bool = default;

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<FzfColors>) {
        return di::make_fields<"FzfColors">(
            di::field<"fg", &FzfColors::fg>, di::field<"fg+", &FzfColors::fgplus>, di::field<"bg", &FzfColors::bg>,
            di::field<"bg+", &FzfColors::bgplus>, di::field<"border", &FzfColors::border>,
            di::field<"header", &FzfColors::header>, di::field<"hl", &FzfColors::hl>,
            di::field<"hl+", &FzfColors::hlplus>, di::field<"info", &FzfColors::info>,
            di::field<"label", &FzfColors::label>, di::field<"marker", &FzfColors::marker>,
            di::field<"pointer", &FzfColors::pointer>, di::field<"preview-border", &FzfColors::preview_border>,
            di::field<"prompt", &FzfColors::prompt>, di::field<"selected-bg", &FzfColors::selected_bg>,
            di::field<"separator", &FzfColors::separator>, di::field<"spinner", &FzfColors::spinner>);
    }
};

struct StatusBarColors {
    terminal::Color badge_text_color { terminal::Color(terminal::Color::Type::Dynamic) };
    terminal::Color label_text_color { terminal::Color(terminal::Color::Palette::White) };
    terminal::Color background_color { terminal::Color(terminal::Color::Palette::Black) };
    terminal::Color label_background_color { terminal::Color(terminal::Color::Palette::DarkGrey) };

    terminal::Color active_tab_badge_color { terminal::Color(terminal::Color::Palette::Yellow) };
    terminal::Color inactive_tab_badge_color { terminal::Color(terminal::Color::Palette::Blue) };
    terminal::Color session_badge_background_color { terminal::Color(terminal::Color::Palette::Green) };
    terminal::Color host_badge_background_color { terminal::Color(terminal::Color::Palette::Magenta) };

    terminal::Color switch_mode_color { terminal::Color(terminal::Color::Palette::Yellow) };
    terminal::Color insert_mode_color { terminal::Color(terminal::Color::Palette::Blue) };
    terminal::Color normal_mode_color { terminal::Color(terminal::Color::Palette::Green) };
    terminal::Color resize_mode_color { terminal::Color(terminal::Color::Palette::Red) };

    auto operator==(StatusBarColors const&) const -> bool = default;

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<StatusBarColors>) {
        return di::make_fields<"StatusBarColors">(
            di::field<"badge_text_color", &StatusBarColors::badge_text_color>,
            di::field<"label_text_color", &StatusBarColors::label_text_color>,
            di::field<"background_color", &StatusBarColors::background_color>,
            di::field<"label_background_color", &StatusBarColors::label_background_color>,
            di::field<"active_tab_badge_color", &StatusBarColors::active_tab_badge_color>,
            di::field<"inactive_tab_badge_color", &StatusBarColors::inactive_tab_badge_color>,
            di::field<"session_badge_background_color", &StatusBarColors::session_badge_background_color>,
            di::field<"host_badge_background_color", &StatusBarColors::host_badge_background_color>,
            di::field<"switch_mode_color", &StatusBarColors::switch_mode_color>,
            di::field<"insert_mode_color", &StatusBarColors::insert_mode_color>,
            di::field<"normal_mode_color", &StatusBarColors::normal_mode_color>,
            di::field<"resize_mode_color", &StatusBarColors::resize_mode_color>);
    }
};
}
