#include "config_nix.h"

#include "config_json.h"
#include "dius/print.h"

namespace ttx {
auto main(ConfigNix&) -> di::Result<> {
    auto string = config_json::v1::nix_options();
    dius::println("{}"_sv, string);
    return {};
}
}
