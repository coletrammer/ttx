#include "ttx/layout.h"

#include "di/container/algorithm/count_if.h"
#include "di/container/interface/erase.h"
#include "di/container/view/as_rvalue.h"
#include "di/function/overload.h"
#include "di/util/scope_exit.h"
#include "di/vocab/pointer/box.h"
#include "di/vocab/variant/get_if.h"
#include "di/vocab/variant/holds_alternative.h"
#include "ttx/layout_json.h"

namespace ttx {
struct FindPaneInLayoutGroup {
    static auto operator()(LayoutGroup* parent, usize index, Pane* target, di::Box<LayoutPane> const& pane)
        -> di::Tuple<LayoutGroup*, usize> {
        if (pane->pane.get() == target) {
            return { parent, index };
        }
        return {};
    }

    static auto operator()(LayoutGroup* parent, usize index, Pane* target, di::Box<LayoutGroup> const& group)
        -> di::Tuple<LayoutGroup*, usize> {
        return FindPaneInLayoutGroup::operator()(parent, index, target, group.get());
    }

    static auto operator()(LayoutGroup*, usize, Pane* target, LayoutGroup* group) -> di::Tuple<LayoutGroup*, usize> {
        for (auto [i, child] : di::enumerate(group->m_children)) {
            auto [parent, index] = di::visit(
                [&](auto& c) {
                    return FindPaneInLayoutGroup {}(group, i, target, c);
                },
                child);
            if (parent) {
                return { parent, index };
            }
        }
        return {};
    }
};

struct GetRelativeSize {
    static auto operator()(di::Box<LayoutPane> const& pane) -> i64& { return pane->relative_size; }

    static auto operator()(di::Box<LayoutGroup> const& group) -> i64& {
        return GetRelativeSize::operator()(group.get());
    }

