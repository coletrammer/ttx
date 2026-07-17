#pragma once

#include "di/util/strong_int.h"

namespace ttx {
namespace detail {
    struct WorkspaceIdTag {
        using Type = u64;
    };

    struct TabIdTag {
        using Type = u64;
    };
}

using WorkspaceId = di::StrongInt<detail::WorkspaceIdTag>;
using TabId = di::StrongInt<detail::TabIdTag>;
}
