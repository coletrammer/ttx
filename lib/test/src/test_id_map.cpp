#include "di/test/prelude.h"
#include "ttx/terminal/id_map.h"
#include "ttx/terminal/multi_cell_info.h"

namespace id_map {
using namespace ttx::terminal;

static void basic() {
    auto map = IdMap<MultiCellInfo> {};

    auto v1 = MultiCellInfo { .width = 2 };
    auto id1 = map.allocate(v1);
    ASSERT_EQ(id1, 1);

    auto v2 = MultiCellInfo { .width = 3 };
    auto id2 = map.allocate(v2);
    ASSERT_EQ(id2, 2);

    ASSERT_EQ(map.lookup_id(*id1), v1);
    ASSERT_EQ(map.lookup_id(*id2), v2);

    // Bump the reference count twice.
    map.use_id(*id1);
    map.use_id(*id1);

    // Now we must drop id1 3 times.
    map.drop_id(*id1);
    map.drop_id(*id1);
    map.drop_id(*id1);

    ASSERT(!map.lookup_key(v1));

    // We should reuse the freed id.
    id1 = map.allocate(v1);
    ASSERT_EQ(id1, 1);
}

static void full() {
    auto map = IdMap<u32> {};

    // Fill the map.
    for (auto id : di::range(IdMap<u32>::max_id)) {
        ASSERT_EQ(map.allocate(id + 1), id + 1);
    }

    // Now allocation should fail.
    ASSERT(!map.allocate(0));
}

TEST(id_map, basic)
TEST(id_map, full)
}
