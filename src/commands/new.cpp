#include "new.h"

namespace ttx {
auto main(New& args) -> di::Result<> {
    return main(static_cast<NewBase&>(args));
}
}
