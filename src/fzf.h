#pragma once

#include "colors.h"
#include "config.h"
#include "theme.h"
#include "ttx/pane.h"
#include "ttx/popup.h"
#include "ttx/terminal/palette.h"

namespace ttx {
class FzfCommand {
public:
    static void update_palette_with_indirect_fzf_colors(terminal::Palette& palette, FzfColors const& colors);

    auto with_prompt(di::String prompt) && -> FzfCommand {
        m_prompt = di::move(prompt);
        return di::move(*this);
    }

    auto with_input(di::Vector<di::String> input) && -> FzfCommand {
        m_input = di::move(input);
        return di::move(*this);
    }

    auto with_title(di::String title) && -> FzfCommand {
        m_title = di::move(title);
        return di::move(*this);
    }

    auto with_query(di::String query) && -> FzfCommand {
        m_query = di::move(query);
        return di::move(*this);
    }

    auto with_no_info(bool no_info = true) && -> FzfCommand {
        m_no_info = no_info;
        return di::move(*this);
    }

    auto with_no_separator(bool no_separator = true) && -> FzfCommand {
        m_no_separator = no_separator;
        return di::move(*this);
    }

    auto with_print_query(bool print_query = true) && -> FzfCommand {
        m_print_query = print_query;
        return di::move(*this);
    }

    auto with_alignment(PopupAlignment alignment) && -> FzfCommand {
        m_layout.alignment = alignment;
        return di::move(*this);
    }

    auto with_width(PopupSize width) && -> FzfCommand {
        m_layout.width = width;
        return di::move(*this);
    }

    auto with_height(PopupSize height) && -> FzfCommand {
        m_layout.height = height;
        return di::move(*this);
    }

    auto with_query_as_extra_output(bool query_as_extra_output = true) && -> FzfCommand {
        m_query_as_extra_output = query_as_extra_output;
        return di::move(*this);
    }

    auto with_selection_as_extra_output(bool selection_as_extra_output = true) && -> FzfCommand {
        m_selection_as_extra_output = selection_as_extra_output;
        return di::move(*this);
    }

    auto with_preview_command(di::TransparentString command) && -> FzfCommand {
        m_preview_command = di::move(command);
        return di::move(*this);
    }

    auto with_indirect_color_palette(bool indirect_color_palette = true) && -> FzfCommand {
        m_with_indirect_color_palette = indirect_color_palette;
        return di::move(*this);
    }

    /// @brief Convience method which configures fzf like a text box
    auto as_text_box() && -> FzfCommand {
        return di::move(*this)
            .with_alignment(PopupAlignment::Top)
            .with_height(AbsoluteSize(3))
            .with_no_info()
            .with_no_separator()
            .with_print_query();
    }

    auto popup_args(CreatePaneArgs&& base, FzfConfig const& config) && -> di::Tuple<CreatePaneArgs, PopupLayout>;

private:
    di::Optional<di::String> m_prompt;
    di::Optional<di::String> m_title;
    di::Optional<di::String> m_query;
    di::Optional<di::TransparentString> m_preview_command;
    di::Vector<di::String> m_input;
    PopupLayout m_layout;
    bool m_no_info { false };
    bool m_no_separator { false };
    bool m_print_query { false };
    bool m_query_as_extra_output { false };
    bool m_selection_as_extra_output { false };
    bool m_with_indirect_color_palette { false };
};
}
