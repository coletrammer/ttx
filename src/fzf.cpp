#include "fzf.h"

#include "di/util/construct.h"
#include "ttx/pane.h"
#include "ttx/popup.h"

namespace ttx {
auto Fzf::popup_args() && -> di::Tuple<CreatePaneArgs, PopupLayout> {
    // Setup the pipes for fzf.
    auto create_pane_args = CreatePaneArgs {
        .pipe_input = m_input | di::join_with(U'\n') | di::to<di::String>(),
        .pipe_output = true,
    };

    // Build the command string based on the configuration. For now, most
    // fields are hard-coded.
    create_pane_args.command = di::Array {
        "fzf"_ts,    "--border"_ts,     "--layout"_ts,   "reverse"_ts,
        "--info"_ts, "inline-right"_ts, "--no-multi"_ts, "--cycle"_ts,
    } | di::to<di::Vector>();
    if (m_prompt) {
        create_pane_args.command.push_back("--prompt"_ts);

        // Add '> ' as a prompt indicator
        auto prompt_string = *di::present("{}> "_sv, m_prompt.value());
        create_pane_args.command.push_back(prompt_string.span() | di::transform(di::construct<char>) |
                                           di::to<di::TransparentString>());
    }
    if (m_title) {
        create_pane_args.command.push_back("--border-label"_ts);

        // Add spacing for padding
        auto label_string = *di::present(" {} "_sv, m_title.value());
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

    return { di::move(create_pane_args), m_layout };
}
}
