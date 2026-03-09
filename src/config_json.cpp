#include "config_json.h"

#include "config.h"
#include "di/serialization/json_deserializer.h"
#include "di/util/construct.h"
#include "di/vocab/error/string_error.h"
#include "dius/system/process.h"

namespace ttx::config_json::v1 {
static auto get_search_paths() -> di::Result<di::Vector<di::Path>> {
    auto const& env = dius::system::get_environment();
    auto config_home = env.at("XDG_CONFIG_HOME"_tsv)
                           .filter(di::not_fn(di::empty))
                           .transform(di::construct<di::PathView>)
                           .transform(di::to_owned)
                           .or_else([&] {
                               return env.at("HOME"_tsv).transform([&](di::TransparentStringView home) {
                                   return di::PathView(home).to_owned() / ".config"_tsv;
                               });
                           });
    if (!config_home) {
        return di::Unexpected(di::format_error("Failed to identify one of $XDG_CONFIG_HOME or $HOME/.config"_sv));
    }

    auto results = di::Vector<di::Path> {};
    results.push_back(di::move(config_home.value()));
    for (auto dir : env.at("XDG_CONFIG_DIRS"_tsv)
                            .filter(di::not_fn(di::empty))
                            .transform(di::construct<di::TransparentStringView>)
                            .value_or("/etc/xdg"_tsv) |
                        di::split(':')) {
        results.push_back(di::PathView(dir).to_owned());
    }

    for (auto& result : results) {
        result /= "ttx"_tsv;
    }
    return results;
}

static auto resolve_config_path(di::Span<di::Path const> search_paths, di::TransparentStringView config_path)
    -> di::Result<di::String> {
    if (config_path.starts_with('/')) {
        return dius::read_to_string(di::PathView(config_path));
    }
    for (auto const& dir : search_paths) {
        auto path = dir.clone() / config_path;
        auto result = dius::read_to_string(path);
        if (result) {
            return di::move(result).value();
        }
        if (result.error() != dius::PosixError::NoSuchFileOrDirectory) {
            return di::Unexpected(di::move(result.error()));
        }
        if (!path.extension()) {
            path += ".json"_tsv;
        }
        result = dius::read_to_string(path);
        if (result) {
            return di::move(result).value();
        }
        if (result.error() != dius::PosixError::NoSuchFileOrDirectory) {
            return di::Unexpected(di::move(result.error()));
        }
    }
    return di::Unexpected(dius::PosixError::NoSuchFileOrDirectory);
}

static auto merge(Config& left, Config&& right) {
    if (right.input.prefix) {
        left.input.prefix = right.input.prefix;
    }
    if (right.input.disable_default_keybinds) {
        left.input.disable_default_keybinds = right.input.disable_default_keybinds;
    }
    if (right.input.save_state_path) {
        left.input.save_state_path = di::move(right.input.save_state_path);
    }
    if (right.layout.hide_status_bar) {
        left.layout.hide_status_bar = right.layout.hide_status_bar;
    }
    if (right.clipboard.mode) {
        left.clipboard.mode = right.clipboard.mode;
    }
    if (right.session.restore_layout) {
        left.session.restore_layout = right.session.restore_layout;
    }
    if (right.session.save_layout) {
        left.session.save_layout = right.session.save_layout;
    }
    if (right.session.layout_name) {
        left.session.layout_name = di::move(right.session.layout_name);
    }
    if (right.shell.command) {
        left.shell.command = di::move(right.shell.command);
    }
    if (right.terminfo.term) {
        left.terminfo.term = di::move(right.terminfo.term);
    }
    if (right.terminfo.force_local_terminfo) {
        left.terminfo.force_local_terminfo = right.terminfo.force_local_terminfo;
    }
}

static auto to_transparent_string(di::String const& s) -> di::TransparentString {
    return s.span() | di::transform(di::construct<char>) | di::to<di::TransparentString>();
}

static auto convert(di::TransparentStringView profile, Config&& json_config) -> di::Result<ttx::Config> {
    if (!json_config.shell.command || json_config.shell.command.value().empty()) {
        auto const& env = dius::system::get_environment();
        if (!env.contains("SHELL"_tsv)) {
            return di::Unexpected(di::StringError(
                "$SHELL environment variable not set. Either set the environment variable, specify shell.command in your config file, or pass a command explicitly when starting ttx"_s));
        }
        json_config.shell.command =
            di::single(to_utf8_string(env.at("SHELL"_tsv).value())) | di::as_rvalue | di::to<di::Vector>();
    }
    auto default_config = ttx::Config {};
    return ttx::Config{
        .input = {
            .prefix = json_config.input.prefix.value_or(default_config.input.prefix),
            .disable_default_keybinds = json_config.input.disable_default_keybinds.value_or(default_config.input.disable_default_keybinds),
            .save_state_path = json_config.input.save_state_path.transform(to_transparent_string).transform(di::construct<di::Path>).value_or(default_config.input.save_state_path.clone()),
        },
        .layout = {
            .hide_status_bar = json_config.layout.hide_status_bar.value_or(default_config.layout.hide_status_bar),
        },
        .clipboard = {
            .mode = json_config.clipboard.mode.value_or(default_config.clipboard.mode),
        },
        .session = {
            .restore_layout = json_config.session.restore_layout.value_or(default_config.session.restore_layout),
            .save_layout = json_config.session.save_layout.value_or(default_config.session.save_layout),
            .layout_name = json_config.session.layout_name.transform(to_transparent_string).value_or(profile.to_owned()),
        },
        .shell = {
            .command = json_config.shell.command.value() | di::transform(to_transparent_string) | di::to<di::Vector>(),
        },
        .terminfo = {
            .term = json_config.terminfo.term.transform(to_transparent_string).value_or(default_config.terminfo.term.clone()),
            .force_local_terminfo = json_config.terminfo.force_local_terminfo.value_or(default_config.terminfo.force_local_terminfo),
        },
    };
}

auto resolve_profile(di::TransparentStringView profile, Config&& base_config) -> di::Result<ttx::Config> {
    auto const search_paths = TRY(get_search_paths());
    auto maybe_profile_config = resolve_config_path(search_paths.span(), profile);
    if (!maybe_profile_config.has_value()) {
        // If no config file is found, we just use the default configuration without erroring.
        if (maybe_profile_config.error() == dius::PosixError::NoSuchFileOrDirectory) {
            return convert(profile, di::move(base_config));
        }
        return di::Unexpected(di::format_error("Failed to reading config file: {}"_sv, maybe_profile_config.error()));
    }

    auto config_json = TRY(di::from_json_string<Config>(maybe_profile_config.value().view())
                               .transform_error([&](auto&& error) -> di::Error {
                                   return di::format_error("Failed to parse JSON: {}"_sv, error);
                               }));
    merge(config_json, di::move(base_config));
    return convert(profile, di::move(config_json));
}
}
