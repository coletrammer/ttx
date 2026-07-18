#include "replay.h"

namespace ttx {
auto main(Replay& args) -> di::Result<> {
    args.hide_status_bar = true;
    args.disable_layout_save = true;
    args.disable_layout_restore = true;
    return main(static_cast<NewBase&>(args));
}
}
