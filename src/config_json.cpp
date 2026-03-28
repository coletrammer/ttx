#include "config_json.h"

#include "config.h"
#include "di/container/string/conversion.h"
#include "di/container/tree/tree_set.h"
#include "di/reflect/reflect.h"
#include "di/serialization/json_deserializer.h"
#include "di/serialization/json_serializer.h"
#include "di/util/construct.h"
#include "di/vocab/error/string_error.h"
#include "dius/filesystem/directory_iterator.h"
#include "dius/print.h"
#include "dius/system/process.h"
#include "theme.h"
#include "ttx/terminal/color.h"
#include "ttx/terminal/palette.h"

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

static auto get_theme_search_paths() -> di::Result<di::Vector<di::Path>> {
    auto result = TRY(get_search_paths());
    for (auto& path : result) {
        path /= "themes"_pv;
    }
    return result;
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

struct Merge {
    template<di::concepts::ReflectableToFields T>
    static void operator()(T& left, T&& right) {
        di::tuple_for_each(
            [&](auto field) {
                using Value = di::meta::Type<decltype(field)>;
                if constexpr (di::concepts::Optional<Value>) {
                    if (field.get(right)) {
                        field.get(left) = di::move(field.get(right));
                    }
                } else if constexpr (di::ReflectableToFields<Value>) {
                    Merge::operator()(field.get(left), di::move(field.get(right)));
                }
            },
            di::reflect(left));
    }
};

constexpr inline auto merge = Merge {};

struct ConvertValue {
    template<typename T>
    static auto operator()(di::InPlaceType<T>, T&& value) -> T {
        return di::forward<T>(value);
    }

    template<typename T, typename U>
    static auto operator()(di::InPlaceType<di::Vector<T>>, di::Vector<U>&& value) {
        auto result = di::Vector<T> {};
        for (auto& v : value) {
            result.emplace_back(ConvertValue::operator()(di::in_place_type<T>, di::move(v)));
        }
        return result;
    }

    static auto operator()(di::InPlaceType<di::Path>, di::String&& value) {
        return di::Path(di::to_transparent_string(value));
    }

    static auto operator()(di::InPlaceType<di::String>, di::Path&& value) {
        return di::to_utf8_string_lossy(value.data());
    }

    static auto operator()(di::InPlaceType<di::TransparentString>, di::String&& value) {
        return di::to_transparent_string(value);
    }

    static auto operator()(di::InPlaceType<di::String>, di::TransparentString&& value) {
        return di::to_utf8_string_lossy(value);
    }

    static auto operator()(di::InPlaceType<terminal::Palette>, Colors&& value) -> terminal::Palette {
        auto result = terminal::Palette::default_global();
        if (value.palette.has_value()) {
            for (auto [i, c] :
                 di::enumerate(value.palette.value()) | di::take(u32(terminal::PaletteIndex::SpecialBegin))) {
                result.set(terminal::PaletteIndex(i), c);
            }
        }
        if (value.foreground.has_value()) {
            result.set(terminal::PaletteIndex::Foreground, value.foreground.value());
        }
        if (value.background.has_value()) {
            result.set(terminal::PaletteIndex::Background, value.background.value());
        }
        if (value.selection_foreground.has_value()) {
            result.set(terminal::PaletteIndex::SelectionForeground, value.selection_foreground.value());
        }
        if (value.selection_background.has_value()) {
            result.set(terminal::PaletteIndex::SelectionBackground, value.selection_background.value());
        }
        if (value.cursor.has_value()) {
            result.set(terminal::PaletteIndex::Cursor, value.cursor.value());
        }
        if (value.cursor_text.has_value()) {
            result.set(terminal::PaletteIndex::CursorText, value.cursor_text.value());
        }
        return result;
    }

    static auto operator()(di::InPlaceType<Colors>, terminal::Palette&& value) -> Colors {
        auto result = Colors {};
        {
            result.palette.emplace();

            auto required_entries = 0_u32;
            for (auto i : di::range(u32(terminal::PaletteIndex::SpecialBegin))) {
                if (!value.get(terminal::PaletteIndex(i)).is_default()) {
                    required_entries = i + 1;
                }
            }
            for (auto i : di::range(required_entries)) {
                result.palette.value().push_back(value.get(terminal::PaletteIndex(i)));
            }
        }
        if (auto color = value.get(terminal::PaletteIndex::Foreground); !color.is_default()) {
            result.foreground = color;
        }
        if (auto color = value.get(terminal::PaletteIndex::Background); !color.is_default()) {
            result.background = color;
        }
        if (auto color = value.get(terminal::PaletteIndex::SelectionForeground); !color.is_default()) {
            result.selection_foreground = color;
        }
        if (auto color = value.get(terminal::PaletteIndex::SelectionBackground); !color.is_default()) {
            result.selection_background = color;
        }
        if (auto color = value.get(terminal::PaletteIndex::Cursor); !color.is_default()) {
            result.cursor = color;
        }
        if (auto color = value.get(terminal::PaletteIndex::CursorText); !color.is_default()) {
            result.cursor_text = color;
        }
        return result;
    }
};

constexpr inline auto convert_value = ConvertValue {};

struct DoConvert {
    template<di::concepts::ReflectableToFields Config, di::concepts::ReflectableToFields JsonConfig>
    static auto operator()(Config& config, JsonConfig&& json_config) {
        di::tuple_for_each(
            [&](auto field) {
                di::tuple_for_each(
                    [&](auto json_field) {
                        if constexpr (field.name == json_field.name) {
                            DoConvert::operator()(field.get(config), di::move(json_field.get(json_config)));
                        }
                    },
                    di::reflect(json_config));
            },
            di::reflect(config));
    }

    static auto operator()(terminal::Palette& palette, Colors&& terminal_theme) {
        palette = convert_value(di::in_place_type<terminal::Palette>, di::move(terminal_theme));
    }

    template<typename T, di::concepts::Optional U>
    static auto operator()(T& value, U&& maybe_json_value) {
        if (maybe_json_value.has_value()) {
            value = convert_value(di::in_place_type<T>, di::forward<U>(maybe_json_value).value());
        }
    }
};

constexpr inline auto do_convert = DoConvert {};

static auto convert(di::TransparentStringView profile, Config&& json_config) -> di::Result<ttx::Config> {
    if (!json_config.shell.command || json_config.shell.command.value().empty()) {
        auto const& env = dius::system::get_environment();
        if (!env.contains("SHELL"_tsv)) {
            return di::Unexpected(di::StringError(
                "$SHELL environment variable not set. Either set the environment variable, specify shell.command in your config file, or pass a command explicitly when starting ttx"_s));
        }
        json_config.shell.command =
            di::single(di::to_utf8_string_lossy(env.at("SHELL"_tsv).value())) | di::as_rvalue | di::to<di::Vector>();
    }
    if (!json_config.session.layout_name) {
        json_config.session.layout_name = di::to_utf8_string_lossy(profile);
    }
    return convert_to_config(di::move(json_config));
}

static auto load_config_recursively(di::Span<di::Path const> search_paths, di::TreeSet<di::TransparentString>& visited,
                                    di::TransparentStringView config_name) -> di::Result<Config> {
    if (visited.contains(config_name)) {
        return di::Unexpected(
            di::format_error("Failed loading configuration due to circular import of config '{}'"_sv, config_name));
    }

    auto maybe_profile_config = resolve_config_path(search_paths.span(), config_name);
    if (!maybe_profile_config.has_value()) {
        // If no config file is found and this is the root config file, we just use the default configuration without
        // erroring.
        if (visited.empty() && maybe_profile_config.error() == dius::PosixError::NoSuchFileOrDirectory) {
            return {};
        }
        return di::Unexpected(di::format_error("Failed to reading config file: {}"_sv, maybe_profile_config.error()));
    }

    visited.insert(config_name.to_owned());
    auto config_json =
        TRY(di::from_json_string<Config>(maybe_profile_config.value().view())
                .transform_error([&](auto&& error) -> di::Error {
                    return di::format_error("Failed to parse JSON for config '{}': {}"_sv, config_name, error);
                }));
    if (config_json.extends.has_value()) {
        for (auto const& name : config_json.extends.value()) {
            auto name_ts = di::to_transparent_string(name);
            auto subconfig = TRY(load_config_recursively(search_paths, visited, name_ts));
            merge(subconfig, di::move(config_json));
            config_json = di::move(subconfig);
        }
    }
    return config_json;
}

auto list_custom_themes() -> di::Result<di::Vector<ListedTheme>> {
    auto result = di::TreeMap<di::TransparentString, ListedTheme> {};
    auto const search_paths = TRY(get_theme_search_paths());
    for (auto const& path : search_paths) {
        auto iterator = dius::filesystem::DirectoryIterator::create(
            path.clone(), dius::filesystem::DirectoryOptions::FollowDirectorySymlink);
        if (!iterator && iterator.error() == dius::PosixError::NoSuchFileOrDirectory) {
            continue;
        }
        if (!iterator) {
            return di::Unexpected(
                di::format_error("Failed to list directory contents of '{}': {}"_sv, path, iterator.error()));
        }
        for (auto&& entry : iterator.value()) {
            if (!entry) {
                return di::Unexpected(
                    di::format_error("Failed to list directory contents of '{}': {}"_sv, path, iterator.error()));
            }
            auto name = entry->filename().value().stem().value().to_owned();
            auto config = TRY(resolve_custom_theme(entry->path_view().data()));
            auto palette = convert_value(di::in_place_type<terminal::Palette>, di::move(config.colors));
            result.try_emplace(name, ListedTheme {
                                         .name = name.clone(),
                                         .source = ThemeSource::Custom,
                                         .theme_mode = palette.theme_mode(),
                                         .palette = palette,
                                     });
        }
    }
    return result | di::values | di::as_rvalue | di::to<di::Vector>();
}

auto resolve_custom_theme(di::TransparentStringView name) -> di::Result<Config> {
    if (name.empty()) {
        return di::Unexpected(di::format_error("Failed to find theme with name '{}'"_sv, name));
    }
    auto const search_paths = TRY(get_theme_search_paths());
    auto maybe_theme = resolve_config_path(search_paths.span(), name);
    if (!maybe_theme.has_value()) {
        return di::Unexpected(di::format_error("Failed to find theme with name '{}'"_sv, name));
    }

    return di::from_json_string<Config>(maybe_theme.value().view()).transform_error([&](auto&& error) -> di::Error {
        return di::format_error("Failed to parse JSON for theme '{}': {}"_sv, name, error);
    });
}

auto convert_to_config(Config&& config) -> ttx::Config {
    auto result = ttx::Config {};
    do_convert(result, di::move(config));
    return result;
}

#ifndef TTX_BUILT_IN_THEMES
auto iterm2_themes() -> di::TreeMap<di::TransparentString, config_json::v1::Config> const& {
    static auto map = di::TreeMap<di::TransparentString, config_json::v1::Config> {};
    return map;
}
#endif

auto list_themes(ThemeSource source, terminal::Palette const& outer_terminal_palette)
    -> di::Result<di::Vector<ListedTheme>> {
    auto result = di::Vector<ListedTheme> {};
    if (source == ThemeSource::Custom || source == ThemeSource::All) {
        result.append_container(TRY(list_custom_themes()) | di::as_rvalue);
    }
    if (source == ThemeSource::BuiltIn || source == ThemeSource::All) {
        result.push_back(ListedTheme {
            .name = "auto"_ts,
            .source = ThemeSource::BuiltIn,
            .theme_mode = outer_terminal_palette.theme_mode(),
            .palette = outer_terminal_palette,
        });
        result.push_back(ListedTheme {
            .name = "ansi"_ts,
            .source = ThemeSource::BuiltIn,
            .theme_mode = outer_terminal_palette.theme_mode(),
            .palette = outer_terminal_palette,
        });
        for (auto const& [name, config] : built_in_themes()) {
            auto palette = convert_value(di::in_place_type<terminal::Palette>, di::clone(config.colors));
            result.push_back(ListedTheme {
                .name = name.clone(),
                .source = ThemeSource::BuiltIn,
                .theme_mode = palette.theme_mode(),
                .palette = palette,

            });
        }
        for (auto const& [name, config] : iterm2_themes()) {
            auto palette = convert_value(di::in_place_type<terminal::Palette>, di::clone(config.colors));
            result.push_back(ListedTheme {
                .name = name.clone(),
                .source = ThemeSource::Iterm2ColorSchemes,
                .theme_mode = palette.theme_mode(),
                .palette = palette,

            });
        }
    }
    return result;
}

struct ApplyThemeDefaults {
    template<typename T, typename U>
    static auto operator()(di::Optional<T>&, U&&) {}

    static auto operator()(Colors& json_value, terminal::Palette&& value) {
        json_value = convert_value(di::in_place_type<Colors>, di::forward<terminal::Palette>(value));
    }

    static auto operator()(di::Optional<terminal::Color>& json_value, terminal::Color&& value) {
        json_value = convert_value(di::in_place_type<terminal::Color>, di::forward<terminal::Color>(value));
    }

    template<di::concepts::ReflectableToFields JsonConfig, di::concepts::ReflectableToFields Config>
    static auto operator()(JsonConfig&& json_config, Config&& config) {
        di::tuple_for_each(
            [&](auto field) {
                di::tuple_for_each(
                    [&](auto json_field) {
                        if constexpr (field.name == json_field.name) {
                            ApplyThemeDefaults::operator()(json_field.get(json_config), di::move(field.get(config)));
                        }
                    },
                    di::reflect(json_config));
            },
            di::reflect(config));
    }
};

constexpr inline auto apply_theme_defaults = ApplyThemeDefaults {};

auto resolve_theme(di::TransparentStringView name, terminal::Palette const& outer_terminal_palette)
    -> di::Result<Config> {
    if (name.empty() || name == "auto"_tsv) {
        // Theme auto detection. First list all candidate themes.
        auto all_themes = TRY(list_themes(ThemeSource::All, outer_terminal_palette));

        // This algorithm is sub-optimal but its not that bad with only ~500 built-in themes.
        // Theme matching would be significantly more optimal by hashing the bottom 8 palette colors
        // for every theme.
        for (auto const& theme : all_themes) {
            // Check for an exact match with the bottom 8 palette colors. If we match,
            // then we override the name to theme we matched.
            if (!theme.name.empty() && theme.name != "auto"_tsv && theme.name != "ansi"_tsv &&
                di::all_of(di::range(8u), [&](u32 index_number) -> bool {
                    auto index = terminal::PaletteIndex(index_number);
                    return theme.palette.get(index) == outer_terminal_palette.get(index);
                })) {
                auto result = TRY(resolve_theme(theme.name.view(), outer_terminal_palette));
                result.theme.name = di::to_utf8_string_lossy(theme.name.view());
                return result;
            }
        }
    }

    auto result = resolve_custom_theme(name);
    if (!result) {
        if (name.empty() || name == "ansi"_tsv || name == "auto"_tsv) {
            auto result = Config {};
            auto defaults = ttx::Config {};
            apply_theme_defaults(result, di::move(defaults));
            return result;
        }
        for (auto const& config : built_in_themes().at(name)) {
            return di::clone(config);
        }
        for (auto const& config : iterm2_themes().at(name)) {
            return di::clone(config);
        }
    }
    return result;
}

auto resolve_profile_to_json(di::TransparentStringView profile, terminal::ThemeMode theme_preference,
                             terminal::Palette const& outer_terminal_palette, Config&& cli_config,
                             bool should_resolve_theme) -> di::Result<Config> {
    auto profile_json = TRY([&] -> di::Result<Config> {
        if (profile.empty()) {
            return {};
        }
        auto const search_paths = TRY(get_search_paths());
        auto visited = di::TreeSet<di::TransparentString> {};
        return load_config_recursively(search_paths.span(), visited, profile);
    }());
    merge(profile_json, di::move(cli_config));

    if (!should_resolve_theme) {
        return profile_json;
    }

    auto theme_name_utf8 = [&] -> di::StringView {
        if (theme_preference == terminal::ThemeMode::Dark && profile_json.theme.dark.has_value()) {
            return profile_json.theme.dark.value().view();
        }
        if (theme_preference == terminal::ThemeMode::Light && profile_json.theme.light.has_value()) {
            return profile_json.theme.light.value().view();
        }
        return profile_json.theme.name.transform(&di::String::view).value_or("auto"_sv);
    }();
    auto theme_name = di::to_transparent_string(theme_name_utf8);
    auto theme = TRY(resolve_theme(theme_name, outer_terminal_palette));
    merge(theme, di::move(profile_json));
    return theme;
}

auto resolve_profile(di::TransparentStringView profile, terminal::ThemeMode theme_preference,
                     terminal::Palette const& outer_terminal_palette, Config&& cli_config, bool resolve_theme)
    -> di::Result<ttx::Config> {
    auto config_json = TRY(resolve_profile_to_json(profile, theme_preference, outer_terminal_palette,
                                                   di::move(cli_config), resolve_theme));
    auto const profile_name = di::PathView(profile).stem().value_or(""_tsv);
    return convert(profile_name, di::move(config_json));
}

auto config_from_palette(terminal::Palette const& palette) -> config_json::v1::Config {
    auto result = config_json::v1::Config {};
    result.colors = convert_value(di::in_place_type<Colors>, auto(palette));
    return result;
}

struct ApplyDefaults {
    template<typename T, typename U>
    static auto operator()(di::Optional<T>& json_value, U&& value) {
        if constexpr (di::concepts::SameAs<di::String, T> || di::concepts::InstanceOf<T, di::Vector>) {
            if (value.empty()) {
                return;
            }
        }
        json_value = convert_value(di::in_place_type<T>, di::forward<U>(value));
    }

    static auto operator()(Colors&, terminal::Palette&&) {}

    static auto operator()(di::Optional<terminal::Color>&, terminal::Color&&) {}

    template<di::concepts::ReflectableToFields JsonConfig, di::concepts::ReflectableToFields Config>
    static auto operator()(JsonConfig&& json_config, Config&& config) {
        di::tuple_for_each(
            [&](auto field) {
                di::tuple_for_each(
                    [&](auto json_field) {
                        if constexpr (field.name == json_field.name) {
                            ApplyDefaults::operator()(json_field.get(json_config), di::move(field.get(config)));
                        }
                    },
                    di::reflect(json_config));
            },
            di::reflect(config));
    }
};

constexpr inline auto apply_defaults = ApplyDefaults {};

auto config_with_defaults(Config&& config) -> Config {
    auto defaults = ttx::Config {};
    apply_defaults(config, di::move(defaults));
    return di::move(config);
}

struct BuildJsonSchemaDefinitions {
    static auto operator()(di::InPlaceType<bool>, di::TreeMap<di::String, di::json::Object>&, bool const& defaults)
        -> di::json::Object {
        auto response = di::json::Object {};
        response["type"_sv] = "boolean"_s;
        response["default"_sv] = defaults;
        return response;
    }

    static auto operator()(di::InPlaceType<di::String>, di::TreeMap<di::String, di::json::Object>&,
                           di::String const& defaults) -> di::json::Object {
        auto response = di::json::Object {};
        response["type"_sv] = "string"_s;
        if (!defaults.empty()) {
            response["default"_sv] = defaults.clone();
        }
        return response;
    }

    static auto operator()(di::InPlaceType<terminal::Color>, di::TreeMap<di::String, di::json::Object>&,
                           terminal::Color const& defaults) -> di::json::Object {
        auto response = di::json::Object {};
        response["type"_sv] = "string"_s;
        response["default"_sv] = di::to_string(defaults);
        return response;
    }

    template<typename T>
    static auto operator()(di::InPlaceType<di::Optional<T>>, di::TreeMap<di::String, di::json::Object>& output,
                           di::Optional<T> const& defaults) -> di::json::Object {
        if (!defaults.has_value()) {
            return BuildJsonSchemaDefinitions::operator()(di::in_place_type<T>, output, T {});
        }
        return BuildJsonSchemaDefinitions::operator()(di::in_place_type<T>, output, defaults.value());
    }

    template<typename T>
    static auto operator()(di::InPlaceType<di::Vector<T>>, di::TreeMap<di::String, di::json::Object>& output,
                           di::Vector<T> const&) -> di::json::Object {
        auto response = di::json::Object {};
        response["type"_sv] = "array"_s;
        response["items"_sv] = BuildJsonSchemaDefinitions::operator()(di::in_place_type<T>, output, T());
        response["default"_sv] = di::json::Array {};
        return response;
    }

    template<di::concepts::ReflectableToEnumerators T, typename M = di::meta::Reflect<T>>
    static auto operator()(di::InPlaceType<T>, di::TreeMap<di::String, di::json::Object>& output, T const& defaults)
        -> di::json::Object {
        constexpr auto type_name = di::container::fixed_string_to_utf8_string_view<M::name>();

        auto response = di::json::Object {};
        response["default"_sv] = di::to_string(defaults);
        response["$ref"_sv] = di::format("#/$defs/{}"_sv, type_name);
        if (output.contains(type_name)) {
            return response;
        }

        auto result = di::json::Object {};
        if (!M::description.empty()) {
            result["description"_sv] = di::container::fixed_string_to_utf8_string_view<M::description>().to_owned();
        }
        auto values = di::json::Array {};
        di::tuple_for_each(
            [&]<typename E>(E) {
                constexpr auto enum_name = di::container::fixed_string_to_utf8_string_view<E::name>();
                values.push_back(enum_name.to_owned());
            },
            M {});
        result["enum"_sv] = di::move(values);
        output[type_name] = di::move(result);
        return response;
    }

    template<di::concepts::ReflectableToFields T, typename M = di::meta::Reflect<T>>
    static auto operator()(di::InPlaceType<T>, di::TreeMap<di::String, di::json::Object>& output, T const& defaults)
        -> di::json::Object {
        constexpr auto type_name = di::container::fixed_string_to_utf8_string_view<M::name>();

        auto response = di::json::Object {};
        response["$ref"_sv] = di::format("#/$defs/{}"_sv, type_name);
        if (output.contains(type_name)) {
            return response;
        }

        auto result = di::json::Object {};
        result["type"_sv] = "object"_s;
        if (!M::description.empty()) {
            result["description"_sv] = di::container::fixed_string_to_utf8_string_view<M::description>().to_owned();
        }
        auto properties = di::json::Object {};
        di::tuple_for_each(
            [&]<typename F>(F field) {
                constexpr auto field_name = di::container::fixed_string_to_utf8_string_view<F::name>();
                if constexpr (field_name.starts_with(U'$')) {
                    return;
                } else if constexpr (field_name == "version"_sv) {
                    auto o = di::json::Object {};
                    o["const"_sv] = 1;
                    if (!F::description.empty()) {
                        o["description"_sv] =
                            di::container::fixed_string_to_utf8_string_view<F::description>().to_owned();
                    }
                    properties[field_name] = di::json::Value(di::move(o));
                } else {
                    auto o = BuildJsonSchemaDefinitions::operator()(di::in_place_type<typename F::Type>, output,
                                                                    field.get(defaults));
                    if (!F::description.empty()) {
                        o["description"_sv] =
                            di::container::fixed_string_to_utf8_string_view<F::description>().to_owned();
                    }
                    properties[field_name] = di::json::Value(di::move(o));
                }
            },
            M {});
        result["properties"_sv] = di::move(properties);
        output[type_name] = di::move(result);
        return response;
    }
};

constexpr inline auto build_json_schema_definitions = BuildJsonSchemaDefinitions {};

auto json_schema() -> di::json::Object {
    auto definitions = di::TreeMap<di::String, di::json::Object> {};
    auto const defaults = config_with_defaults(Config {});
    build_json_schema_definitions(di::in_place_type<Config>, definitions, defaults);

    auto& o = definitions["Config"_sv];
    o["$schema"_sv] = "https://json-schema.org/draft/2020-12/schema"_s;
    o["$id"_sv] = Config().schema;
    o["title"_sv] = "ttx Configuration"_s;
    auto defs = di::json::Object();
    for (auto& [name, schema] : definitions) {
        if (name == "Config"_sv) {
            continue;
        }
        defs[name] = di::move(schema);
    }
    if (!defs.empty()) {
        o["$defs"_sv] = di::move(defs);
    }

    auto examples = di::json::Array {};
    auto default_as_json_string = *di::to_json_string(defaults);
    auto example_object = *di::from_json_string<di::json::Object>(default_as_json_string.view());
    strip_empty_objects(example_object);
    examples.emplace_back(di::move(example_object));
    o["examples"_sv] = di::move(examples);

    return di::move(o);
}

static auto camel_case(di::StringView s) -> di::String {
    auto result = ""_s;
    auto first = true;
    for (auto c : s) {
        if (first && c >= U'A' && c <= U'Z') {
            result.push_back(c | 0x20);
        } else {
            result.push_back(c);
        }
        first = false;
    }
    return result;
}

struct NixOption {
    di::String name;
    di::String type;
    di::String description;
};

struct NixSubmodule {
    di::String name;
    di::Vector<NixOption> options;
};

struct NixEnum {
    di::String name;
    di::Vector<di::String> values;
};

using NixValue = di::Variant<NixSubmodule, NixEnum>;

struct BuildNixOptions {
    static auto operator()(di::InPlaceType<bool>, di::TreeMap<di::String, NixValue>&) -> di::String { return "bool"_s; }

    static auto operator()(di::InPlaceType<u32>, di::TreeMap<di::String, NixValue>&) -> di::String {
        return "ints.u32"_s;
    }

    static auto operator()(di::InPlaceType<di::String>, di::TreeMap<di::String, NixValue>&) -> di::String {
        return "str"_s;
    }

    static auto operator()(di::InPlaceType<terminal::Color>, di::TreeMap<di::String, NixValue>&) -> di::String {
        return "str"_s;
    }

    template<typename T>
    static auto operator()(di::InPlaceType<di::Optional<T>>, di::TreeMap<di::String, NixValue>& output) -> di::String {
        auto result = BuildNixOptions::operator()(di::in_place_type<T>, output);
        return di::format("nullOr ({})"_sv, result);
    }

    template<typename T>
    static auto operator()(di::InPlaceType<di::Vector<T>>, di::TreeMap<di::String, NixValue>& output) -> di::String {
        auto type = BuildNixOptions::operator()(di::in_place_type<T>, output);
        return di::format("listOf ({})"_sv, type);
    }

    template<di::concepts::ReflectableToEnumerators T, typename M = di::meta::Reflect<T>>
    static auto operator()(di::InPlaceType<T>, di::TreeMap<di::String, NixValue>& output) -> di::String {
        auto const type_name = camel_case(di::container::fixed_string_to_utf8_string_view<M::name>());

        auto response = type_name.clone();
        if (output.contains(type_name)) {
            return response;
        }

        auto result = NixEnum {};
        result.name = type_name.clone();
        di::tuple_for_each(
            [&]<typename E>(E) {
                constexpr auto enum_name = di::container::fixed_string_to_utf8_string_view<E::name>();
                result.values.push_back(enum_name.to_owned());
            },
            M {});
        output[type_name] = di::move(result);
        return response;
    }

    template<di::concepts::ReflectableToFields T, typename M = di::meta::Reflect<T>>
    static auto operator()(di::InPlaceType<T>, di::TreeMap<di::String, NixValue>& output) -> di::String {
        auto const type_name = camel_case(di::container::fixed_string_to_utf8_string_view<M::name>());

        auto response = di::format("nullOr {}"_sv, type_name.clone());
        if (output.contains(type_name)) {
            return response;
        }

        auto result = NixSubmodule {};
        result.name = type_name.clone();
        di::tuple_for_each(
            [&]<typename F>(F) {
                constexpr auto field_name = di::container::fixed_string_to_utf8_string_view<F::name>();
                auto type = BuildNixOptions::operator()(di::in_place_type<typename F::Type>, output);
                result.options.push_back(NixOption {
                    .name = field_name.to_owned(),
                    .type = di::move(type),
                    .description = di::container::fixed_string_to_utf8_string_view<F::description>().to_owned(),
                });
            },
            M {});
        output[type_name] = di::move(result);
        return response;
    }
};

constexpr inline auto build_nix_options = BuildNixOptions {};

struct NixToString {
    static auto operator()(NixEnum const& value) -> di::String {
        return di::format("  {} = enum [ {} ]"_sv, value.name,
                          value.values | di::transform([](auto const& value) {
                              return di::format("\"{}\""_sv, value);
                          }) | di::join_with(U' ') |
                              di::to<di::String>());
    }

    static auto operator()(NixOption const& value) -> di::String {
        auto result = ""_s;
        result += di::format("      {:?} = lib.mkOption {{\n"_sv, value.name);
        result += di::format("        type = {};\n"_sv, value.type);
        if (!value.description.empty()) {
            result += di::format("        description = {:?};\n"_sv, value.description);
        }
        result += di::format("        default = null;\n"_sv);
        result += di::format("      }}"_sv);
        return result;
    }

    static auto operator()(NixSubmodule const& value) -> di::String {
        auto result = ""_s;
        result += di::format("  {} = submodule {{\n"_sv, value.name);
        result += di::format("    options = {{\n"_sv);
        for (auto const& option : value.options) {
            result += di::format("{};\n"_sv, NixToString::operator()(option));
        }
        result += di::format("    }};\n"_sv);
        result += di::format("  }}"_sv);
        return result;
    }

    static auto operator()(NixValue const& value) -> di::String { return di::visit(NixToString {}, value); }
};

constexpr inline auto nix_to_string = NixToString {};

auto nix_options() -> di::String {
    auto definitions = di::TreeMap<di::String, NixValue> {};
    build_nix_options(di::in_place_type<Config>, definitions);

    auto defs = ""_s;
    for (auto const& [_, value] : definitions) {
        defs += di::format("{};\n"_sv, nix_to_string(value));
    }
    return di::format(R"~(# This file was autogenerated via `ttx config nix`. Do not modify.
{{ lib, ... }}:
with lib.types;
let
{}in
lib.mkOption {{
  type = attrsOf config;
  default = {{}};
  description = ''
    Attribute set representing a set ttx configuration files to be written to `$XDG_CONFIG_HOME/ttx`.
    Each file name is accessible as a ttx profile.
    The default ttx profile is `main`, and is commonly the only configuration file which must be defined.'';
  example = {{
    main = {{
      input = {{
        prefix = "A";
      }};
    }};
  }};
}})~"_sv,
                      defs);
}

