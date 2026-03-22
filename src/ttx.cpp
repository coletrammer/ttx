#include "config.h"
#include "config_json.h"
#include "di/cli/parser.h"
#include "di/container/string/conversion.h"
#include "di/container/string/string_view.h"
#include "di/io/writer_print.h"
#include "di/sync/synchronized.h"
#include "di/vocab/error/string_error.h"
#include "dius/filesystem/operations.h"
#include "dius/main.h"
#include "dius/sync_file.h"
#include "dius/system/process.h"
#include "input.h"
#include "layout_state.h"
#include "render.h"
#include "save_layout.h"
#include "ttx/features.h"
#include "ttx/terminal/capability.h"

namespace ttx {
struct NewBase {
    di::Optional<Key> prefix;
    bool hide_status_bar { false };
    bool headless { false };
    di::Optional<di::PathView> save_state_path;
    di::Optional<di::PathView> capture_command_output_path;

    // New
    bool disable_layout_restore { false };
    bool disable_layout_save { false };
    di::Optional<di::TransparentStringView> layout_name;
    di::Optional<di::TransparentStringView> term;
    di::Optional<ClipboardMode> clipboard_mode;
    bool force_local_terminfo { false };
    di::Vector<di::TransparentStringView> command;
    di::TransparentStringView profile { "main"_tsv };

    // Replay
    di::Vector<di::PathView> replay_paths;
};

struct New : NewBase {
    bool help { false };

    constexpr static auto get_cli_parser() {
        return di::cli_parser<New>("new"_tsv, "Start a new ttx instance"_sv)
            .option<&NewBase::prefix>({}, "prefix"_tsv, "Override config prefix key for key bindings"_sv, false,
                                      "KEY"_sv)
            .option<&NewBase::disable_layout_restore>({}, "disable-layout-restore"_tsv,
                                                      "Disable restoring from saved layout"_sv)
            .option<&NewBase::disable_layout_save>({}, "disable-layout-restore"_tsv,
                                                   "Disable continuously saving the current layout"_sv)
            .option<&NewBase::layout_name>({}, "layout-name"_tsv,
                                           "Layout name for save/restore (default: profile name)"_sv)
            .option<&NewBase::profile>('p', "profile"_tsv,
                                       "Profile name to use for this session (empty string loads no configuration)"_sv)
            .option<&NewBase::hide_status_bar>('s', "hide-status-bar"_tsv, "Hide the status bar"_sv)
            .option<&NewBase::capture_command_output_path>('c', "capture-command-output-path"_tsv,
                                                           "Capture command output to a file"_sv)
            .option<&NewBase::save_state_path>('S', "save-state-path"_tsv,
                                               "Save state path when triggering saving a pane's state"_sv)
            .option<&NewBase::headless>('h', "headless"_tsv, "Headless mode"_sv)
            .option<&NewBase::term>('t', "term"_tsv, "Override config TERM environment variable"_sv)
            .option<&NewBase::clipboard_mode>({}, "clipboard"_tsv, "Set the clipboard mode"_sv, false, "MODE"_sv)
            .option<&NewBase::force_local_terminfo>(
                {}, "force-local-terminfo"_tsv,
                "Always try and compile built-in terminfo, and set TERMINFO env variable"_sv)
            .argument<&NewBase::command>("COMMAND"_sv, "Program to run in terminal (default: $SHELL)"_sv, false,
                                         di::cli::ValueType::CommandWithArgs)
            .help();
    }
};

struct Replay : NewBase {
    bool help { false };

    constexpr static auto get_cli_parser() {
        return di::cli_parser<Replay>("replay"_tsv, "Start a Replay ttx instance"_sv)
            .option<&NewBase::prefix>('p', "prefix"_tsv, "Prefix key for key bindings"_sv, false, "KEY"_sv)
            .option<&NewBase::capture_command_output_path>('c', "capture-command-output-path"_tsv,
                                                           "Capture command output to a file"_sv)
            .option<&NewBase::save_state_path>('S', "save-state-path"_tsv,
                                               "Save state path when triggering saving a pane's state"_sv)
            .option<&NewBase::headless>('h', "headless"_tsv, "Headless mode"_sv)
            .argument<&NewBase::replay_paths>("REPLAY_FILES"_sv, "Files to replay (each file gets its own pane)"_sv,
                                              true)
            .help();
    }
};

struct Keybinds {
    di::TransparentStringView profile { "main"_tsv };
    bool replay_mode { false };
    bool help { false };

