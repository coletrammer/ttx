#include "config_schema.h"

#include "config_json.h"
#include "dius/print.h"

namespace ttx {
auto main(ConfigSchema&) -> di::Result<> {
    auto string =
        *di::to_json_string(config_json::v1::json_schema(), di::JsonSerializerConfig().pretty().indent_width(4));
    dius::println("{}"_sv, string);
    return {};
}
}
