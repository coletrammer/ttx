#pragma once

#include "di/container/string/prelude.h"
#include "di/reflect/prelude.h"
#include "di/serialization/json_deserializer.h"
#include "di/serialization/json_serializer.h"
#include "di/serialization/percent_encoded.h"
#include "ttx/direction.h"
#include "ttx/ids.h"
#include "ttx/ipc/pane_id.h"

namespace ttx::json::v1 {
struct Pane {
    i64 relative_size { 0 };
    PaneId id { 0 };
    di::Optional<di::PercentEncoded<>> current_working_directory;

    auto operator==(Pane const&) const -> bool = default;

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<Pane>) {
        return di::make_fields<"json::v1::Pane">(
            di::field<"relative_size", &Pane::relative_size>, di::field<"id", &Pane::id>,
            di::field<"current_working_directory", &Pane::current_working_directory>);
    }
};

struct PaneLayoutNode;

struct PaneLayoutVariant : di::Variant<di::Box<PaneLayoutNode>, Pane> {
    using Base = di::Variant<di::Box<PaneLayoutNode>, Pane>;

    using Base::Base;
    using Base::operator=;

    auto operator==(PaneLayoutVariant const& other) const -> bool;
};

struct PaneLayoutNode {
    di::Vector<PaneLayoutVariant> children;
    i64 relative_size { 0 };
    Direction direction { Direction::None };

    auto operator==(PaneLayoutNode const& other) const -> bool = default;

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<PaneLayoutNode>) {
        return di::make_fields<"json::v1::PaneLayoutNode">(di::field<"children", &PaneLayoutNode::children>,
                                                           di::field<"relative_size", &PaneLayoutNode::relative_size>,
                                                           di::field<"direction", &PaneLayoutNode::direction>);
    }
};

inline auto PaneLayoutVariant::operator==(PaneLayoutVariant const& other) const -> bool {
    if (index() != other.index()) {
        return false;
    }
    if (index() == 0) {
        return *di::get<di::Box<PaneLayoutNode>>(*this) == *di::get<di::Box<PaneLayoutNode>>(other);
    }
    return di::get<Pane>(*this) == di::get<Pane>(other);
}

struct Tab {
    PaneLayoutNode pane_layout;
    di::Vector<PaneId> pane_ids_by_recency;
    di::Optional<PaneId> active_pane_id;
    di::Optional<PaneId> full_screen_pane_id;
    di::Optional<di::String> name;
    TabId id { 0 };

    auto operator==(Tab const&) const -> bool = default;

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<Tab>) {
        return di::make_fields<"json::v1::Tab">(di::field<"pane_layout", &Tab::pane_layout>,
                                                di::field<"pane_ids_by_recency", &Tab::pane_ids_by_recency>,
                                                di::field<"active_pane_id", &Tab::active_pane_id>,
                                                di::field<"full_screen_pane_id", &Tab::full_screen_pane_id>,
                                                di::field<"name", &Tab::name>, di::field<"id", &Tab::id>);
    }
};

struct Workspace {
    di::Vector<Tab> tabs;
    di::Optional<TabId> active_tab_id;
    di::Optional<di::String> name;
    WorkspaceId id { 0 };

    auto operator==(Workspace const&) const -> bool = default;

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<Workspace>) {
        // Backwards compat: workspaces were originally sessions
        return di::make_fields<"json::v1::Session">(
            di::field<"tabs", &Workspace::tabs>, di::field<"active_tab_id", &Workspace::active_tab_id>,
            di::field<"name", &Workspace::name>, di::field<"id", &Workspace::id>);
    }
};

struct LayoutState {
    di::Vector<Workspace> workspaces;
    di::Optional<WorkspaceId> active_workspace_id;

    auto operator==(LayoutState const&) const -> bool = default;

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<LayoutState>) {
        return di::make_fields<"json::v1::LayoutState">(
            // Backwards compat: workspaces were originally sessions
            di::field<"sessions", &LayoutState::workspaces>,
            di::field<"active_session_id", &LayoutState::active_workspace_id>);
    }
};
}

namespace ttx::json {
using Layout = di::Variant<json::v1::LayoutState>;
}
