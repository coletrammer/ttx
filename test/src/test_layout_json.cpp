#include "di/test/prelude.h"
#include "layout_state.h"
#include "render.h"
#include "ttx/layout_json.h"

namespace layout_json {
constexpr auto test_layout = R"(
{
    "json::v1::LayoutState": {
        "sessions": [
            {
                "tabs": [
                    {
                        "pane_layout": {
                            "children": [
                                {
                                    "json::v1::Pane": {
                                        "relative_size": 77120,
                                        "id": 1,
                                        "current_working_directory": "%2Fhome%2FWorkspace"
                                    }
                                },
                                {
                                    "json::v1::Pane": {
                                        "relative_size": 22880,
                                        "id": 13
                                    }
                                }
                            ],
                            "relative_size": 100000,
                            "direction": "Vertical"
                        },
                        "pane_ids_by_recency": [
                            13,
                            1
                        ],
                        "active_pane_id": 13,
                        "name": "ttx",
                        "id": 1
                    },
                    {
                        "pane_layout": {
                            "children": [
                                {
                                    "json::v1::Pane": {
                                        "relative_size": 100000,
                                        "id": 24
                                    }
                                }
                            ],
                            "relative_size": 100000,
                            "direction": "None"
                        },
                        "pane_ids_by_recency": [
                            24
                        ],
                        "active_pane_id": 24,
                        "name": "test",
                        "id": 2
                    },
                    {
                        "pane_layout": {
                            "children": [
                                {
                                    "json::v1::Pane": {
                                        "relative_size": 73730,
                                        "id": 3
                                    }
                                },
                                {
                                    "json::v1::Pane": {
                                        "relative_size": 26270,
                                        "id": 14
                                    }
                                }
                            ],
                            "relative_size": 100000,
                            "direction": "Vertical"
                        },
                        "pane_ids_by_recency": [
                            14,
                            3
                        ],
                        "active_pane_id": 14,
                        "name": "dius",
                        "id": 3
                    },
                    {
                        "pane_layout": {
                            "children": [
                                {
                                    "json::v1::Pane": {
                                        "relative_size": 77120,
                                        "id": 4
                                    }
                                },
                                {
                                    "Box<json::v1::PaneLayoutNode>": {
                                        "children": [
                                            {
                                                "json::v1::Pane": {
                                                    "relative_size": 100000,
                                                    "id": 15
                                                }
                                            }
                                        ],
                                        "relative_size": 22880,
                                        "direction": "None"
                                    }
                                }
                            ],
                            "relative_size": 100000,
                            "direction": "Vertical"
                        },
                        "pane_ids_by_recency": [
                            4,
                            15
                        ],
                        "active_pane_id": 4,
                        "name": "di",
                        "id": 4
                    }
                ],
                "active_tab_id": 2,
                "name": "ttx",
                "id": 1
            },
            {
                "tabs": [
                    {
                        "pane_layout": {
                            "children": [
                                {
                                    "json::v1::Pane": {
                                        "relative_size": 77120,
                                        "id": 10
                                    }
                                },
                                {
                                    "json::v1::Pane": {
                                        "relative_size": 22880,
                                        "id": 11
                                    }
                                }
                            ],
                            "relative_size": 100000,
                            "direction": "Vertical"
                        },
                        "pane_ids_by_recency": [
                            11,
                            10
                        ],
                        "active_pane_id": 11,
                        "name": "dotfiles",
                        "id": 5
                    }
                ],
                "active_tab_id": 5,
                "name": "dotfiles",
                "id": 2
            }
        ],
        "active_session_id": 1
    }
}
)"_sv;

static void roundtrip() {
    auto json_object = di::from_json_string<ttx::json::Layout>(test_layout);
    ASSERT(json_object);
    ASSERT(di::holds_alternative<ttx::json::v1::LayoutState>(json_object.value()));

    auto state = di::Synchronized(ttx::LayoutState({ 10, 10 }, false));
    auto render_thread = ttx::RenderThread::create_mock(state);
    ASSERT(
        state.get_assuming_no_concurrent_accesses().restore_json(json_object.value(), { .mock = true }, render_thread));

    auto json_save = state.get_assuming_no_concurrent_accesses().as_json();

    auto original_as_string = di::to_json_string(json_object.value(), di::JsonSerializerConfig().pretty()).value();
    auto saved_as_string = di::to_json_string(json_save, di::JsonSerializerConfig().pretty()).value();
    ASSERT_EQ(original_as_string, saved_as_string);
}

TEST(layout_json, roundtrip)
}