    constexpr static auto get_cli_parser() {
        return di::cli_parser<Keybinds>("keybinds"_tsv, "List active keybinds for ttx"_sv)
            .option<&Keybinds::profile>('p', "profile"_tsv,
                                        "Profile name to use for this session (empty string loads no configuration)"_sv)
            .option<&Keybinds::profile>({}, "replay-mode"_tsv,
                                        "Show keybinds assuming ttx is replaying an captured input file"_sv)
            .help();
    }
};

struct Completions {
    di::cli::Shell shell { di::cli::Shell::Bash };
    bool help { false };

    constexpr static auto get_cli_parser() {
        return di::cli_parser<Completions>("completions"_tsv, "Write shell completions for ttx to stdout"_sv)
            .argument<&Completions::shell>("SHELL"_sv, "Shell to generate completions for"_sv, true)
            .help();
    }
};

struct Features {
    bool help { false };

    constexpr static auto get_cli_parser() {
        return di::cli_parser<Features>("features"_tsv, "List features detected in the current terminal"_sv).help();
    }
};

struct ConfigShow {
    di::TransparentStringView profile { "main"_tsv };
    bool help { false };

    constexpr static auto get_cli_parser() {
        return di::cli_parser<ConfigShow>(
                   "show"_tsv, "Print the resolved configuration for the profile as a valid JSON configuration file"_sv)
            .option<&ConfigShow::profile>(
                'p', "profile"_tsv, "Profile name to use for this session (empty string loads no configuration)"_sv)
            .help();
    }
};

struct ConfigNix {
    bool help { false };

    constexpr static auto get_cli_parser() {
        return di::cli_parser<ConfigNix>("nix"_tsv, "Print the nix option schema for ttx configuration"_sv).help();
    }
};

struct ConfigSchema {
    bool help { false };

    constexpr static auto get_cli_parser() {
        return di::cli_parser<ConfigSchema>("schema"_tsv, "Print the JSON schema for ttx configuration"_sv).help();
    }
};

struct ConfigCommand {
    di::Variant<ConfigShow, ConfigNix, ConfigSchema> subcommand;
    bool help { false };

    constexpr static auto get_cli_parser() {
        return di::cli_parser<ConfigCommand>("config"_tsv, "Show information about ttx's configuration"_sv)
            .subcommands<&ConfigCommand::subcommand>()
            .help();
    }
};

enum class TerminfoFormat {
    Terminfo,
    Verbose,
};

constexpr static auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<TerminfoFormat>) {
    using enum TerminfoFormat;
    return di::make_enumerators<"TerminfoFormat">(
        di::enumerator<"terminfo", Terminfo, "Official terminfo format used by tools like tic">,
        di::enumerator<"verbose", Verbose,
                       "Verbose format with descriptions to understand the details of ttx's terminfo">);
}

struct Terminfo {
    TerminfoFormat format { TerminfoFormat::Terminfo };
    bool help { false };

    constexpr static auto get_cli_parser() {
        return di::cli_parser<Terminfo>("terminfo"_tsv, "Display the terminfo for ttx"_sv)
            .option<&Terminfo::format>('f', "format"_tsv, "Display format for the terminfo"_sv)
            .help();
    }
};

struct Args {
    di::Variant<New, ConfigCommand, Completions, Replay, Keybinds, Features, Terminfo> subcommand;
    bool help { false };

    constexpr static auto get_cli_parser() {
        return di::cli_parser<Args>("ttx"_tsv, "Terminal multiplexer"_sv).subcommands<&Args::subcommand>().help();
    }
};

static auto get_session_save_dir() -> di::Result<di::Path> {
    auto const& env = dius::system::get_environment();
    auto data_home = env.at("XDG_DATA_HOME"_tsv)
                         .transform([&](di::TransparentStringView path) {
                             return di::PathView(path).to_owned();
                         })
                         .or_else([&] {
                             return env.at("HOME"_tsv).transform([&](di::TransparentStringView home) {
                                 return di::PathView(home).to_owned() / ".local"_tsv / "share"_tsv;
                             });
                         });
    if (!data_home) {
        return di::Unexpected(di::BasicError::NoSuchFileOrDirectory);
    }

    auto& result = data_home.value();
    result /= "ttx"_tsv;
    result /= "layouts"_tsv;
    return di::move(result);
}

