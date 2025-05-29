#include "ttx/terminal/escapes/osc_133.h"

#include "di/format/prelude.h"
#include "di/serialization/percent_encoded.h"
#include "di/util/construct.h"
#include "ttx/escape_sequence_parser.h"

namespace ttx::terminal {
static auto prompt_kinds = di::Array {
    di::Tuple { PromptKind::Initial, U'i' },
    di::Tuple { PromptKind::Secondary, U's' },
    di::Tuple { PromptKind::Continuation, U'c' },
    di::Tuple { PromptKind::Right, U'r' },
};

static auto prompt_click_modes = di::Array {
    di::Tuple { PromptClickMode::Line, "line"_sv },
    di::Tuple { PromptClickMode::MultipleLeftRight, "m"_sv },
    di::Tuple { PromptClickMode::MultipleUpDownConservative, "w"_sv },
    di::Tuple { PromptClickMode::MultipleUpDownConservative, "v"_sv },
};

static auto find_prompt_kind(PromptKind kind) -> c32 {
    for (auto const& [v, r] : prompt_kinds) {
        if (v == kind) {
            return r;
        }
    }
    return 0;
}

static auto lookup_prompt_kind(c32 s) -> PromptKind {
    for (auto const& [v, r] : prompt_kinds) {
        if (s == r) {
            return v;
        }
    }

    // Default value - we're trying to be as lenient as possible when parsing, to match
    // the specifications intent.
    return PromptKind::Initial;
}

static auto find_click_mode(PromptClickMode mode) -> di::StringView {
    for (auto const& [v, r] : prompt_click_modes) {
        if (v == mode) {
            return r;
        }
    }
    return ""_sv;
}

static auto lookup_click_mode(di::StringView s) -> PromptClickMode {
    for (auto const& [v, r] : prompt_click_modes) {
        if (s == r) {
            return v;
        }
    }

    // Per the spec, we can always assume line regardless of the actual value is.
    return PromptClickMode::Line;
}

auto OSC133::parse(di::StringView data) -> di::Optional<OSC133> {
    auto parts = data | di::split(U';') | di::to<di::Vector>();
    if (parts.empty()) {
        return {};
    }
    if (parts[0].size_code_units() != 1) {
        return {};
    }
    switch (*parts[0].front()) {
        case 'A': {
            auto result = BeginPrompt();
            for (auto option : parts | di::drop(1)) {
                auto equal = option.find('=');
                if (!equal) {
                    continue;
                }
                auto name = option.substr(option.begin(), equal.begin());
                auto value = option.substr(equal.end());
                if (name == "aid"_sv) {
                    result.application_id = value.to_owned();
                } else if (name == "cl"_sv) {
                    result.click_mode = lookup_click_mode(value);
                } else if (name == "k"_sv) {
                    if (value.size_code_units() == 1) {
                        result.kind = lookup_prompt_kind(*value.front());
                    }
                } else if (name == "redraw"_sv) {
                    if (value == "0"_sv) {
                        result.redraw = false;
                    }
                }
            }
            return OSC133(di::move(result));
        }
        case 'B':
            return OSC133(EndPrompt());
        case 'C':
            return OSC133(EndInput());
        case 'D': {
            auto result = EndCommand();
            if (parts.size() > 1) {
                auto exit_code_string = parts[1];
                auto exit_code = di::parse<u32>(exit_code_string);
                if (!exit_code) {
                    return {};
                }
                result.exit_code = exit_code.value();
            }
            for (auto option : parts | di::drop(2)) {
                auto equal = option.find('=');
                if (!equal) {
                    continue;
                }
                auto name = option.substr(option.begin(), equal.begin());
                auto value = option.substr(equal.end());
                if (name == "aid"_sv) {
                    result.application_id = value.to_owned();
                } else if (name == "err"_sv) {
                    result.error = value.to_owned();
                }
            }
            return OSC133(di::move(result));
        }
        default:
            return {};
    }
}

auto OSC133::serialize() const -> di::String {
    return di::visit(
        di::overload(
            [](BeginPrompt const& value) -> di::String {
                auto aid = value.application_id.empty() ? ""_s : *di::present(";aid={}"_sv, value.application_id);
                auto cl = value.click_mode == PromptClickMode::None
                              ? ""_s
                              : *di::present(";cl={}"_sv, find_click_mode(value.click_mode));
                auto redraw = value.redraw ? ""_s : ";redraw=0"_s;
                return *di::present("\033]133;A;k={}{}{}{}\033\\"_sv, find_prompt_kind(value.kind), aid, cl, redraw);
            },
            [](EndPrompt const&) -> di::String {
                return "\033]133;B\033\\"_s;
            },
            [](EndInput const&) -> di::String {
                return "\033]133;C\033\\"_s;
            },
            [](EndCommand const& value) -> di::String {
                auto err = value.error.empty() ? ""_s : *di::present(";err={}"_sv, value.error);
                auto aid = value.application_id.empty() ? ""_s : *di::present(";aid={}"_sv, value.application_id);
                return *di::present("\033]133;D;{}{}{}\033\\"_sv, value.exit_code, err, aid);
            }),
        command);
}
}
