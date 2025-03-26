#pragma once

#include "di/container/tree/tree_set.h"
#include "di/reflect/prelude.h"
#include "di/vocab/pointer/box.h"
#include "direction.h"
#include "ttx/pane.h"
#include "ttx/size.h"

namespace ttx {
struct LayoutNode;
class LayoutGroup;

// Represents the layout result for a single pane. The row and col
// coordinates are absolute.
struct LayoutEntry {
    u32 row { 0 };
    u32 col { 0 };
    Size size;
    LayoutNode* parent { nullptr };
    Pane* pane { nullptr };

    auto operator==(LayoutEntry const&) const -> bool = default;

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<LayoutEntry>) {
        return di::make_fields<"LayoutEntry">(di::field<"row", &LayoutEntry::row>, di::field<"col", &LayoutEntry::col>,
                                              di::field<"size", &LayoutEntry::size>,
                                              di::field<"parent", &LayoutEntry::parent>,
                                              di::field<"pane", &LayoutEntry::pane>);
    }
};

// Represents a full layout tree. This is created by calling
// LayoutGroup::layout().
struct LayoutNode {
    u32 row { 0 };
    u32 col { 0 };
    Size size;
    di::Vector<di::Variant<di::Box<LayoutNode>, LayoutEntry>> children;
    LayoutNode* parent { nullptr };
    LayoutGroup* group { nullptr };
    Direction direction { Direction::None };

    auto find_pane(Pane* pane) -> di::Optional<LayoutEntry&>;
    auto hit_test(u32 row, u32 col) -> di::Optional<LayoutEntry&>;

    auto hit_test_horizontal_line(u32 row, u32 col_start, u32 col_end) -> di::TreeSet<LayoutEntry*>;
    auto hit_test_vertical_line(u32 col, u32 row_start, u32 row_end) -> di::TreeSet<LayoutEntry*>;

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<LayoutNode>) {
        return di::make_fields<"LayoutNode">(
            di::field<"row", &LayoutNode::row>, di::field<"col", &LayoutNode::col>,
            di::field<"size", &LayoutNode::size>, di::field<"children", &LayoutNode::children>,
            di::field<"parent", &LayoutNode::parent>, di::field<"group", &LayoutNode::group>,
            di::field<"direction", &LayoutNode::direction>);
    }
};

// Pane sizes are computed in units of this number of precision, using fixed point arithemetic.
constexpr inline auto max_layout_precision = i64(100'000);

// Represents a pane in a layout group. This includes extra metadata
// necessary for layout.
struct LayoutPane {
    di::Box<Pane> pane {};
    i64 relative_size { max_layout_precision };
};

enum class ResizeDirection {
    Left,
    Right,
    Top,
    Bottom,
};

constexpr auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<ResizeDirection>) {
    using enum ResizeDirection;
    return di::make_enumerators<"ResizeDirection">(di::enumerator<"Left", Left>, di::enumerator<"Right", Right>,
                                                   di::enumerator<"Top", Top>, di::enumerator<"Bottom", Bottom>);
}

// Represents a group of panes in a hierarchy. Instead of using a strict binary tree,
// we allow multiple children on a single level so that by default, splits made in
// the same direction share space evenly.
class LayoutGroup {
public:
    constexpr auto direction() const -> Direction { return m_direction; }
    constexpr auto empty() const -> bool { return m_children.empty(); }
    constexpr auto single() const -> bool { return m_children.size() == 1; }
    constexpr auto relative_size() -> i64& { return m_relative_size; }

    // NOTE: this method returns the correct size for the new pane, and a lvalue reference where
    // the caller should store its newly created Pane. We need this akward API so that we can
    // create a Pane with a sane initial size.
    auto split(Size const& size, u32 row_offset, u32 col_offset, Pane* reference, Direction direction)
        -> di::Tuple<di::Box<LayoutNode>, di::Optional<LayoutEntry&>, di::Optional<di::Box<Pane>&>>;

    // NOTE: after removing a pane, calling layout() is necessary as any previous LayoutNode's may become invalid.
    auto remove_pane(Pane* pane) -> di::Box<Pane>;

    // NOTE: after resizing a pane, caling layout() is necessary for the change to take effect. This functions
    // returns true if any change occurred.
    auto resize(LayoutNode& root, Pane* pane, ResizeDirection direction, i32 amount_in_cells) -> bool;

    // NOTE: in addition to computing the layout tree, Pane::resize() is called to inform each Pane of its (potentially)
    // new size.
    auto layout(Size const& size, u32 row_offset, u32 col_offset) -> di::Box<LayoutNode>;

private:
    friend struct FindPaneInLayoutGroup;

    void redistribute_space(di::Variant<di::Box<LayoutGroup>, di::Box<LayoutPane>>* new_child,
                            i64 original_size_available, i64 new_size_available);
    void validate_layout();

    di::Vector<di::Variant<di::Box<LayoutGroup>, di::Box<LayoutPane>>> m_children;
    i64 m_relative_size { max_layout_precision };
    Direction m_direction { Direction::None };
};
}
