#include "config_json.h"
#include "di/container/string/string.h"
#include "di/test/prelude.h"
#include "dius/system/process.h"
#include "ttx/terminal/color.h"
#include "ttx/terminal/palette.h"

namespace config_json {
static void resolution() {
    auto& env = const_cast<di::TreeMap<di::TransparentString, di::TransparentString>&>(dius::system::get_environment());
    env["XDG_CONFIG_HOME"_ts] = CONFIG_FILES ""_ts;

    auto config = ttx::config_json::v1::resolve_profile("test-config"_tsv, {}, {});
    auto palette = ttx::terminal::Palette {};
    palette.set(ttx::terminal::PaletteIndex(0), ttx::terminal::Color(255, 0, 0));
    palette.set(ttx::terminal::PaletteIndex(1), ttx::terminal::Color());
    palette.set(ttx::terminal::PaletteIndex(2), ttx::terminal::Color(0xab, 0xcd, 0xef));
    palette.set(ttx::terminal::PaletteIndex::CursorText, ttx::terminal::Color(ttx::terminal::Color::Type::Dynamic));
    palette.set(ttx::terminal::PaletteIndex::Cursor, ttx::terminal::Color(0, 255, 0));
    palette.set(ttx::terminal::PaletteIndex::SelectionForeground,
                ttx::terminal::Color(ttx::terminal::Color::Type::Dynamic));
    palette.set(ttx::terminal::PaletteIndex::SelectionBackground,
                ttx::terminal::Color(ttx::terminal::Color::Type::Dynamic));
    auto expected = ttx::Config {
                          .input =
                              ttx::InputConfig {
                                  .prefix = ttx::Key::A,
                                  .disable_default_keybinds = false,
                                  .save_state_path = "/tmp/ttx-save-state.ansi"_p,
                              },
                          .theme = {
                              .name = "test"_ts,
                          },
                          .colors = palette,
                          .clipboard =
                              ttx::ClipboardConfig {
                                  .mode = ttx::ClipboardMode::Disabled,
                              },
                          .session =
                              ttx::SessionConfig {
                                  .restore_layout = false,
                                  .save_layout = false,
                                  .layout_name = "layout"_ts,
                              },
                          .shell =
                              ttx::ShellConfig {
                                  .command = di::single("zsh"_ts) | di::as_rvalue | di::to<di::Vector>(),
                              },
                          .status_bar =
                              ttx::StatusBarConfig {
                                  .hide = false,
                              },
                          .fzf = {
                              .colors =  {
                                  .fg = ttx::terminal::Color(0, 0, 0),
                              },
                          },
                          .terminfo =
                              ttx::TerminfoConfig {
                                  .term = "ttx"_ts,
                                  .force_local_terminfo = true,
                              },
                      };

    auto error = di::Optional<di::Error> {};
    if (!config) {
        error = di::move(config.error());
    }
    ASSERT_EQ(error, di::nullopt);
    di::tuple_for_each(
        [&](auto field) {
            ASSERT_EQ(field.get(config.value()), field.get(expected));
        },
        di::reflect(config.value()));
}

TEST(config_json, resolution)
}
