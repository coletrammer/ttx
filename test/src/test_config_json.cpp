#include "config_json.h"
#include "di/container/string/string.h"
#include "di/test/prelude.h"
#include "dius/system/process.h"

namespace config_json {
static void resolution() {
    auto& env = const_cast<di::TreeMap<di::TransparentString, di::TransparentString>&>(dius::system::get_environment());
    env["XDG_CONFIG_HOME"_ts] = CONFIG_FILES ""_ts;

    auto config = ttx::config_json::v1::resolve_profile("test-config"_tsv, {});
    ASSERT_EQ(config, (ttx::Config {
                          .input =
                              ttx::InputConfig {
                                  .prefix = ttx::Key::A,
                                  .disable_default_keybinds = false,
                                  .save_state_path = "/tmp/ttx-save-state.ansi"_p,
                              },
                          .layout =
                              ttx::LayoutConfig {
                                  .hide_status_bar = false,
                              },
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
                          .terminfo =
                              ttx::TerminfoConfig {
                                  .term = "ttx"_ts,
                                  .force_local_terminfo = true,
                              },
                      }));
}

TEST(config_json, resolution)
}
