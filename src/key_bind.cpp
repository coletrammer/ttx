#include "key_bind.h"

#include "actions.h"
#include "input_mode.h"
#include "tab.h"
#include "ttx/modifiers.h"

namespace ttx {
static auto make_switch_tab_binds(di::Vector<KeyBind>& result) {
    for (auto i : di::range(9zu)) {
        auto key = Key(di::to_underlying(Key::_1) + i);
        result.push_back({
            .key = key,
            .mode = InputMode::Normal,
            .action = switch_tab(i + 1),
        });
    }
    result.push_back({
        .key = Key::P,
        .mode = InputMode::Normal,
        .action = switch_prev_tab(),
    });
    result.push_back({
        .key = Key::N,
        .mode = InputMode::Normal,
        .action = switch_next_tab(),
    });
}

static auto make_navigate_binds(di::Vector<KeyBind>& result, InputMode mode, InputMode next_mode) {
    auto keys = di::Array {
        di::Tuple { Key::J, NavigateDirection::Down },
        di::Tuple { Key::K, NavigateDirection::Up },
        di::Tuple { Key::L, NavigateDirection::Right },
        di::Tuple { Key::H, NavigateDirection::Left },
    };
    for (auto [key, direction] : keys) {
        result.push_back({
            .key = key,
            .modifiers = Modifiers::Control,
            .mode = mode,
            .next_mode = next_mode,
            .action = navigate(direction),
        });
    }
}

static auto make_resize_binds(di::Vector<KeyBind>& result, InputMode mode) {
    auto keys = di::Array {
        di::Tuple { Key::J, ResizeDirection::Bottom },
        di::Tuple { Key::K, ResizeDirection::Top },
        di::Tuple { Key::L, ResizeDirection::Right },
        di::Tuple { Key::H, ResizeDirection::Left },
    };
    for (auto [key, direction] : keys) {
        result.push_back({
            .key = key,
            .mode = mode,
            .next_mode = InputMode::Resize,
            .action = resize(direction, 2),
        });
        result.push_back({
            .key = key,
            .modifiers = Modifiers::Shift,
            .mode = mode,
            .next_mode = InputMode::Resize,
            .action = resize(direction, -2),
        });
    }
}

static auto make_replay_key_binds() -> di::Vector<KeyBind> {
    auto result = di::Vector<KeyBind> {};

    // Insert mode
    result.push_back({
        .key = Key::Q,
        .mode = InputMode::Insert,
        .action = quit(),
    });
    result.push_back({
        .key = Key::C,
        .modifiers = Modifiers::Control,
        .mode = InputMode::Insert,
        .action = quit(),
    });
    result.push_back({
        .key = Key::J,
        .mode = InputMode::Insert,
        .action = scroll(Direction::Vertical, 1),
    });
    result.push_back({
        .key = Key::K,
        .mode = InputMode::Insert,
        .action = scroll(Direction::Vertical, -1),
    });
    result.push_back({
        .key = Key::L,
        .mode = InputMode::Insert,
        .action = scroll(Direction::Horizontal, 1),
    });
    result.push_back({
        .key = Key::H,
        .mode = InputMode::Insert,
        .action = scroll(Direction::Horizontal, -1),
    });
    result.push_back({
        .key = Key::Z,
        .mode = InputMode::Insert,
        .action = toggle_full_screen_pane(),
    });
    make_navigate_binds(result, InputMode::Insert, InputMode::Insert);

    return result;
}

auto make_key_binds(Key prefix, di::Path save_state_path, bool replay_mode) -> di::Vector<KeyBind> {
    if (replay_mode) {
        return make_replay_key_binds();
    }

    auto result = di::Vector<KeyBind> {};

    // Insert mode
    {
        result.push_back({
            .key = prefix,
            .modifiers = Modifiers::Control,
            .mode = InputMode::Insert,
            .next_mode = InputMode::Normal,
            .action = enter_normal_mode(),
        });
        result.push_back({
            .key = Key::None,
            .mode = InputMode::Insert,
            .action = send_to_pane(),
        });
    }

    // Normal Mode
    {
        result.push_back({
            .key = prefix,
            .modifiers = Modifiers::Control,
            .mode = InputMode::Normal,
            .action = send_to_pane(),
        });
        make_resize_binds(result, InputMode::Normal);
        make_navigate_binds(result, InputMode::Normal, InputMode::Insert);
        result.push_back({
            .key = Key::C,
            .mode = InputMode::Normal,
            .action = create_tab(),
        });
        make_switch_tab_binds(result);
        result.push_back({
            .key = Key::D,
            .mode = InputMode::Normal,
            .action = quit(),
        });
        result.push_back({
            .key = Key::S,
            .modifiers = Modifiers::Shift,
            .mode = InputMode::Normal,
            .action = save_layout(),
        });
        result.push_back({
            .key = Key::F,
            .mode = InputMode::Normal,
            .action = find_tab(),
        });
        result.push_back({
            .key = Key::Comma,
            .mode = InputMode::Normal,
            .action = rename_tab(),
        });
        result.push_back({
            .key = Key::R,
            .modifiers = Modifiers::Shift,
            .mode = InputMode::Normal,
            .action = hard_reset(),
        });
        result.push_back({
            .key = Key::I,
            .modifiers = Modifiers::Shift,
            .mode = InputMode::Normal,
            .action = stop_capture(),
        });
        result.push_back({
            .key = Key::S,
            .modifiers = Modifiers::Shift,
            .mode = InputMode::Normal,
            .action = save_state(di::move(save_state_path)),
        });
        result.push_back({
            .key = Key::X,
            .mode = InputMode::Normal,
            .action = exit_pane(),
        });
        result.push_back({
            .key = Key::Z,
            .mode = InputMode::Normal,
            .action = toggle_full_screen_pane(),
        });
        result.push_back({
            .key = Key::C,
            .modifiers = Modifiers::Shift,
            .mode = InputMode::Normal,
            .action = create_session(),
        });
        result.push_back({
            .key = Key::_4,
            .modifiers = Modifiers::Shift,
            .mode = InputMode::Normal,
            .action = rename_session(),
        });
        result.push_back({
            .key = Key::C,
            .modifiers = Modifiers::Shift,
            .mode = InputMode::Normal,
            .action = create_session(),
        });
        result.push_back({
            .key = Key::F,
            .modifiers = Modifiers::Shift,
            .mode = InputMode::Normal,
            .action = find_session(),
        });
        result.push_back({
            .key = Key::_9,
            .modifiers = Modifiers::Shift,
            .mode = InputMode::Normal,
            .action = switch_prev_session(),
        });
        result.push_back({
            .key = Key::_0,
            .modifiers = Modifiers::Shift,
            .mode = InputMode::Normal,
            .action = switch_next_session(),
        });
        result.push_back({
            .key = Key::BackSlash,
            .modifiers = Modifiers::Shift,
            .mode = InputMode::Normal,
            .action = add_pane(Direction::Horizontal),
        });
        result.push_back({
            .key = Key::Minus,
            .mode = InputMode::Normal,
            .action = add_pane(Direction::Vertical),
        });
        result.push_back({
            .key = Key::LeftBracket,
            .mode = InputMode::Normal,
            .action = scroll_prev_command(),
        });
        result.push_back({
            .key = Key::RightBracket,
            .mode = InputMode::Normal,
            .action = scroll_next_command(),
        });
        result.push_back({
            .key = Key::None,
            .mode = InputMode::Normal,
            .action = reset_mode(),
        });
    }

    // Switch Mode
    {
        result.push_back({
            .key = prefix,
            .modifiers = Modifiers::Control,
            .mode = InputMode::Switch,
            .next_mode = InputMode::Normal,
            .action = enter_normal_mode(),
        });
        make_navigate_binds(result, InputMode::Switch, InputMode::Switch);
        result.push_back({
            .key = Key::None,
            .mode = InputMode::Switch,
            .action = reset_mode(),
        });
    }

    // Resize Mode
    {
        result.push_back({
            .key = prefix,
            .modifiers = Modifiers::Control,
            .mode = InputMode::Resize,
            .next_mode = InputMode::Normal,
            .action = enter_normal_mode(),
        });
        make_resize_binds(result, InputMode::Resize);
        make_navigate_binds(result, InputMode::Resize, InputMode::Resize);
        result.push_back({
            .key = Key::None,
            .mode = InputMode::Resize,
            .action = reset_mode(),
        });
    }

    return result;
}
}
