#include "attach.h"

namespace ttx {
auto main(Attach& args) -> di::Result<> {
    return main(static_cast<RunClientBase&>(args));
}
}
