#pragma once

#include "di/container/string/prelude.h"
#include "di/reflect/prelude.h"
#include "di/serialization/json_deserializer.h"
#include "di/serialization/json_serializer.h"
#include "ttx/direction.h"

namespace ttx::json::v1 {
struct Pane {
    i64 relative_size { 0 };
    u64 id { 0 };

    auto operator==(Pane const&) const -> bool = default;

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<Pane>) {
        return di::make_fields<"json::v1::Pane">(di::field<"relative_size", &Pane::relative_size>,
                                                 di::field<"id", &Pane::id>);
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
    di::Vector<u64> pane_ids_by_recency;
    di::Optional<u64> active_pane_id;
    di::Optional<u64> full_screen_pane_id;
    di::String name;
    u64 id { 0 };

    auto operator==(Tab const&) const -> bool = default;

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<Tab>) {
        return di::make_fields<"json::v1::Tab">(di::field<"pane_layout", &Tab::pane_layout>,
                                                di::field<"pane_ids_by_recency", &Tab::pane_ids_by_recency>,
                                                di::field<"active_pane_id", &Tab::active_pane_id>,
                                                di::field<"full_screen_pane_id", &Tab::full_screen_pane_id>,
                                                di::field<"name", &Tab::name>, di::field<"id", &Tab::id>);
    }
};

struct Session {
    di::Vector<Tab> tabs;
    di::Optional<u64> active_tab_id;
    di::String name;
    u64 id { 0 };

    auto operator==(Session const&) const -> bool = default;

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<Session>) {
        return di::make_fields<"json::v1::Session">(di::field<"tabs", &Session::tabs>,
                                                    di::field<"active_tab_id", &Session::active_tab_id>,
                                                    di::field<"name", &Session::name>, di::field<"id", &Session::id>);
    }
};

struct LayoutState {
    di::Vector<Session> sessions;
    di::Optional<u64> active_session_id;

    auto operator==(LayoutState const&) const -> bool = default;

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<LayoutState>) {
        return di::make_fields<"json::v1::LayoutState">(
            di::field<"sessions", &LayoutState::sessions>,
            di::field<"active_session_id", &LayoutState::active_session_id>);
    }
};
}

namespace ttx::json {
using Layout = di::Variant<json::v1::LayoutState>;
}
