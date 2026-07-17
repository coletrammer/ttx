#pragma once

#include "di/container/tree/tree_set.h"
#include "di/reflect/prelude.h"
#include "di/vocab/pointer/box.h"
#include "direction.h"
#include "ttx/ipc/pane_id.h"
#include "ttx/layout_json.h"
#include "ttx/pane.h"
#include "ttx/size.h"

namespace ttx {
struct LayoutNode;
struct LayoutPane;
class LayoutGroup;

// Represents the layout result for a single pane. The row and col
// coordinates are absolute.
struct LayoutEntry {
    u32 row { 0 };
    u32 col { 0 };
    Size size;
    LayoutNode* parent { nullptr };
    LayoutPane const* ref { nullptr };
    PaneId pane_id { 0 };

    auto operator==(LayoutEntry const&) const -> bool = default;

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<LayoutEntry>) {
        return di::make_fields<"LayoutEntry">(
            di::field<"row", &LayoutEntry::row>, di::field<"col", &LayoutEntry::col>,
            di::field<"size", &LayoutEntry::size>, di::field<"parent", &LayoutEntry::parent>,
            di::field<"ref", &LayoutEntry::ref>, di::field<"pane_id", &LayoutEntry::pane_id>);
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
    // Intersection points with the leading edge of this node splitting up descendants placed in the same direction.
    di::Vector<u32> start_intersections;
    // Intersection points with the trailing edge of this node splitting up descendants placed in the same direction.
    di::Vector<u32> end_intersections;

    auto find_pane(PaneId pane_id) -> di::Optional<LayoutEntry&>;
    auto hit_test(u32 row, u32 col) -> di::Optional<LayoutEntry&>;

    auto hit_test_horizontal_line(u32 row, u32 col_start, u32 col_end) -> di::TreeSet<LayoutEntry*>;
    auto hit_test_vertical_line(u32 col, u32 row_start, u32 row_end) -> di::TreeSet<LayoutEntry*>;

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<LayoutNode>) {
        return di::make_fields<"LayoutNode">(
            di::field<"row", &LayoutNode::row>, di::field<"col", &LayoutNode::col>,
            di::field<"size", &LayoutNode::size>, di::field<"children", &LayoutNode::children>,
            di::field<"parent", &LayoutNode::parent>, di::field<"group", &LayoutNode::group>,
            di::field<"direction", &LayoutNode::direction>,
            di::field<"start_intersections", &LayoutNode::start_intersections>,
            di::field<"end_intersections", &LayoutNode::end_intersections>);
    }
};

// Get the start and end intersections of a LayoutNode's or LayoutEntry's borders.
auto border_intersections(di::Variant<di::Box<LayoutNode>, LayoutEntry> const& layout)
    -> di::Tuple<di::Span<u32 const>, di::Span<u32 const>>;

// Pane sizes are computed in units of this number of precision, using fixed point arithemetic.
constexpr inline auto max_layout_precision = i64(100'000);

// Represents a pane in a layout group. This includes extra metadata
// necessary for layout.
struct LayoutPane {
    PaneId pane_id { 0 };
    di::Optional<di::Path> cwd {}; // Used when restoring a pane.
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
    constexpr auto relative_size() const -> i64 { return m_relative_size; }

    static auto from_json_v1(json::v1::PaneLayoutNode const& json, Size const& size) -> di::Result<LayoutGroup>;

    // This API returns the proper size for a pane assuming a given split. This can be used before caling split()
    // to know what size to give to the newly created pane.
    auto split_size(Size const& size, u32 row_offset, u32 col_offset, PaneId reference, Direction direction)
        -> di::Optional<Size>;

    // NOTE: this API returns the new layout object to save re-computing the layout.
    auto split(Size const& size, u32 row_offset, u32 col_offset, PaneId reference, Direction direction, PaneId new_pane)
        -> di::Box<LayoutNode>;

    // NOTE: after removing a pane, calling layout() is necessary as any previous LayoutNode's may become invalid.
    void remove_pane(PaneId pane);

    // NOTE: after resizing a pane, caling layout() is necessary for the change to take effect. This functions
    // returns true if any change occurred.
    auto resize(LayoutNode& root, PaneId, ResizeDirection direction, i32 amount_in_cells) -> bool;

    // Perform a layout given the new size, returning the new layout tree. The actual pane sizes need to be applied
    // by the caller, since the layout tree only contains references to server-side panes.
    auto layout(Size const& size, u32 row_offset, u32 col_offset) -> di::Box<LayoutNode>;

    auto find_layout_pane(PaneId pane_id) -> LayoutPane const*;

    auto as_json_v1() const -> json::v1::PaneLayoutNode;

private:
    friend struct FindPaneInLayoutGroup;
    friend struct ToJsonV1;
    friend struct FromJsonV1;

    void redistribute_space(di::Variant<di::Box<LayoutGroup>, di::Box<LayoutPane>>* new_child,
                            i64 original_size_available, i64 new_size_available);
    void validate_layout();

    di::Vector<di::Variant<di::Box<LayoutGroup>, di::Box<LayoutPane>>> m_children;
    i64 m_relative_size { max_layout_precision };
    Direction m_direction { Direction::None };
};
}
