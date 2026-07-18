#include "terminfo.h"

#include "dius/print.h"
#include "ttx/terminal/capability.h"

namespace ttx {
auto main(Terminfo& args) -> di::Result<> {
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
}
