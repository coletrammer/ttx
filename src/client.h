#pragma once

#include "config.h"
#include "ttx/features.h"

namespace ttx {
auto run_client(Config config, Feature features) -> di::Result<>;
}
