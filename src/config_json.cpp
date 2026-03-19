#include "config_json.h"

#include <di/reflect/reflect.h>
#include <di/serialization/json_serializer.h>

#include "config.h"
#include "di/container/string/conversion.h"
#include "di/container/tree/tree_set.h"
#include "di/serialization/json_deserializer.h"
#include "di/util/construct.h"
#include "di/vocab/error/string_error.h"
#include "dius/print.h"
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
};

constexpr inline auto convert_value = ConvertValue {};

struct DoUnconvert {
    template<di::concepts::ReflectableToFields JsonConfig, di::concepts::ReflectableToFields Config>
    static auto operator()(JsonConfig& json_config, Config&& config) {
        di::tuple_for_each(
            [&](auto json_field) {
                di::tuple_for_each(
                    [&](auto field) {
                        if constexpr (json_field.name == field.name) {
                            DoUnconvert::operator()(json_field.get(json_config), di::move(field.get(config)));
                        }
                    },
                    di::reflect(config));
            },
            di::reflect(json_config));
    }

    template<di::concepts::Optional T, typename U>
    static auto operator()(T& maybe_json_value, U&& value) {
        if constexpr (di::concepts::SameAs<di::TransparentString, di::meta::RemoveCVRef<U>>) {
            if (value.empty()) {
                return;
            }
        }
        maybe_json_value = convert_value(di::in_place_type<di::meta::OptionalValue<T>>, di::forward<U>(value));
    }
};

constexpr inline auto do_unconvert = DoUnconvert {};

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
    auto result = ttx::Config {};
    do_convert(result, di::move(json_config));
    return result;
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
    for (auto const& name : config_json.extends) {
        auto name_ts = di::to_transparent_string(name);
        auto subconfig = TRY(load_config_recursively(search_paths, visited, name_ts));
        merge(subconfig, di::move(config_json));
        config_json = di::move(subconfig);
    }
    return config_json;
}

auto resolve_profile(di::TransparentStringView profile, Config&& cli_config) -> di::Result<ttx::Config> {
    auto const profile_name = di::PathView(profile).stem().value();
    auto const search_paths = TRY(get_search_paths());
    auto visited = di::TreeSet<di::TransparentString> {};
    auto config_json = TRY(load_config_recursively(search_paths.span(), visited, profile));
    merge(config_json, di::move(cli_config));
    return convert(profile_name, di::move(config_json));
}

auto to_config_json(ttx::Config&& config) -> Config {
    auto result = Config {};
    do_unconvert(result, di::move(config));
    return result;
}

struct BuidlJsonSchemaDefinitions {
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

    template<typename T>
    static auto operator()(di::InPlaceType<di::Optional<T>>, di::TreeMap<di::String, di::json::Object>& output,
                           di::Optional<T> const& defaults) -> di::json::Object {
        if (!defaults.has_value()) {
            return BuidlJsonSchemaDefinitions::operator()(di::in_place_type<T>, output, T {});
        }
        return BuidlJsonSchemaDefinitions::operator()(di::in_place_type<T>, output, defaults.value());
    }

    template<typename T>
    static auto operator()(di::InPlaceType<di::Vector<T>>, di::TreeMap<di::String, di::json::Object>& output,
                           di::Vector<T> const&) -> di::json::Object {
        auto response = di::json::Object {};
        response["type"_sv] = "array"_s;
        response["items"_sv] = BuidlJsonSchemaDefinitions::operator()(di::in_place_type<T>, output, T());
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
                    auto o = BuidlJsonSchemaDefinitions::operator()(di::in_place_type<typename F::Type>, output,
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

constexpr inline auto build_json_schema_definitions = BuidlJsonSchemaDefinitions {};

auto json_schema() -> di::json::Object {
    auto definitions = di::TreeMap<di::String, di::json::Object> {};
    auto const defaults = to_config_json(ttx::Config {});
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
    examples.emplace_back(*di::from_json_string<di::json::Object>(default_as_json_string.view()));
    o["examples"_sv] = di::move(examples);

    return di::move(o);
}
}
