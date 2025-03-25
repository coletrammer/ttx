#pragma once

#include "di/assert/prelude.h"
#include "di/bit/bitset/prelude.h"
#include "di/container/tree/tree_map.h"
#include "di/types/prelude.h"

namespace ttx::terminal {
namespace detail {
    template<typename T>
    struct DefaultOpsImpl {
        using Key = T;

        constexpr static auto get_key(T const& t) -> T const& { return t; }
    };

    template<typename T>
    struct DefaultOpsT : di::meta::TypeConstant<DefaultOpsImpl<T>> {};

    template<typename T>
    requires(requires { typename T::DefaultOps; })
    struct DefaultOpsImplT : di::meta::TypeConstant<typename T::DefaultOps> {};

    template<typename T>
    using DefaultOps = di::meta::Type<DefaultOpsT<T>>;
}

/// @brief A two-way map between a numberic id and a value.
///
/// @tparam T The value type
/// @tparam Ops An operation struct, which can be used to define a custom key.
///
/// This class implements the two way mapping behavior which deduplicates
/// graphics renditions and other cell specific state across cells. This implemenation
/// uses manual reference counting, which is non-ideal for C++, but seems necessary
/// for performance.
template<typename T, typename Ops = detail::DefaultOps<T>>
class IdMap {
public:
    using Id = u16;
    using Key = typename Ops::Key;

    constexpr static auto max_id = di::NumericLimits<Id>::max;

private:
    struct RefCounted {
        template<typename... Args>
        explicit RefCounted(Args&&... args) : value(di::forward<Args>(args)...) {}

        T value {};
        u32 ref_count { 1 };
    };

public:
    auto lookup_id(Id id) const -> T const& {
        auto it = m_id_map.find(id);
        ASSERT_NOT_EQ(it, m_id_map.end());

        return di::get<1>(*it).value;
    }

    auto lookup_key(Key const& key) const -> di::Optional<Id> {
        auto it = m_id_lookup.find(key);
        if (it == m_id_lookup.end()) {
            return {};
        }

        return di::get<1>(*it);
    }

    auto allocate(T const& value) -> di::Optional<Id> {
        auto id = allocate_id();
        if (!id) {
            return {};
        }

        auto const& key = get_key(value);
        ASSERT(!m_id_lookup.contains(key));
        m_id_lookup[key] = *id;
        m_id_map.insert_or_assign(*id, value);
        return id;
    }

    auto use_id(Id id) -> Id {
        auto it = m_id_map.find(id);
        ASSERT_NOT_EQ(it, m_id_map.end());

        di::get<1>(*it).ref_count++;
        return id;
    }

    void drop_id(Id id) {
        auto it = m_id_map.find(id);
        ASSERT_NOT_EQ(it, m_id_map.end());

        auto& rc = di::get<1>(*it);
        if (--rc.ref_count == 0) {
            m_id_lookup.erase(rc.value);
            m_ids_used[id - 1] = false;
            m_id_map.erase(id);
        }
    }

private:
    auto get_key(T const& value) -> Key const& { return Ops::get_key(value); }

    auto allocate_id() -> di::Optional<Id> {
        // TODO: use an optimized method from di.

        // This for loop intentionally uses unsigned overflow, and starts from
        // id 1 to pervent ever allocating id 0.
        for (auto id = Id(1); id != 0; ++id) {
            if (!m_ids_used[id - 1]) {
                m_ids_used[id - 1] = true;
                return id;
            }
        }
        return {};
    }

    di::TreeMap<Id, RefCounted> m_id_map;
    di::TreeMap<Key, Id> m_id_lookup;
    di::BitSet<max_id> m_ids_used;
};
}
