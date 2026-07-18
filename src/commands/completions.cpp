#include "completions.h"

#include "args.h"
#include "dius/print.h"

namespace ttx {
auto main(Completions& args) -> di::Result<> {
    auto parser = di::get_cli_parser<Args>();
    parser.write_completions(dius::std_out, args.shell);
    return {};
}
}
