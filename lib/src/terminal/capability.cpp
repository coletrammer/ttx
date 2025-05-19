#include "ttx/terminal/capability.h"

#include "capabilities.h"
#include "di/container/algorithm/binary_search.h"
#include "di/container/algorithm/sort.h"
#include "di/container/algorithm/unique.h"
#include "di/format/prelude.h"
#include "ttx/terminal/escapes/terminfo_string.h"

namespace ttx::terminal {
auto Capability::serialize() const -> di::String {
    return di::visit(di::overload(
                         [&](di::Void) -> di::String {
                             // Boolean capability
                             return *di::present("{}"_sv, short_name.to_owned());
                         },
                         [&](u32 value) {
                             // Numeric capability
                             return *di::present("{}#{}"_sv, short_name, value);
                         },
                         [&](di::TransparentStringView value) {
                             // String capability
                             return *di::present("{}={}"_sv, short_name, value);
                         }),
                     value);
}

auto Terminfo::serialize() const -> di::String {
    auto result = ""_s;

    // Name line:
    result += names | di::transform(di::to_string) | di::join_with(U'|');
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

auto lookup_terminfo_string(di::StringView hex_name) -> TerminfoString {
    constexpr auto enabled_capabilities_count = [] {
        return usize(di::distance(ttx_terminfo.capabilities | di::filter(&Capability::enabled)));
    }();

    constexpr auto lookup_table = [] {
        auto result = di::Array<Capability, enabled_capabilities_count + 3> {};

        // Special capabilities supported by xterm, but not terminfo file.
        result[0] = Capability {
            .long_name = {},
            .short_name = "Co"_tsv,
            .value = 16u, // TODO: update to 256 after supporting 256 color palette
            .description = {},
        };
        result[1] = Capability {
            .long_name = {},
            .short_name = "TN"_tsv,
            .value = ttx_terminfo.names[0],
            .description = {},
        };
        result[2] = Capability {
            .long_name = {},
            .short_name = "RGB"_tsv,
            .description = {},
        };

        di::copy(ttx_terminfo.capabilities | di::filter(&Capability::enabled), result.begin() + 3);
        di::sort(result, di::compare, &Capability::short_name);
        return result;
    }();

    auto name = TerminfoString::unhex(hex_name);
    if (!name) {
        return {};
    }

    auto result = di::binary_search(lookup_table, name.value(), di::compare, &Capability::short_name);
    if (!result.found) {
        return {};
    }
    return TerminfoString::from_capability(*result.in);
}

consteval static auto validate_terminfo(Terminfo const& terminfo) -> bool {
    auto short_names = terminfo.capabilities | di::transform(&Capability::short_name) | di::to<di::Vector>();
    di::sort(short_names);
    return di::unique(short_names).end() == short_names.end();
}

static_assert(validate_terminfo(ttx_terminfo));
}
