#pragma once

#include "ttx/pane.h"
#include "ttx/popup.h"

namespace ttx {
class Fzf {
public:
    auto with_prompt(di::String prompt) && -> Fzf {
        m_prompt = di::move(prompt);
        return di::move(*this);
    }

    auto with_input(di::Vector<di::String> input) && -> Fzf {
        m_input = di::move(input);
        return di::move(*this);
    }

    auto with_title(di::String title) && -> Fzf {
        m_title = di::move(title);
        return di::move(*this);
    }

    auto popup_args() && -> di::Tuple<CreatePaneArgs, PopupLayout>;

private:
    di::Optional<di::String> m_prompt;
    di::Optional<di::String> m_title;
    di::Vector<di::String> m_input;
};
}
