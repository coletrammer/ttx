#include "di/test/prelude.h"
#include "ttx/terminal/absolute_position.h"
#include "ttx/terminal/reflow_result.h"

namespace reflow_result {
using namespace ttx;

static auto basic() {
    auto reflow_result = terminal::ReflowResult {};

    reflow_result.add_offset({ 1, 10 }, 1, -10);
    reflow_result.add_offset({ 1, 20 }, 2, -20);
    reflow_result.add_offset({ 2, 0 }, 3, 0);
    reflow_result.add_offset({ 3, 0 }, 2, 10);
    reflow_result.add_offset({ 4, 0 }, 2, 0);

    struct Test {
        terminal::AbsolutePosition input;
        terminal::AbsolutePosition output;
    };

    auto tests = di::Array {
        Test {
            .input = { 0, 0 },
            .output = { 0, 0 },
        },
        Test {
            .input = { 1, 10 },
            .output = { 2, 0 },
        },
        Test {
            .input = { 1, 11 },
            .output = { 2, 1 },
        },
        Test {
            .input = { 1, 20 },
            .output = { 3, 0 },
        },
        Test {
            .input = { 3, 0 },
            .output = { 5, 10 },
        },
        Test {
            .input = { 6, 5 },
            .output = { 8, 5 },
        },
    };

    for (auto const& t : tests) {
        auto result = reflow_result.map_position(t.input);
        ASSERT_EQ(result, t.output);
    }
}

static void merge() {
    auto a = terminal::ReflowResult {};
    auto b = terminal::ReflowResult {};
    auto c = terminal::ReflowResult {};

    a.add_offset({ 5, 0 }, 1, 0);
    b.add_offset({ 10, 0 }, 1, 0);

    a.merge(di::move(b));

    auto expected = terminal::ReflowResult {};
    expected.add_offset({ 5, 0 }, 1, 0);
    expected.add_offset({ 10, 0 }, 2, 0);
    ASSERT_EQ(a, expected);

    c.add_offset({ 0, 0 }, 1, 0);

    a.merge(di::move(c));

    expected = terminal::ReflowResult();
    expected.add_offset({ 0, 0 }, 1, 0);
    expected.add_offset({ 5, 0 }, 2, 0);
    expected.add_offset({ 10, 0 }, 3, 0);

    ASSERT_EQ(a, expected);
}

TEST(reflow_result, basic)
TEST(reflow_result, merge)
}
