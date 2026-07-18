#include "paths.h"

#include "dius/system/process.h"

namespace ttx {
auto get_session_save_dir() -> di::Result<di::Path> {
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

auto get_runtime_dir() -> di::Path {
    auto const& env = dius::system::get_environment();
    auto xdg_runtime_dir = env.at("XDG_RUNTIME_DIR"_tsv)
                               .transform([&](di::TransparentStringView path) {
                                   return di::PathView(path);
                               })
                               .value_or("/tmp"_pv)
                               .to_owned();
    return di::move(xdg_runtime_dir) / "ttx"_tsv;
}

auto get_local_terminfo_dir() -> di::Result<di::Path> {
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
}