struct MarkdownType {
    static auto operator()(di::InPlaceType<bool>) -> di::String { return "boolean"_s; }

    static auto operator()(di::InPlaceType<u32>) -> di::String { return "unsigned integer"_s; }

    static auto operator()(di::InPlaceType<di::String>) -> di::String { return "string"_s; }

    static auto operator()(di::InPlaceType<terminal::Color>) -> di::String {
        auto const type_name = "Color"_sv;
        return di::format("[{}](#{})"_sv, type_name, type_name);
    }

    template<typename T>
    static auto operator()(di::InPlaceType<di::Vector<T>>) -> di::String {
        return di::format("list of {}"_sv, MarkdownType::operator()(di::in_place_type<T>));
    }

    template<typename T>
    static auto operator()(di::InPlaceType<di::Optional<T>>) -> di::String {
        return MarkdownType::operator()(di::in_place_type<T>);
    }

    template<di::concepts::ReflectableToEnumerators T>
    static auto operator()(di::InPlaceType<T>) -> di::String {
        auto const type_name = di::container::fixed_string_to_utf8_string_view<di::Reflect<T>::name>();
        return di::format("[{}](#{})"_sv, type_name, type_name);
    }

    template<di::concepts::ReflectableToFields T>
    static auto operator()(di::InPlaceType<T>) -> di::String {
        auto const type_name = di::container::fixed_string_to_utf8_string_view<di::Reflect<T>::name>();
        return di::format("[{}](#{})"_sv, type_name, type_name);
    }
};