static auto get_local_terminfo_dir() -> di::Result<di::Path> {
    auto const& env = dius::system::get_environment();
    auto data_home = env.at("XDG_STATE_HOME"_tsv)
                         .transform([&](di::TransparentStringView path) {
                             return di::PathView(path).to_owned();
                         })
                         .or_else([&] {
                             return env.at("HOME"_tsv).transform([&](di::TransparentStringView home) {
                                 return di::PathView(home).to_owned() / ".local"_tsv / "state"_tsv;
                             });
                         });
    if (!data_home) {
        return di::Unexpected(di::BasicError::NoSuchFileOrDirectory);
    }

    auto& result = data_home.value();
    result /= "ttx"_tsv;
    result /= "terminfo"_tsv;
    return di::move(result);
}

static auto maybe_get_terminfo_dir(di::TransparentStringView term, bool force_local_terminfo)
    -> di::Result<di::Optional<di::Path>> {
    // If the user is overriding TERM, don't setup our terminfo.
    if (term != "ttx"_tsv && term != "xterm-ttx"_tsv) {
        return {};
    }

    if (!force_local_terminfo) {
        // First, start by searching for an existing terminfo for ttx. We could try and implement
        // this check ourselves, but its probably better to rely on the actual curses implementation.
        // This does slow down start-up time, but we can rework this logic later on. For now, this
        // is very convenient.
        auto null = TRY(dius::open_sync("/dev/null"_pv, dius::OpenMode::ReadWrite));
        auto process_result = TRY(dius::system::Process(di::Array {
                                                            "tput"_ts,
                                                            "-T"_ts,
                                                            "xterm-ttx"_ts,
                                                            "colors"_ts,
                                                        } |
                                                        di::to<di::Vector>())
                                      .with_file_dup(null.file_descriptor(), 1)
                                      .with_file_dup(null.file_descriptor(), 2)
                                      .spawn_and_wait());
        if (process_result.exited() && process_result.exit_code() == 0) {
            return {};
        }
    }

    // In this case, we're going to compile our terminfo ourselves and then return the
    // PATH to it. We will store the data in $XDG_STATE_HOME/ttx/terminfo.
    auto terminfo_dir = TRY(get_local_terminfo_dir());
    TRY(dius::filesystem::create_directories(terminfo_dir));

    // To avoid redundant recompilations, hash our serialized terminfo and see if we're already written
    // it out.
    auto const& terminfo = terminal::get_ttx_terminfo();
    auto serialized_terminfo = terminfo.serialize();
    auto terminfo_hash = di::hash(serialized_terminfo);
    if (auto result = dius::read_to_string(terminfo_dir.clone() / "ttx.terminfo.hash"_pv)) {
        if (result.value() == di::to_string(terminfo_hash)) {
            return terminfo_dir;
        }
    }

    auto terminfo_file = TRY(dius::open_sync(terminfo_dir.clone() / "ttx.terminfo"_pv, dius::OpenMode::WriteClobber));
    di::writer_print<di::String::Encoding>(terminfo_file, "{}"_sv, serialized_terminfo);

    auto null = TRY(dius::open_sync("/dev/null"_pv, dius::OpenMode::ReadWrite));
    auto process_result = TRY(dius::system::Process(di::Array {
                                                        "tic"_ts,
                                                        "-x"_ts,
                                                        "-o"_ts,
                                                        terminfo_dir.data().to_owned(),
                                                        (terminfo_dir.clone() / "ttx.terminfo"_pv).data().to_owned(),
                                                    } |
                                                    di::to<di::Vector>())
                                  .with_file_dup(null.file_descriptor(), 1)
                                  .with_file_dup(null.file_descriptor(), 2)
                                  .spawn_and_wait());
    if (!process_result.exited() || process_result.exit_code() != 0) {
        return di::Unexpected(di::BasicError::InvalidArgument);
    }

    auto terminfo_hash_file =
        TRY(dius::open_sync(terminfo_dir.clone() / "ttx.terminfo.hash"_pv, dius::OpenMode::WriteClobber));
    di::writer_print<di::String::Encoding>(terminfo_hash_file, "{}"_sv, di::to_string(terminfo_hash));

    return terminfo_dir;
}

