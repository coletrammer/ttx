#pragma once

#include "di/util/strong_int.h"

namespace ttx {
namespace detail {
    struct PaneIdTag {
        using Type = u64;
    };
}

using PaneId = di::StrongInt<detail::PaneIdTag>;
}