constexpr inline auto markdown_type = MarkdownType {};

struct MarkdownDefault {
    template<di::concepts::OneOf<bool, u32> T>
    static auto operator()(T const& value) -> di::String {
        return di::format("{}"_sv, value);
    }

    static auto operator()(terminal::Color value) -> di::String { return di::format("\"{}\""_sv, value); }

    static auto operator()(di::String const& value) -> di::String { return di::format("{:?}"_sv, value); }

    template<typename T>
    static auto operator()(di::Vector<T> const&) -> di::String {
        return "[]"_s;
    }

    template<typename T>
    static auto operator()(di::Optional<T> const& value) -> di::String {
        if (!value) {
            return "unset"_s;
        }
        return MarkdownDefault::operator()(value.value());
    }

    template<di::concepts::ReflectableToEnumerators T>
    static auto operator()(T value) -> di::String {
        return di::format("\"{}\""_sv, value);
    }

    template<di::concepts::ReflectableToFields T>
    static auto operator()(T const&) -> di::String {
        return "{}"_s;
    }
};

constexpr inline auto markdown_default = MarkdownDefault {};

struct BuildMarkdownTable {
    template<di::concepts::OneOf<bool, u32, di::String> T>
    static auto operator()(di::InPlaceType<T>, di::Vector<di::Tuple<di::String, di::String>>&, T const&) -> di::String {
        return markdown_type(di::in_place_type<T>);
    }

