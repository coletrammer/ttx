#pragma once

#include "di/container/path/prelude.h"
#include "di/vocab/error/result.h"

namespace ttx {
auto get_session_save_dir() -> di::Result<di::Path>;
auto get_runtime_dir() -> di::Path;
auto get_local_terminfo_dir() -> di::Result<di::Path>;
}