static auto do_new(Args&, NewBase& args) -> di::Result<> {
    auto const replay_mode = !args.replay_paths.empty();
    if (args.headless || replay_mode) {
        args.profile = ""_tsv;
    }

    if (args.profile.ends_with('/')) {
        return di::Unexpected(di::format_error("--profile cannot be a directory"_sv));
    }
    auto config_from_args = config_json::v1::Config {
        .input = {
            .prefix = args.prefix,
            .save_state_path = args.save_state_path.transform([](di::PathView path) { return di::to_utf8_string_lossy(path.data()); }),
        },
        .layout = {
            .hide_status_bar = args.hide_status_bar ? di::Optional(true) : di::nullopt,
        },
        .clipboard = {
            .mode = args.clipboard_mode,
        },
        .session = {
            .restore_layout = args.disable_layout_restore ? di::Optional(false) : di::nullopt,
            .save_layout = args.disable_layout_save ? di::Optional(false) : di::nullopt,
            .layout_name = args.layout_name.transform(di::to_utf8_string_lossy),
        },
        .shell = {
            .command = !args.command.empty () ? di::Optional(args.command | di::transform(di::to_utf8_string_lossy) | di::to<di::Vector>()) : di::nullopt,
        },
        .terminfo = {
            .term = args.term.transform(di::to_utf8_string_lossy),
            .force_local_terminfo = args.force_local_terminfo ? di::Optional(true) : di::nullopt,
        },
    };
    auto config = TRY(
        config_json::v1::resolve_profile(args.profile, di::clone(config_from_args)).transform_error([&](auto&& error) {
            return di::format_error("Failed to resolve profile '{}': {}"_sv, args.profile, error);
        }));

    auto features = Feature::All;
    if (!args.headless) {
        features = TRY(detect_features(dius::std_in));
    }

    // Setup - log to file.
    [[maybe_unused]] auto& log = dius::std_err = TRY(dius::open_sync("/tmp/ttx.log"_pv, dius::OpenMode::WriteClobber));

    // Setup - potentially compile terminfo database (this config only applies at startup)
    auto maybe_terminfo_dir =
        TRY(maybe_get_terminfo_dir(config.terminfo.term.view(), config.terminfo.force_local_terminfo));

    // Setup - initialize pane arguments
    auto base_create_pane_args = CreatePaneArgs {
        .command = config.shell.command.clone(),
        .capture_command_output_path = args.capture_command_output_path.transform(di::to_owned),
        .save_state_path = config.input.save_state_path.clone(),
        .terminfo_dir = di::move(maybe_terminfo_dir),
        .term = config.terminfo.term.clone(),
    };
    if (replay_mode) {
        base_create_pane_args.replay_path = di::PathView(args.replay_paths[0]).to_owned();
        if (!args.save_state_path) {
            base_create_pane_args.save_state_path = {};
        }
    }

    // Setup - in headless mode there is no terminal. Ensure stdin is not valid.
    if (args.headless) {
        (void) dius::std_in.close();
    }

    // Setup - initial state and terminal size.
    auto initial_size = args.headless ? Size { 24, 80, 24 * 16, 80 * 16 }
                                      : Size::from_window_size(TRY(dius::std_in.get_tty_window_size()));
    auto layout_state = di::Synchronized(LayoutState(initial_size, config.layout));

    // Setup - raw mode
    auto _ = args.headless ? di::ScopeExit(di::Function<void()>([] {})) : TRY(dius::std_in.enter_raw_mode());

    // Setup - block SIGWINCH.
    TRY(dius::system::mask_signal(dius::Signal::WindowChange));

    // Callback to exit the main thread.
    auto done = di::Atomic<bool>(false);
    auto set_done = [&] {
        if (!done.exchange(true, di::MemoryOrder::Release)) {
            // Ensure the SIGWINCH (main) thread exits.
            (void) dius::system::ProcessHandle::self().signal(dius::Signal::WindowChange);
        }
    };

    // Setup - layout save thread.
    auto session_save_dir = TRY(get_session_save_dir());
    auto layout_save_thread = TRY([&] -> di::Result<di::Box<SaveLayoutThread>> {
        if (args.headless) {
            return SaveLayoutThread::create_mock(layout_state);
        }
        return SaveLayoutThread::create(layout_state, session_save_dir.clone(), di::clone(config.session));
    }());
    auto _ = di::ScopeExit([&] {
        if (layout_save_thread) {
            layout_save_thread->request_exit();
        }
    });
    if (layout_save_thread) {
        layout_state.get_assuming_no_concurrent_accesses().set_layout_did_update([&] {
            layout_save_thread->request_save_layout();
        });
    }

    // Setup - render thread.
    auto render_thread = TRY(RenderThread::create(layout_state, set_done, di::clone(config), features));
    auto _ = di::ScopeExit([&] {
        render_thread->request_exit();
    });

    // Setup - input thread.
    auto input_thread = TRY([&] -> di::Result<di::Box<InputThread>> {
        if (args.headless) {
            return InputThread::create_mock(layout_state, *render_thread, *layout_save_thread);
        }
        return InputThread::create(base_create_pane_args.clone(), di::clone(config), di::clone(config_from_args),
                                   args.profile, layout_state, features, *render_thread, *layout_save_thread);
    }());
    auto _ = di::ScopeExit([&] {
        if (input_thread) {
            input_thread->request_exit();
        }
    });

    // Setup - remove all panes and tabs on exit.
    auto _ = di::ScopeExit([&] {
        layout_state.lock()->set_layout_did_update(nullptr);
        layout_state.with_lock([&](LayoutState& state) {
            while (!state.empty()) {
                auto& session = *state.sessions().front();
                while (!session->empty()) {
                    auto last_tab = session->tabs().size() == 1;
                    auto& tab = **session->tabs().front();
                    for (auto* pane : tab.panes()) {
                        state.remove_pane(*session, tab, pane);
                    }
                    // We must explicitly check this because the session object is destroyed
                    // after the last tab is removed.
                    if (last_tab) {
                        break;
                    }
                }
            }
        });
    });

    // Setup - initial tab and pane.
    auto exit_status = 0;
    TRY(layout_state.with_lock([&](LayoutState& state) -> di::Result<> {
        if (replay_mode) {
            for (auto replay_path : args.replay_paths) {
                auto create_pane_args = base_create_pane_args.clone();
                create_pane_args.replay_path = di::PathView(replay_path).to_owned();
                if (state.empty()) {
                    TRY(state.add_session(di::move(create_pane_args), *render_thread, *input_thread));
                } else {
                    // Horizontal split (means vertical layout)
                    TRY(state.add_pane(*state.active_session(), *state.active_tab(), di::move(create_pane_args),
                                       Direction::Vertical, *render_thread, *input_thread));
                }
            }
        } else {
            auto make_pane_args = [&] -> CreatePaneArgs {
                auto result = base_create_pane_args.clone();
                if (args.headless) {
                    result.hooks.did_exit = [&](Pane&, di::Optional<dius::system::ProcessResult> result) {
                        if (result && result.value().exited() && result.value().exit_code() == 0) {
                            exit_status = 0;
                        } else {
                            exit_status = 1;
                        }
                        render_thread->request_exit();
                    };
                }
                return result;
            };

            // Attempt to restore layout when running in auto-layout mode, or when specifically requested.
            if (config.session.restore_layout) {
                auto path = session_save_dir.clone();
                path /= config.session.layout_name;
                path += ".json"_tsv;

                // Ignore file not found errors.
                auto file = dius::open_sync(path, dius::OpenMode::Readonly);
                if (file) {
                    auto string = TRY(di::read_to_string(file.value()));
                    auto json = TRY(di::from_json_string<json::Layout>(string));
                    TRY(state.restore_json(json, make_pane_args(), *render_thread, *input_thread));
                } else if (file.error() != dius::PosixError::NoSuchFileOrDirectory) {
                    return di::Unexpected(di::move(file).error());
                }
            }

            if (state.empty()) {
                TRY(state.add_session(make_pane_args(), *render_thread, *input_thread));
            }
        }
        return {};
    }));

    // In headless and replay mode, exit immediately.
    if (args.headless && replay_mode) {
        return {};
    }

    render_thread->request_render();

#ifndef __linux__
    // On MacOS, we need to install a useless signal handlers for sigwait() to
    // actually work...
    dius::system::install_dummy_signal_handler(dius::Signal::WindowChange);
#endif

    while (!done.load(di::MemoryOrder::Acquire)) {
        if (!dius::system::wait_for_signal(dius::Signal::WindowChange)) {
            break;
        }
        if (done.load(di::MemoryOrder::Acquire)) {
            break;
        }

        auto size = dius::std_in.get_tty_window_size();
        if (!size) {
            continue;
        }

        render_thread->push_event(Size::from_window_size(size.value()));
    }

    if (exit_status) {
        return di::Unexpected(di::BasicError::InvalidArgument);
    }
    return {};
}

