#include "di/test/prelude.h"
#include "ttx/params.h"

namespace params {
static void basic() {
    auto params = ttx::Params({ { 1, 2 }, { 3 }, { 4, 5, 6 } });

    ASSERT_EQ(params.size(), 3);
    ASSERT_EQ(params.get(0), 1);
    ASSERT_EQ(params.get(1), 3);
    ASSERT_EQ(params.get(2), 4);
    ASSERT_EQ(params.get(3), 0);
    ASSERT_EQ(params.get(3, 29), 29);
    ASSERT(!params.empty());

    ASSERT_EQ(params.subparams(0).size(), 2);
    ASSERT(!params.subparams(0).empty());
    ASSERT_EQ(params.subparams(0).get(1), 2);
    ASSERT_EQ(params.subparams(0).get(2, 33), 33);
    ASSERT(params.subparams(4).empty());

    ASSERT_EQ(params.get_subparam(2, 1), 5);
    ASSERT_EQ(params.get_subparam(2, 3, 77), 77);
    ASSERT_EQ(params.get_subparam(3, 0, 99), 99);

    auto empty = ttx::Params();
    ASSERT(empty.empty());

    ASSERT_NOT_EQ(empty, params);

    auto params2 = empty.clone();
    params2.add_subparams({ 1, 2 });
    params2.add_param(3);
    params2.add_subparams({ 4, 5 });
    params2.add_subparam(6);
    ASSERT_EQ(params, params2);
}

static void parse() {
    auto params = ttx::Params::from_string("12;3:45:7;1;2:3"_sv);
    auto expected = ttx::Params({ { 12 }, { 3, 45, 7 }, { 1 }, { 2, 3 } });
    ASSERT_EQ(params, expected);

    auto params2 = ttx::Params::from_string("12;3:45:7;1;2:3;4::2:"_sv);
    auto expected2 = ttx::Params({ { 12 }, { 3, 45, 7 }, { 1 }, { 2, 3 } });
    expected2.add_param(4);
    expected2.add_empty_subparam();
    expected2.add_subparam(2);
    expected2.add_empty_subparam();
    ASSERT_EQ(params2, expected2);
}

static void to_string() {
    auto params = ttx::Params({ { 12 }, { 3, 45, 7 }, { 1 }, { 2, 3 } });

    ASSERT_EQ(params.to_string(), "12;3:45:7;1;2:3"_sv);
    ASSERT_EQ(di::to_string(params), "12;3:45:7;1;2:3"_sv);

    auto params2 = ttx::Params({ { 24 }, { 32, 1 } });
    params2.add_empty_param();
    params2.add_param(2);
    ASSERT_EQ(params2.to_string(), "24;32:1;;2"_sv);

    params2.add_param(1);
    params2.add_empty_subparam();
    params2.add_subparam(2);
    ASSERT_EQ(params2.to_string(), "24;32:1;;2;1::2"_sv);
}

TEST(params, basic)
TEST(params, parse)
TEST(params, to_string)
}
