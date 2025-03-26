#include "di/test/prelude.h"
#include "dius/print.h"
#include "ttx/layout.h"
#include "ttx/size.h"

namespace layout {
using namespace ttx;

static auto add_pane(LayoutGroup& root, Size const& size, Pane* reference, Direction direction)
    -> di::Tuple<Pane&, di::Box<LayoutNode>> {
    auto [layout_tree, entry, pane_out] = root.split(size, 0, 0, reference, direction);
    ASSERT(entry);
    ASSERT(pane_out);
    auto& pane = *pane_out = Pane::create_mock();
    entry->pane = pane.get();

    return { *pane, di::move(layout_tree) };
};

static auto validate_layout_for_pane(Pane& pane, LayoutNode& tree, u32 row, u32 col, Size size) {
    auto entry = tree.find_pane(&pane);
    ASSERT(entry);

    // Default the pixel widths/heights.
    size.ypixels = size.rows * 10;
    size.xpixels = size.cols * 10;

    ASSERT_EQ(entry->pane, &pane);
    ASSERT_EQ(entry->row, row);
    ASSERT_EQ(entry->col, col);
    ASSERT_EQ(entry->size, size);
};

static void splits() {
    constexpr auto size = Size(64, 128, 1280, 640);

    auto root = LayoutGroup {};

    // Initial pane
    auto [pane0, l0] = add_pane(root, size, nullptr, Direction::None);
    validate_layout_for_pane(pane0, *l0, 0, 0, size);

    // Vertical split
    auto [pane1, l1] = add_pane(root, size, &pane0, Direction::Vertical);
    validate_layout_for_pane(pane0, *l1, 0, 0, { 32, 128 });
    validate_layout_for_pane(pane1, *l1, 33, 0, { 31, 128 });

    // Horitzontal split above
    auto [pane2, l2] = add_pane(root, size, &pane0, Direction::Horizontal);
    validate_layout_for_pane(pane0, *l2, 0, 0, { 32, 64 });
    validate_layout_for_pane(pane1, *l2, 33, 0, { 31, 128 });
    validate_layout_for_pane(pane2, *l2, 0, 65, { 32, 63 });

    // 2 Veritcal splits under pane 2.
    auto [pane4, _] = add_pane(root, size, &pane2, Direction::Vertical);
    auto [pane3, l3] = add_pane(root, size, &pane2, Direction::Vertical);
    validate_layout_for_pane(pane0, *l3, 0, 0, { 32, 64 });
    validate_layout_for_pane(pane1, *l3, 33, 0, { 31, 128 });
    validate_layout_for_pane(pane2, *l3, 0, 65, { 10, 63 });
    validate_layout_for_pane(pane3, *l3, 11, 65, { 10, 63 });
    validate_layout_for_pane(pane4, *l3, 22, 65, { 10, 63 });
}

static void many_splits() {
    constexpr auto size = Size(1000 + 99, 1000 + 99, 0, 0);

    auto root = LayoutGroup {};

    // Initial pane
    auto [pane0, l0] = add_pane(root, size, nullptr, Direction::None);

    // Add 99 panes, and verify space is distributed evenly between them.
    auto panes = di::Vector<Pane*> {};
    for (auto _ : di::range(99)) {
        auto [pane, _] = add_pane(root, size, &pane0, Direction::Vertical);
        panes.push_back(&pane);
    }

    auto l = root.layout(size, 0, 0);
    for (auto* pane : panes) {
        auto entry = l->find_pane(pane);
        ASSERT(entry);

        ASSERT_EQ(entry->size.rows, 10);
    }
}

static void remove_pane() {
    constexpr auto size = Size(64, 128, 1280, 640);

    {
        auto root = LayoutGroup {};

        // Initial pane
        auto [pane0, _] = add_pane(root, size, nullptr, Direction::None);

        // Vertical split
        auto [pane1, _] = add_pane(root, size, &pane0, Direction::Vertical);

        // Horitzontal split above
        auto [pane2, _] = add_pane(root, size, &pane0, Direction::Horizontal);

        // 2 Veritcal splits under pane 2.
        auto [pane4, _] = add_pane(root, size, &pane2, Direction::Vertical);
        auto [pane3, _] = add_pane(root, size, &pane2, Direction::Vertical);

        // Now the layout looks something like this:
        // |---------|--------|
        // |0        |2       |
        // |         |--------|
        // |         |3       |
        // |         |--------|
        // |         |4       |
        // |---------|--------|
        // |1                 |
        // |                  |
        // |                  |
        // |                  |
        // |                  |
        // |------------------|

        // When we remove pane 0, we need to collapse panes 2-4, into the same vertical layout
        // group with pane 1.
        root.remove_pane(&pane0);

        // Now the layout looks something like this:
        // |------------------|
        // |2                 |
        // |------------------|
        // |3                 |
        // |------------------|
        // |4                 |
        // |------------------|
        // |1                 |
        // |                  |
        // |                  |
        // |                  |
        // |                  |
        // |------------------|

        auto l0 = root.layout(size, 0, 0);
        validate_layout_for_pane(pane2, *l0, 0, 0, { 11, 128 });
        validate_layout_for_pane(pane3, *l0, 12, 0, { 10, 128 });
        validate_layout_for_pane(pane4, *l0, 23, 0, { 11, 128 });
        validate_layout_for_pane(pane1, *l0, 35, 0, { 29, 128 });
    }

    {
        auto root = LayoutGroup {};

        // Initial pane
        auto [pane0, _] = add_pane(root, size, nullptr, Direction::None);

        // Horizontal split
        auto [pane1, _] = add_pane(root, size, &pane0, Direction::Horizontal);

        // Vertical Split
        auto [pane2, _] = add_pane(root, size, &pane1, Direction::Vertical);

        // Now the layout looks something like this:
        // |---------|--------|
        // |0        |1       |
        // |         |--------|
        // |         |2       |
        // |---------|--------|

        // When we remove pane 0, we need need to replace the root layout group with its direct child.
        root.remove_pane(&pane0);

        auto l0 = root.layout(size, 0, 0);
        validate_layout_for_pane(pane1, *l0, 0, 0, { 32, 128 });
        validate_layout_for_pane(pane2, *l0, 33, 0, { 31, 128 });
    }
}

static void hit_test() {
    constexpr auto size = Size(64, 128, 1280, 640);

    auto root = LayoutGroup {};

    // Initial pane
    auto [pane0, _] = add_pane(root, size, nullptr, Direction::None);

    // Vertical split
    auto [pane1, _] = add_pane(root, size, &pane0, Direction::Vertical);

    // Horitzontal split above
    auto [pane2, _] = add_pane(root, size, &pane0, Direction::Horizontal);

    // 2 Veritcal splits under pane 2.
    auto [pane4, _] = add_pane(root, size, &pane2, Direction::Vertical);
    auto [pane3, l0] = add_pane(root, size, &pane2, Direction::Vertical);

    auto p0 = l0->find_pane(&pane0);
    auto p1 = l0->find_pane(&pane1);
    auto p2 = l0->find_pane(&pane2);
    auto p3 = l0->find_pane(&pane3);
    auto p4 = l0->find_pane(&pane4);
    ASSERT(p0.has_value());
    ASSERT(p1.has_value());
    ASSERT(p2.has_value());
    ASSERT(p3.has_value());
    ASSERT(p4.has_value());

    auto res = l0->hit_test_vertical_line(p0->col + p0->size.cols + 1, p0->row, p0->row + p0->size.rows);
    auto expected = di::TreeSet<LayoutEntry*> {};
    expected.insert(p2.data());
    expected.insert(p3.data());
    expected.insert(p4.data());
    ASSERT_EQ(res, expected);

    res = l0->hit_test_vertical_line(p3->col - 2, p3->row, p3->row + p3->size.rows);
    expected = di::TreeSet<LayoutEntry*> {};
    expected.insert(p0.data());
    ASSERT_EQ(res, expected);

    res = l0->hit_test_horizontal_line(p1->row - 2, p1->col, p1->col + p1->size.cols);
    expected = di::TreeSet<LayoutEntry*> {};
    expected.insert(p0.data());
    expected.insert(p4.data());
    ASSERT_EQ(res, expected);
}

static void resize() {
    constexpr auto size = Size(65, 129, 1290, 650);

    auto root = LayoutGroup {};

    // Initial pane
    auto [pane0, _] = add_pane(root, size, nullptr, Direction::None);

    // Vertical split
    auto [pane1, _] = add_pane(root, size, &pane0, Direction::Vertical);

    // 3 Horizontal splits
    auto [pane2, _] = add_pane(root, size, &pane0, Direction::Horizontal);
    auto [pane3, l] = add_pane(root, size, &pane1, Direction::Horizontal);

    // Now the layout looks something like this:
    // |---------|--------|
    // |0        |2       |
    // |---------|--------|
    // |1        |3       |
    // |---------|--------|

    // Validate the layout after adjusting an edge accordingly.
    auto validate = [&](i32 e0, i32 e1, i32 e2) {
        validate_layout_for_pane(pane0, *l, 0, 0, { u32(32 + e0), u32(64 + e1) });
        validate_layout_for_pane(pane1, *l, u32(33 + e0), 0, { u32(32 - e0), u32(64 + e2) });
        validate_layout_for_pane(pane2, *l, 0, u32(65 + e1), { u32(32 + e0), u32(64 - e1) });
        validate_layout_for_pane(pane3, *l, u32(33 + e0), u32(65 + e2), { u32(32 - e0), u32(64 - e2) });
    };

    // Initially, the layout should be valid.
    validate(0, 0, 0);

    struct Case {
        Pane* pane { nullptr };
        ResizeDirection direction { ResizeDirection::Bottom };
        i32 amount { 0 };
        di::Tuple<i32, i32, i32> edges;
        bool changed { true };
    };

    // Now test several possible resize events to make they work.
    auto cases = di::Array {
        // Pane 0
        Case { &pane0, ResizeDirection::Bottom, 1, { 1, 0, 0 } },
        Case { &pane0, ResizeDirection::Bottom, -1, { -1, 0, 0 } },
        Case { &pane0, ResizeDirection::Right, 1, { 0, 1, 0 } },
        Case { &pane0, ResizeDirection::Right, -1, { 0, -1, 0 } },
        Case { &pane0, ResizeDirection::Left, 1, { 0, 0, 0 }, false },
        Case { &pane0, ResizeDirection::Left, -1, { 0, 0, 0 }, false },
        Case { &pane0, ResizeDirection::Top, 1, { 0, 0, 0 }, false },
        Case { &pane0, ResizeDirection::Top, -1, { 0, 0, 0 }, false },

        // Pane 3
        Case { &pane3, ResizeDirection::Top, -1, { 1, 0, 0 } },
        Case { &pane3, ResizeDirection::Top, 1, { -1, 0, 0 } },
        Case { &pane3, ResizeDirection::Left, -1, { 0, 0, 1 } },
        Case { &pane3, ResizeDirection::Left, 1, { 0, 0, -1 } },
        Case { &pane3, ResizeDirection::Right, 1, { 0, 0, 0 }, false },
        Case { &pane3, ResizeDirection::Right, -1, { 0, 0, 0 }, false },
        Case { &pane3, ResizeDirection::Bottom, 1, { 0, 0, 0 }, false },
        Case { &pane3, ResizeDirection::Bottom, -1, { 0, 0, 0 }, false },
    };

    for (auto [pane, direction, amount, edges, changed] : cases) {
        auto [e0, e1, e2] = edges;

        // Do the resize.
        ASSERT_EQ(root.resize(*l, pane, direction, amount), changed);

        l = root.layout(size, 0, 0);
        validate(e0, e1, e2);

        // Undo the resize.
        ASSERT_EQ(root.resize(*l, pane, direction, -amount), changed);
        l = root.layout(size, 0, 0);
        validate(0, 0, 0);
    }
}

static void resize_to_zero() {
    constexpr auto size = Size(64, 128, 1280, 640);

    auto root = LayoutGroup {};

    // Initial pane
    auto [pane0, _] = add_pane(root, size, nullptr, Direction::None);

    // 2 vertical splits
    auto [pane1, _] = add_pane(root, size, &pane0, Direction::Vertical);
    auto [pane2, l0] = add_pane(root, size, &pane1, Direction::Vertical);

    // Resize pane 1 and pane 2 to be empty.
    ASSERT(root.resize(*l0, &pane2, ResizeDirection::Top, -128));
    ASSERT(root.resize(*l0, &pane1, ResizeDirection::Top, -128));

    // Bounds checking, these should do nothing, since there's no space left to take.
    ASSERT(!root.resize(*l0, &pane2, ResizeDirection::Top, -128));
    ASSERT(!root.resize(*l0, &pane1, ResizeDirection::Top, -128));

    // The layout should now have both pane1 and pane2 having a height of 1 (the minimum value).
    auto l1 = root.layout(size, 0, 0);
    validate_layout_for_pane(pane0, *l1, 0, 0, { 60, 128 });
    validate_layout_for_pane(pane1, *l1, 61, 0, { 1, 128 });
    validate_layout_for_pane(pane2, *l1, 63, 0, { 1, 128 });

    // After erasing pane 0, the space should be distributed evenly.
    ASSERT(root.remove_pane(&pane0));
    auto l2 = root.layout(size, 0, 0);
    validate_layout_for_pane(pane1, *l2, 0, 0, { 32, 128 });
    validate_layout_for_pane(pane2, *l2, 33, 0, { 31, 128 });
}

TEST(layout, splits)
TEST(layout, many_splits)
TEST(layout, remove_pane)
TEST(layout, hit_test)
TEST(layout, resize)
TEST(layout, resize_to_zero)
}