static auto main(Args& base_args, New& args) -> di::Result<> {
    return do_new(base_args, args);
}

static auto main(Args& base_args, Replay& args) -> di::Result<> {
    args.hide_status_bar = true;
    args.disable_layout_save = true;
    args.disable_layout_restore = true;
    return do_new(base_args, args);
}

static auto main(Args&, Completions& args) -> di::Result<> {
    auto parser = di::get_cli_parser<Args>();
    parser.write_completions(dius::std_out, args.shell);
    return {};
}

static auto main(Args&, Keybinds& args) -> di::Result<> {
    auto config = args.profile.empty()
                      ? Config()
                      : TRY(config_json::v1::resolve_profile(args.profile, {}).transform_error([&](auto&& error) {
                            return di::format_error("Failed to resolve profile '{}': {}"_sv, args.profile, error);
                        }));
    auto key_binds = make_key_binds(config.input, args.replay_mode);
    for (auto const& bind : key_binds) {
        dius::println("{}"_sv, bind);
    }
    return {};
}

static auto main(Args&, Features&) -> di::Result<> {
    auto features = TRY(detect_features(dius::std_in).transform_error([](auto error) {
        return di::format_error("Failed to detect terminal features: {}"_sv, error);
    }));
    dius::println("Feature: {}"_sv, features);
    return {};
}

