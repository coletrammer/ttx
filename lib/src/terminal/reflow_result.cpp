#include "ttx/terminal/reflow_result.h"

#include "di/assert/prelude.h"
#include "di/container/algorithm/lower_bound.h"
#include "ttx/terminal/absolute_position.h"

namespace ttx::terminal {
void ReflowResult::add_offset(AbsolutePosition position, i64 dr, i32 dc) {
    if (!m_ranges.empty()) {
        ASSERT_LT(m_ranges.back().value().position, position);
    }
    m_ranges.push_back({ position, dr, dc });
}

void ReflowResult::merge(ReflowResult&& other) {
    if (other.m_ranges.empty()) {
        return;
    }
    if (this->m_ranges.empty()) {
        *this = di::move(other);
        return;
    }

    auto this_last_position = this->m_ranges.back().value().position;
    auto other_first_position = other.m_ranges.front().value().position;
    if (this_last_position > other_first_position) {
        // In this case, we are applying the row offset from the other range to all our entries.
        for (auto& range : this->m_ranges) {
            range.dr += other.m_ranges.back().value().dr;
        }
        this->m_ranges.insert_container(this->m_ranges.begin(), other.m_ranges);
    } else {
        // In this case, we are applying the row offset from the this range to all the entries from other.
        for (auto& range : other.m_ranges) {
            this->m_ranges.push_back({
                range.position,
                range.dr + this->m_ranges.back().value().dr,
                range.dc,
            });
        }
    }
}

auto ReflowResult::map_position(AbsolutePosition position) const -> AbsolutePosition {
    if (m_ranges.empty()) {
        return position;
    }
    auto const* it = di::lower_bound(m_ranges, position, di::compare, &ReflowRange::position);
    if (it->position > position) {
        if (it == m_ranges.begin()) {
            return position;
        }
        --it;
    }
    return {
        position.row + it->dr,
        position.col + it->dc,
    };
}
}
