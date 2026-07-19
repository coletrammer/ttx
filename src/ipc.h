#pragma once

#include "create_pane_args.h"
#include "di/container/string/prelude.h"
#include "di/reflect/prelude.h"
#include "di/vocab/expected/prelude.h"
#include "ttx/ipc/pane_id.h"

namespace ttx::ipc {
struct CreatePane {
    struct Reply {
        di::Expected<PaneId, di::String> pane_id;

        constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<Reply>) {
            return di::make_fields<"Reply">(di::field<"pane_id", &CreatePane::pane_id>);
        }
    };

    di::Optional<PaneId> pane_id;
    CreatePaneArgs args;

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<CreatePane>) {
        return di::make_fields<"CreatePane">(di::field<"pane_id", &CreatePane::pane_id>,
                                             di::field<"modifiers", &CreatePane::args>);
    }
};
}