static auto main(Args&, Terminfo& args) -> di::Result<> {
    auto const& terminfo = terminal::get_ttx_terminfo();
    if (args.format == TerminfoFormat::Terminfo) {
        dius::print("{}"_sv, terminfo.serialize());
        return {};
    }
    if (args.format == TerminfoFormat::Verbose) {
        dius::println("{}: {}"_sv, di::Styled("Names"_sv, di::FormatEffect::Bold),
                      terminfo.names | di::transform(di::to_string) | di::join_with(", "_sv) | di::to<di::String>());

        for (auto const& capability : terminfo.capabilities | di::filter(&terminal::Capability::enabled)) {
            dius::println("\t{: <32}{: <90}{: <80}"_sv, di::Styled(capability.long_name, di::FormatEffect::Bold),
                          capability.description, capability.serialize());
        }
        return {};
    }
    return di::Unexpected(di::BasicError::InvalidArgument);
}

static auto main(Args&, ConfigCommand&, ConfigShow& args) -> di::Result<> {
    if (args.profile.ends_with('/')) {
        return di::Unexpected(di::format_error("--profile cannot be a directory"_sv));
    }
    auto config = args.profile.empty()
                      ? Config()
                      : TRY(config_json::v1::resolve_profile(args.profile).transform_error([&](auto&& error) {
                            return di::format_error("Failed to resolve profile '{}': {}"_sv, args.profile, error);
                        }));
    auto string = *di::to_json_string(config_json::v1::to_config_json(di::move(config)),
                                      di::JsonSerializerConfig().pretty().indent_width(4));
    dius::println("{}"_sv, string);
    return {};
}

static auto main(Args&, ConfigCommand&, ConfigNix&) -> di::Result<> {
    auto string = config_json::v1::nix_options();
    dius::println("{}"_sv, string);
    return {};
}

static auto main(Args&, ConfigCommand&, ConfigSchema&) -> di::Result<> {
    auto string =
        *di::to_json_string(config_json::v1::json_schema(), di::JsonSerializerConfig().pretty().indent_width(4));
    dius::println("{}"_sv, string);
    return {};
}

static auto main(Args& base_args, ConfigCommand& args) -> di::Result<> {
    return di::visit(
        [&](auto& subcommand) -> di::Result<> {
            if constexpr (di::SameAs<di::Void, di::meta::RemoveCVRef<decltype(subcommand)>>) {
                ASSERT(false);
                return {};
            } else {
                return main(base_args, args, subcommand);
            }
        },
        args.subcommand);
}

static auto main(Args& args) -> di::Result<> {
    return di::visit(
        [&](auto& subcommand) {
            return main(args, subcommand);
        },
        args.subcommand);
}
}

DIUS_MAIN(ttx::Args, ttx)