    template<typename T>
    static auto operator()(di::InPlaceType<di::Vector<T>>, di::Vector<di::Tuple<di::String, di::String>>& result,
                           di::Vector<T> const&) -> di::String {
        BuildMarkdownTable::operator()(di::in_place_type<T>, result, T());
        return markdown_type(di::in_place_type<di::Vector<T>>);
    }

    template<typename T>
    static auto operator()(di::InPlaceType<di::Optional<T>>, di::Vector<di::Tuple<di::String, di::String>>& result,
                           di::Optional<T> const&) -> di::String {
        BuildMarkdownTable::operator()(di::in_place_type<T>, result, T());
        return markdown_type(di::in_place_type<di::Optional<T>>);
    }

    static auto operator()(di::InPlaceType<terminal::Color>, di::Vector<di::Tuple<di::String, di::String>>& result,
                           terminal::Color const&) -> di::String {
        auto response = markdown_type(di::in_place_type<terminal::Color>);
        constexpr auto type_name = "Color"_sv;
        if (di::contains(result | di::keys, type_name)) {
            return response;
        }
        auto section_content = di::format("### {}\n"_sv, type_name);
        section_content += R"md(
A color in ttx can be defined in one of four ways:

- A named special color ("default" or "dynamic"). Default has the special meaning of using the outer terminal's default color. Dynamic has the special depending on the specific color entry being specified and is not normally valid.
- A normal named color (such as "red" or "blue") as defined in the kitty color stack [docs](https://sw.kovidgoyal.net/kitty/color-stack).
- An 8 bit hex color (such as "#ff0000").
- A palette color via palette:N (such as "palette:0"). This is only valid when specifying colors not in already a palette color. For example, you can set the status bar's background to a palette color but not palette color 0 itself.

)md"_sv;
        result.push_back(di::Tuple { type_name.to_owned(), di::move(section_content) });
        return response;
    }

