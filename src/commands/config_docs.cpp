#include "config_docs.h"

#include "config_json.h"
#include "dius/print.h"

namespace ttx {
auto main(ConfigDocs&) -> di::Result<> {
    auto string = config_json::v1::markdown_docs();
    dius::println("{}"_sv, string);
    return {};
}
}
