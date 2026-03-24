#include "fzf.h"

#include "di/container/string/conversion.h"
#include "di/util/construct.h"
#include "theme.h"
#include "ttx/pane.h"
#include "ttx/popup.h"
#include "ttx/terminal/color.h"

namespace ttx {
static auto to_fzf_color(terminal::Color color) -> di::TransparentString {
    if (color.is_default() || color.is_dynamic()) {
        return "-1"_ts;
    }
    if (color.is_palette()) {
        auto s = di::to_string(color.r);
        return di::to_transparent_string(s);
    }
    auto s = color.to_string();
    return di::to_transparent_string(s);
}

auto FzfCommand::popup_args(CreatePaneArgs&& base,
                            FzfConfig const& config) && -> di::Tuple<CreatePaneArgs, PopupLayout> {
    auto const& colors = config.colors;

    // Setup the pipes for fzf.
    auto create_pane_args = di::move(base);
    create_pane_args.pipe_input = m_input | di::join_with(U'\n') | di::to<di::String>();
    create_pane_args.pipe_output = true;

    // Build the command string based on the configuration. For now, most
    // fields are hard-coded.
    create_pane_args.command = di::Array {
        "fzf"_ts,          "--border"_ts,   "--layout"_ts, "reverse"_ts,        "--info"_ts,
        "inline-right"_ts, "--no-multi"_ts, "--cycle"_ts,  "--no-scrollbar"_ts,
    } | di::to<di::Vector>();
    if (m_prompt) {
        create_pane_args.command.push_back("--prompt"_ts);

        // Add '> ' as a prompt indicator
        auto prompt_string = di::format("{}> "_sv, m_prompt.value());
        create_pane_args.command.push_back(prompt_string.span() | di::transform(di::construct<char>) |
                                           di::to<di::TransparentString>());
    }
    if (m_title) {
        create_pane_args.command.push_back("--border-label"_ts);

        // Add spacing for padding
        auto label_string = di::format(" {} "_sv, m_title.value());
        create_pane_args.command.push_back(label_string.span() | di::transform(di::construct<char>) |
                                           di::to<di::TransparentString>());
    }
    if (m_query) {
        create_pane_args.command.push_back("--query"_ts);
        create_pane_args.command.push_back(m_query.value().span() | di::transform(di::construct<char>) |
                                           di::to<di::TransparentString>());
    }
    if (m_no_info) {
        create_pane_args.command.push_back("--no-info"_ts);
    }
    if (m_no_separator) {
        create_pane_args.command.push_back("--no-separator"_ts);
    }
    if (m_print_query) {
        create_pane_args.command.push_back("--print-query"_ts);
    }
    if (m_query_as_extra_output || m_selection_as_extra_output) {
        create_pane_args.pipe_extra_output = true;
    }
    if (m_query_as_extra_output) {
        create_pane_args.command.push_back("--bind"_ts);
        create_pane_args.command.push_back("change:execute-silent(echo {q} >&3)"_ts);
    }
    if (m_selection_as_extra_output) {
        create_pane_args.command.push_back("--bind"_ts);
        create_pane_args.command.push_back("focus:execute-silent(echo {} >&3)"_ts);
    }
    if (m_preview_command) {
        create_pane_args.command.push_back("--preview"_ts);
        create_pane_args.command.push_back(m_preview_command.value().clone());
        create_pane_args.command.push_back("--preview-window"_ts);
        create_pane_args.command.push_back("border-left"_ts);
    }

    auto color_arg = "--color="_ts;
    auto index = 0_u8;
    di::tuple_for_each(
        [&]<typename F>(F field) {
            constexpr auto field_name = di::container::fixed_string_to_utf8_string_view<F::name>();
            if (index > 0) {
                color_arg.push_back(',');
            }
            color_arg += di::to_transparent_string(field_name);
            color_arg.push_back(':');
            if (m_with_indirect_color_palette) {
                color_arg += to_fzf_color(
                    terminal::Color(terminal::Color::Palette(u8(terminal::PaletteIndex::StaticEnd) - index)));
            } else {
                color_arg += to_fzf_color(field.get(colors));
            }
            index++;
        },
        di::reflect(colors));
    create_pane_args.command.push_back(di::move(color_arg));

    return { di::move(create_pane_args), m_layout };
}

void FzfCommand::update_palette_with_indirect_fzf_colors(terminal::Palette& palette, FzfColors const& colors) {
    auto index = 0_u8;
    di::tuple_for_each(
        [&]<typename F>(F field) {
            constexpr auto field_name = di::container::fixed_string_to_utf8_string_view<F::name>();

            auto color = field.get(colors);
            if (field_name.starts_with("bg"_sv)) {
                color = palette.resolve_background(color);
            } else {
                color = palette.resolve_foreground(color);
            }
            palette.set(terminal::PaletteIndex(u8(terminal::PaletteIndex::StaticEnd) - index), color);
            index++;
        },
        di::reflect(colors));
}
}