    template<di::concepts::ReflectableToEnumerators T, typename M = di::Reflect<T>>
    static auto operator()(di::InPlaceType<T>, di::Vector<di::Tuple<di::String, di::String>>& result, T const&)
        -> di::String {
        auto response = markdown_type(di::in_place_type<T>);
        constexpr auto type_name = di::container::fixed_string_to_utf8_string_view<M::name>();
        if (di::contains(result | di::keys, type_name)) {
            return response;
        }
        auto section_content = di::format("### {}\n\n"_sv, type_name);
        if (!M::description.empty()) {
            section_content +=
                di::format("{}.\n\n"_sv, di::container::fixed_string_to_utf8_string_view<M::description>());
        }
        section_content += "| Value | Description |\n"_sv;
        section_content += "| - | - |\n"_sv;
        di::tuple_for_each(
            [&]<typename E>(E) {
                constexpr auto enum_name = di::container::fixed_string_to_utf8_string_view<E::name>();
                constexpr auto description = di::container::fixed_string_to_utf8_string_view<E::description>();
                section_content += di::format("| {} | {} |\n"_sv, enum_name, description);
            },
            M {});
        result.push_back(di::Tuple { type_name.to_owned(), di::move(section_content) });
        return response;
    }

    template<di::concepts::ReflectableToFields T, typename M = di::Reflect<T>>
    static auto operator()(di::InPlaceType<T>, di::Vector<di::Tuple<di::String, di::String>>& result, T const& defaults)
        -> di::String {
        auto response = markdown_type(di::in_place_type<T>);
        constexpr auto type_name = di::container::fixed_string_to_utf8_string_view<M::name>();
        if (di::contains(result | di::keys, type_name)) {
            return response;
        }
        auto const index = result.size();
        result.emplace_back(type_name.to_owned(), ""_s);
        auto section_content = di::format("### {}\n\n"_sv, type_name);
        if (!M::description.empty()) {
            section_content +=
                di::format("{}.\n\n"_sv, di::container::fixed_string_to_utf8_string_view<M::description>());
        }
        section_content += "| Field | Type | Default | Description |\n"_sv;
        section_content += "| - | - | - | - |\n"_sv;
        di::tuple_for_each(
            [&]<typename F>(F field) {
                constexpr auto field_name = di::container::fixed_string_to_utf8_string_view<F::name>();
                if constexpr (field_name.starts_with(U'$') || field_name == "version"_sv) {
                    return;
                } else {
                    constexpr auto description = di::container::fixed_string_to_utf8_string_view<F::description>();
                    auto type = BuildMarkdownTable::operator()(di::in_place_type<typename F::Type>, result,
                                                               field.get(defaults));
                    section_content += di::format("| {} | {} | {} | {}. |\n"_sv, field_name, type,
                                                  markdown_default(field.get(defaults)), description);
                }
            },
            M {});
        section_content.push_back(U'\n');
        di::get<1>(result[index]) = di::move(section_content);
        return response;
    }
};

