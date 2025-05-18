#include "ttx/terminal/capability.h"

#include "capabilities.h"
#include "di/container/tree/tree_set.h"
#include "di/format/prelude.h"

namespace ttx::terminal {
auto Capability::serialize() const -> di::String {
    return di::visit(di::overload(
                         [&](di::Void) -> di::String {
                             // Boolean capability
                             return short_name.to_owned();
                         },
                         [&](u32 value) {
                             // Numeric capability
                             return *di::present("{}#{}"_sv, short_name, value);
                         },
                         [&](di::StringView value) {
                             // String capability
                             return *di::present("{}={}"_sv, short_name, value);
                         }),
                     value);
}

auto Terminfo::serialize() const -> di::String {
    auto result = ""_s;

    // Name line:
    result += di::join_with(names, U'|');
    result += ",\n"_sv;

    // Each capability goes on a separate line
    for (auto const& capability : capabilities | di::filter(&Capability::enabled)) {
        result += *di::present("\t{},\n"_sv, capability.serialize());
    }

    return result;
}

auto get_ttx_terminfo() -> Terminfo const& {
    return ttx_terminfo;
}

consteval static auto validate_terminfo(Terminfo const& terminfo) -> bool {
    auto short_names = di::TreeSet<di::StringView> {};
    for (auto const& capability : terminfo.capabilities) {
        if (short_names.contains(capability.short_name)) {
            return false;
        }
        short_names.insert(capability.short_name);
    }
    return true;
}

static_assert(validate_terminfo(ttx_terminfo));
}
