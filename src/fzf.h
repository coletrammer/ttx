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

    auto with_query(di::String query) && -> Fzf {
        m_query = di::move(query);
        return di::move(*this);
    }

    auto with_no_info(bool no_info = true) && -> Fzf {
        m_no_info = no_info;
        return di::move(*this);
    }

    auto with_no_separator(bool no_separator = true) && -> Fzf {
        m_no_separator = no_separator;
        return di::move(*this);
    }

    auto with_print_query(bool print_query = true) && -> Fzf {
        m_print_query = print_query;
        return di::move(*this);
    }

    auto with_alignment(PopupAlignment alignment) && -> Fzf {
        m_layout.alignment = alignment;
        return di::move(*this);
    }

    auto with_width(PopupSize width) && -> Fzf {
        m_layout.width = width;
        return di::move(*this);
    }

    auto with_height(PopupSize height) && -> Fzf {
        m_layout.height = height;
        return di::move(*this);
    }

    /// @brief Convience method which configures fzf like a text box
    auto as_text_box() && -> Fzf {
        return di::move(*this)
            .with_alignment(PopupAlignment::Top)
            .with_height(AbsoluteSize(3))
            .with_no_info()
            .with_no_separator()
            .with_print_query();
    }

    auto popup_args(CreatePaneArgs&& base) && -> di::Tuple<CreatePaneArgs, PopupLayout>;

private:
    di::Optional<di::String> m_prompt;
    di::Optional<di::String> m_title;
    di::Optional<di::String> m_query;
    di::Vector<di::String> m_input;
    PopupLayout m_layout;
    bool m_no_info { false };
    bool m_no_separator { false };
    bool m_print_query { false };
};
}