constexpr inline auto build_markdown_table = BuildMarkdownTable {};

auto markdown_docs() -> di::String {
    auto result = R"md(# Configuration

ttx configuration is done via JSON configuration files stored in `$XDG_CONFIG_HOME/ttx`.
Each individual configuration file created can be loaded as a "profile" by passing `--profile=$PROFILE` to ttx new.
The default profile is "main" so your first configuration file should be `$XDG_CONFIG_HOME/ttx/main.json`.

To share configuration across multiple profiles use the "extends" field in the configuration file.
This lets you override or combine as many other configuration files as you want into the final loaded configuration.
To see the configuration file after handling "extends", use `ttx config show --profile "$PROFILE"`

The configuration file is fully specified by a JSON schema file.
When defining your configuration, you can include `"$schema": "https://github.com/coletrammer/ttx/raw/refs/heads/main/meta/schema/config.json"`
to get auto-completions in your text editor.

You can also start your configuration from the defaults by running:

```sh
mkdir -p ~/.config/ttx
ttx config show > ~/.config/ttx/main.json
```

## Nix Home Manager

When using home manager, use the settings field to define your configuration files.
The top-level key is the profile name, followed by the exact same keys as the JSON configuration file.
You can also define your configuration in JSON and load it directly into nix. You will still get the benefits of build time validation of the configuration file:

```nix
programs.ttx = {
  enable = true;
  settings = {
    main = builtins.fromJSON (builtins.readFile ./ttx-main.json);
    super = builtins.fromJSON (builtins.readFile ./ttx-super.json);
  };
};
```

## Configuration Format

Each table below corresponds to a JSON object in the ttx configuration. The first object is the top-level
config. Each field has a corresponding type, which for objects will link to the documentation for that JSON
sub-object.

)md"_s;

    auto sections = di::Vector<di::Tuple<di::String, di::String>> {};
    auto defaults = config_with_defaults(Config {});
    defaults.extends.emplace();
    build_markdown_table(di::in_place_type<Config>, sections, defaults);
    for (auto const& [_, content] : sections) {
        result += content;
    }

    result += R"md(## Default JSON Configuration

This JSON block contains the default JSON configuration used by ttx:

```json
)md"_sv;
    auto string = *di::to_json_string(defaults);
    auto json = *di::from_json_string<di::json::Object>(string);
    strip_empty_objects(json);
    result += *di::to_json_string(json, di::JsonSerializerConfig().pretty().indent_width(4));
    result += "\n```\n"_sv;
    return result;
}

void strip_empty_objects(di::json::Object& object) {
    auto keys_to_remove = di::Vector<di::String> {};
    for (auto& [key, value] : object) {
        for (auto& o : value.as_object()) {
            strip_empty_objects(o);
            if (o.empty()) {
                keys_to_remove.push_back(key.clone());
            }
        }
    }
    for (auto const& key : keys_to_remove) {
        object.erase(key);
    }
}
}
