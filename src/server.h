#pragma once

#include "di/container/path/prelude.h"
#include "di/vocab/error/prelude.h"

namespace ttx {
auto run_server(di::Path path) -> di::Result<>;
}