    static auto operator()(LayoutGroup* group) -> i64& { return group->relative_size(); }
};

auto LayoutNode::find_pane(Pane* pane) -> di::Optional<LayoutEntry&> {
    for (auto& child : children) {
        auto result = di::visit(di::overload(
                                    [&](LayoutEntry& entry) -> di::Optional<LayoutEntry&> {
                                        if (entry.pane == pane) {
                                            return entry;
                                        }
                                        return {};
                                    },
                                    [&](di::Box<LayoutNode>& node) -> di::Optional<LayoutEntry&> {
                                        return node->find_pane(pane);
                                    }),
                                child);
        if (result) {
            return result;
        }
    }
    return {};
}

auto LayoutNode::hit_test(u32 row, u32 col) -> di::Optional<LayoutEntry&> {
    if (row >= this->row + size.rows || col >= this->col + size.cols) {
        return {};
    }
    for (auto& child : children) {
        auto result = di::visit(di::overload(
                                    [&](LayoutEntry& entry) -> di::Optional<LayoutEntry&> {
                                        if (row >= entry.row && row < entry.row + entry.size.rows && col >= entry.col &&
                                            col < entry.col + entry.size.cols) {
                                            return entry;
                                        }
                                        return {};
                                    },
                                    [&](di::Box<LayoutNode>& node) -> di::Optional<LayoutEntry&> {
                                        return node->hit_test(row, col);
                                    }),
                                child);
        if (result) {
            return result;
        }
    }
    return {};
}

auto LayoutNode::hit_test_vertical_line(u32 col, u32 row_start, u32 row_end) -> di::TreeSet<LayoutEntry*> {
    auto result = di::TreeSet<LayoutEntry*> {};
    for (auto& child : children) {
        auto res = di::visit(di::overload(
                                 [&](LayoutEntry& entry) -> di::TreeSet<LayoutEntry*> {
                                     auto line_intersects =
                                         row_end >= entry.row && row_start < entry.row + entry.size.rows;
                                     if (line_intersects && col >= entry.col && col < entry.col + entry.size.cols) {
                                         return di::single(&entry) | di::to<di::TreeSet>();
                                     }
                                     return {};
                                 },
                                 [&](di::Box<LayoutNode>& node) -> di::TreeSet<LayoutEntry*> {
                                     return node->hit_test_vertical_line(col, row_start, row_end);
                                 }),
                             child);
        result.insert_container(di::move(res));
    }
    return result;
}

auto LayoutNode::hit_test_horizontal_line(u32 row, u32 col_start, u32 col_end) -> di::TreeSet<LayoutEntry*> {
    auto result = di::TreeSet<LayoutEntry*> {};
    for (auto& child : children) {
        auto res = di::visit(di::overload(
                                 [&](LayoutEntry& entry) -> di::TreeSet<LayoutEntry*> {
                                     auto line_intersects =
                                         col_end >= entry.col && col_start < entry.col + entry.size.cols;
                                     if (line_intersects && row >= entry.row && row < entry.row + entry.size.rows) {
                                         return di::single(&entry) | di::to<di::TreeSet>();
                                     }
                                     return {};
                                 },
                                 [&](di::Box<LayoutNode>& node) -> di::TreeSet<LayoutEntry*> {
                                     return node->hit_test_horizontal_line(row, col_start, col_end);
                                 }),
                             child);
        result.insert_container(di::move(res));
    }
    return result;
}

void LayoutGroup::redistribute_space(di::Variant<di::Box<LayoutGroup>, di::Box<LayoutPane>>* new_child,
                                     i64 original_size_available, i64 new_size_available) {
    auto relevant_children_count = di::count_if(m_children, [&](auto const& child) {
        return &child != new_child;
    });
    if (relevant_children_count == 0) {
        return;
    }

    // Special case: if all relevant children have size 0, we need to evenly space them out
    // amonst the available space.
    auto all_zero = di::all_of(m_children, [&](auto const& child) {
        return &child == new_child || di::visit(GetRelativeSize {}, child) == 0;
    });

    // Redistribute existing layout size's into the space remaining.
    auto available_size = new_size_available;
    auto leftover_size = di::Rational(i64(0));
    for (auto& child : m_children) {
        if (&child == new_child) {
            continue;
        }

        auto& relative_size = di::visit(GetRelativeSize {}, child);
        auto new_target_size = leftover_size;
        if (all_zero) {
            new_target_size += di::Rational(1_i64, i64(relevant_children_count));
        } else {
            new_target_size += di::Rational(relative_size, original_size_available);
        }
        auto new_relative_size = (new_target_size * available_size).round();
        relative_size = new_relative_size;

        leftover_size = new_target_size;
        leftover_size -= { new_relative_size, available_size };
    }

    // Validate that the new layout has a total sum of relative sizes equal to
    // the total size available.
    auto total_relative_size = i64(0);
    for (auto& child : m_children) {
        if (&child == new_child) {
            continue;
        }
        total_relative_size += di::visit(GetRelativeSize {}, child);
    }
    ASSERT_EQ(total_relative_size, new_size_available);
}

void LayoutGroup::validate_layout() {
    if (empty()) {
        return;
    }

    // Validate that at each level of the layout tree, the total availsize always sums to the
    // correct value.
    auto total_relative_size = i64(0);
    for (auto& child : m_children) {
        auto size = di::visit(GetRelativeSize {}, child);
        ASSERT_GT_EQ(size, 0);
        ASSERT_LT_EQ(size, max_layout_precision);
        total_relative_size += size;
    }
    ASSERT_EQ(total_relative_size, max_layout_precision);
}

auto LayoutGroup::split(Size const& size, u32 row_offset, u32 col_offset, Pane* reference, Direction direction)
    -> di::Tuple<di::Box<LayoutNode>, di::Optional<LayoutEntry&>, di::Optional<di::Box<Pane>&>> {
    auto do_final_layout = [&](di::Box<LayoutPane>& child)
        -> di::Tuple<di::Box<LayoutNode>, di::Optional<LayoutEntry&>, di::Optional<di::Box<Pane>&>> {
        auto result = layout(size, row_offset, col_offset);
        return { di::move(result), result->find_pane(child->pane.get()), child->pane };
    };

    auto redistribute = [&](LayoutGroup& parent, di::Variant<di::Box<LayoutGroup>, di::Box<LayoutPane>>& new_child) {
        // The new child will get 1/N of the available space, where N is the total number of children.
        auto new_children_count = i32(parent.m_children.size());
        auto new_used_size = max_layout_precision / new_children_count;
        di::visit(GetRelativeSize {}, new_child) = new_used_size;

        // Now redistribute the remaining space.
        auto available_size = max_layout_precision - new_used_size;
        parent.redistribute_space(&new_child, max_layout_precision, available_size);
    };

    // Init case: we're adding the first pane.
    if (empty()) {
        ASSERT(reference == nullptr);

        m_direction = Direction::None;
        auto& child = m_children.emplace_back(di::make_box<LayoutPane>());
        return do_final_layout(child.get<di::Box<LayoutPane>>());
    }

    // Recursive case: find the reference node in the tree.
    ASSERT(direction != Direction::None);
    auto [parent, index] = FindPaneInLayoutGroup {}(nullptr, 0, reference, this);
    if (!parent) {
        return {};
    }
    ASSERT(index < parent->m_children.size());
    ASSERT(di::holds_alternative<di::Box<LayoutPane>>(parent->m_children[index]));

    // Base case: we only have 1 pane so we take the new direction.
    if (parent->single()) {
        ASSERT(di::holds_alternative<di::Box<LayoutPane>>(m_children[0]));

        parent->m_direction = direction;
        auto& child = parent->m_children.emplace_back(di::make_box<LayoutPane>());
        redistribute(*parent, child);
        return do_final_layout(child.get<di::Box<LayoutPane>>());
    }

    // Case 2: the parent's direction is the same as the requested direction.
    if (parent->m_direction == direction) {
        // The new child will get 1/N of the available space, where N is the total number of children.
        auto* new_child =
            parent->m_children.emplace(parent->m_children.begin() + index + 1, di::make_box<LayoutPane>());
        redistribute(*parent, *new_child);
        return do_final_layout(new_child->get<di::Box<LayoutPane>>());
    }

    // Case 3: the parent' direction differs. We need to create a new LayoutGroup.
    auto new_group = di::make_box<LayoutGroup>();
    new_group->m_direction = direction;
    auto& old_layout_pane =
        new_group->m_children.emplace_back(di::move(parent->m_children[index].get<di::Box<LayoutPane>>()));
    auto& old_layout_pane_relative_size = di::visit(GetRelativeSize {}, old_layout_pane);
    di::swap(old_layout_pane_relative_size, new_group->relative_size());
    auto& child = new_group->m_children.emplace_back(di::make_box<LayoutPane>());
    redistribute(*new_group, child);
    parent->m_children[index] = di::move(new_group);
    return do_final_layout(child.get<di::Box<LayoutPane>>());
}

auto LayoutGroup::remove_pane(Pane* pane) -> di::Box<Pane> {
    auto _ = di::ScopeExit([&] {
        validate_layout();
    });

    // First, try to delete the pane from this level.
    auto result = di::Box<Pane> {};
    auto removed_size = di::Optional<i64> {};
    di::erase_if(m_children, [&](auto const& variant) {
        return di::visit(di::overload(
                             [&](di::Box<LayoutGroup> const&) {
                                 return false;
                             },
                             [&](di::Box<LayoutPane> const& layout_pane) {
                                 if (layout_pane->pane.get() == pane) {
                                     result = di::move(layout_pane->pane);
                                     removed_size = layout_pane->relative_size;
                                     return true;
                                 }
                                 return false;
                             }),
                         variant);
    });
    if (removed_size) {
        redistribute_space(nullptr, max_layout_precision - removed_size.value(), max_layout_precision);
    }

    // Then, try to delete the pane recursively.
    for (auto const& child : m_children) {
        if (auto group = di::get_if<di::Box<LayoutGroup>>(child)) {
            auto res = group.value()->remove_pane(pane);
            if (res) {
                result = di::move(res);
            }
        }
    }

    // Now determine if we have any immediate children which are either empty or singular.
    // If so, we need to either remove or shrink our children.
    for (auto* it = m_children.begin(); it != m_children.end();) {
        auto advance = di::ScopeExit([&] {
            ++it;
        });

        auto group = di::get_if<di::Box<LayoutGroup>>(*it);
        if (!group) {
            continue;
        }

        // If empty, remove the node entirely.
        if (group.value()->empty()) {
            advance.release();
            auto freed_size = group.value()->relative_size();
            it = m_children.erase(it);
            redistribute_space(nullptr, max_layout_precision - freed_size, max_layout_precision);
            continue;
        }

        // If our child is a layout group with the same direction as us, we need to
        // absorb all its children.
        if (group.value()->direction() == this->direction()) {
            // First erase the child and then insert all its children.
            auto save = di::get<di::Box<LayoutGroup>>(di::move(*it));
            advance.release();

            auto available_size = save->relative_size();
            it = m_children.erase(it);

            save->redistribute_space(nullptr, max_layout_precision, available_size);
            it = m_children.insert_container(it, di::move(save->m_children) | di::as_rvalue).end();
            continue;
        }
    }

    // If we're single, and our sole child is a layout group, we need to replace ourselves with our child, while
    // preserving our existing relative size.
    if (single() && di::holds_alternative<di::Box<LayoutGroup>>(m_children[0])) {
        auto original_size = relative_size();
        auto save = di::move(m_children[0]);
        *this = di::move(*di::get<di::Box<LayoutGroup>>(save));
        m_relative_size = original_size;
    }

    // Clear our direction if don't have multiple children.
    if (m_children.size() <= 1) {
        m_direction = Direction::None;
    }

    return result;
}

static void resize_pane(Pane* pane, Size const& size) {
    if (pane) {
        pane->resize(size);
    }
};

auto LayoutGroup::resize(LayoutNode& root, Pane* pane, ResizeDirection direction, i32 amount_in_cells) -> bool {
    auto entry = root.find_pane(pane);
    if (!entry) {
        return false;
    }
    auto* parent = entry->parent;
    ASSERT(parent);
    ASSERT(parent->group);
    auto* grand_parent = parent->parent;

    // First identify whether we are modifying the parent layout group or the grand parent layout group.
    // Each level only concerns itself with horizontal or vertical spacing, not both.
    auto is_horizontal = direction == ResizeDirection::Right || direction == ResizeDirection::Left;
    auto* relevant_node = (parent->direction == Direction::Horizontal && is_horizontal) ||
                                  (parent->direction == Direction::Vertical && !is_horizontal)
                              ? parent
                              : grand_parent;

    // Ignore request to resize groups in the wrong direction or if there is only a single item in the group
    // (direction == Direction::None).
    if (!relevant_node || (is_horizontal && relevant_node->direction != Direction::Horizontal) ||
        (!is_horizontal && relevant_node->direction != Direction::Vertical)) {
        return false;
    }

    ASSERT(relevant_node);
    ASSERT(relevant_node->group);
    ASSERT_NOT_EQ(relevant_node->group->m_direction, Direction::None);
    ASSERT_GT(relevant_node->group->m_children.size(), 1);

    // Now find the relevant layout item.
    auto* it = di::find_if(relevant_node->group->m_children, [&](auto const& x) {
        return di::visit(di::overload(
                             [&](di::Box<LayoutPane> const& layout_pane) -> bool {
                                 return layout_pane->pane.get() == pane;
                             },
                             [&](di::Box<LayoutGroup> const& layout_node) -> bool {
                                 return layout_node.get() == parent->group;
                             }),
                         x);
    });
    ASSERT_NOT_EQ(it, relevant_node->group->m_children.end());

    // Now determine the sibling we need to take size from, and validate that it exists.
    auto sibling_is_prior = direction == ResizeDirection::Left || direction == ResizeDirection::Top;
    if (it == relevant_node->group->m_children.begin() && sibling_is_prior) {
        return false;
    }
    auto* sibling = sibling_is_prior ? di::prev(it) : di::next(it);
    if (sibling == relevant_node->group->m_children.end()) {
        return false;
    }

    // Now convert from cell space to relative layout space.
    auto available_size =
        relevant_node->direction == Direction::Horizontal ? relevant_node->size.cols : relevant_node->size.rows;
    if (available_size <= relevant_node->children.size() * 2 - 1) {
        return false;
    }
    available_size -= relevant_node->children.size() * 2 - 1;

    // Now assign new layout values, clamping to prevent negative numbers.
    auto relative_size = (di::Rational<i64>(amount_in_cells, available_size) * max_layout_precision).round();
    auto& child_size = di::visit(GetRelativeSize {}, *it);
    auto& sibling_size = di::visit(GetRelativeSize {}, *sibling);
    if (relative_size > 0) {
        relative_size = di::min(relative_size, sibling_size);
    } else if (relative_size < 0) {
        relative_size = di::max(relative_size, -child_size);
    }
    if (relative_size == 0) {
        return false;
    }
    child_size += relative_size;
    sibling_size -= relative_size;

    relevant_node->group->validate_layout();
    return true;
}

auto LayoutGroup::layout(Size const& size, u32 row_offset, u32 col_offset) -> di::Box<LayoutNode> {
    auto node = di::make_box<LayoutNode>(row_offset, col_offset, size,
                                         di::Vector<di::Variant<di::Box<LayoutNode>, LayoutEntry>> {}, nullptr, this,
                                         direction());

    // Base case: we less than 1 pane, so allocate all of our space to it.
    if (m_direction == Direction::None) {
        ASSERT(empty() || single());
        if (single()) {
            auto pane = di::get_if<di::Box<LayoutPane>>(m_children[0]);
            ASSERT(pane.has_value());

            resize_pane(pane.value()->pane.get(), size);
            node->children.push_back(
                LayoutEntry { row_offset, col_offset, size, node.get(), pane.value()->pane.get() });
        }
        return node;
    }

    auto const fixed_dimension = m_direction == Direction::Horizontal ? &Size::rows : &Size::cols;
    auto const fixed_pixels_dimension = m_direction == Direction::Horizontal ? &Size::ypixels : &Size::xpixels;
    auto const dynamic_dimension = m_direction == Direction::Horizontal ? &Size::cols : &Size::rows;
    auto const dynamic_pixels_dimension = m_direction == Direction::Horizontal ? &Size::xpixels : &Size::ypixels;

    // We need at least 1 cell for each child, and N - 1 cells for the box drawing separator between panes.
    auto const min_dynamic_size = m_children.size() * 2 - 1;
    auto const min_fixed_size = 1_u32;
    if (size.*fixed_dimension < min_fixed_size || size.*dynamic_dimension < min_dynamic_size) {
        return node;
    }

    // Account for the N - 1 cells of padding between panes, as well as the unconditional minimum size of 1.
    auto available_size = size;
    available_size.*dynamic_pixels_dimension -=
        (2 * m_children.size() - 1) * available_size.*dynamic_pixels_dimension / available_size.*dynamic_dimension;
    available_size.*dynamic_dimension -= 2 * m_children.size() - 1;

    auto leftover_dynamic_size = di::Rational(i64(0));

    auto const fixed_size = available_size.*fixed_dimension;
    auto const fixed_pixels = available_size.*fixed_pixels_dimension;

    auto fixed_offset = m_direction == Direction::Horizontal ? row_offset : col_offset;
    auto dynamic_offset = m_direction == Direction::Horizontal ? col_offset : row_offset;

    for (auto const& child : m_children) {
        leftover_dynamic_size += di::Rational(di::visit(GetRelativeSize {}, child), max_layout_precision) *
                                 available_size.*dynamic_dimension;
        auto dynamic_size = u32(leftover_dynamic_size.round());
        auto dynamic_pixels =
            (dynamic_size + 1) * available_size.*dynamic_pixels_dimension / available_size.*dynamic_dimension;

        auto size = Size {
            m_direction == Direction::Horizontal ? fixed_size : dynamic_size + 1,
            m_direction == Direction::Horizontal ? dynamic_size + 1 : fixed_size,
            m_direction == Direction::Horizontal ? dynamic_pixels : fixed_pixels,
            m_direction == Direction::Horizontal ? fixed_pixels : dynamic_pixels,
        };
        auto row = m_direction == Direction::Horizontal ? fixed_offset : dynamic_offset;
        auto col = m_direction == Direction::Horizontal ? dynamic_offset : fixed_offset;

        // Do recursive layout.
        di::visit(di::overload(
                      [&](di::Box<LayoutPane> const& layout_pane) {
                          resize_pane(layout_pane->pane.get(), size);
                          node->children.emplace_back(LayoutEntry {
                              row,
                              col,
                              size,
                              node.get(),
                              layout_pane->pane.get(),
                          });
                      },
                      [&](di::Box<LayoutGroup> const& group) {
                          auto result = group->layout(size, row, col);
                          result->parent = node.get();
                          node->children.emplace_back(di::move(result));
                      }),
                  child);

        leftover_dynamic_size -= dynamic_size;
        dynamic_offset += dynamic_size;
        dynamic_offset++; // Account for the minimum dynamic size of 1.
        dynamic_offset++; // Account for box drawing character.
    }

    return node;
}

struct ToJsonV1 {
    static auto operator()(LayoutGroup const& node) -> json::v1::PaneLayoutNode {
        auto json = json::v1::PaneLayoutNode {};
        json.direction = node.direction();
        json.relative_size = node.relative_size();
        for (auto const& child : node.m_children) {
            json.children.push_back(di::visit<json::v1::PaneLayoutVariant>(ToJsonV1 {}, child));
        }
        return json;
    }

    static auto operator()(di::Box<LayoutGroup> const& node) -> di::Box<json::v1::PaneLayoutNode> {
        return di::make_box<json::v1::PaneLayoutNode>(ToJsonV1::operator()(*node));
    }

    static auto operator()(di::Box<LayoutPane> const& node) -> json::v1::Pane {
        auto json = json::v1::Pane {};
        if (node->pane) {
            json.id = node->pane->id();
        }
        json.relative_size = node->relative_size;
        return json;
    }
};

auto LayoutGroup::as_json_v1() const -> json::v1::PaneLayoutNode {
    return ToJsonV1::operator()(*this);
}
}
