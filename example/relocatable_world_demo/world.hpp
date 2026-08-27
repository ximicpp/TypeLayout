// world.hpp -- Stored world schema for the relocatable world demo.
//
// Copyright (c) 2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#ifndef RELOCATABLE_WORLD_DEMO_WORLD_HPP
#define RELOCATABLE_WORLD_DEMO_WORLD_HPP

#include "region.hpp"

#include <boost/typelayout.hpp>

#include <cstdint>

namespace relocatable_world_demo {

inline constexpr std::uint64_t hero_id = 1001;
inline constexpr std::uint64_t boss_id = 2001;
inline constexpr auto whole_region_profile =
    boost::typelayout::TransferProfile::whole_region_relocation;

struct Position {
    std::int32_t x;
    std::int32_t y;
};

enum class EntityKind : std::uint8_t { player, boss };

#if defined(TYPELAYOUT_RELOCATABLE_WORLD_PACKED_ENTITY)
#pragma pack(push, 1)
#endif
struct Entity {
    std::uint64_t id;
    EntityKind kind;
    Position position;
    std::int32_t hp;
    region_string name;
    relative_ptr<Entity> owner;
    relative_ptr<Entity> target;
};
#if defined(TYPELAYOUT_RELOCATABLE_WORLD_PACKED_ENTITY)
#pragma pack(pop)
#endif

using EntityIndexEntry = region_key_value<std::uint64_t, std::uint32_t>;
using EntityRelativePtr = relative_ptr<Entity>;

struct WorldSnapshot {
    std::uint64_t tick;
    region_vector<Entity> entities;
    region_flat_map<std::uint64_t, std::uint32_t> entity_index;
    region_vector<EntityRelativePtr> party;
    EntityRelativePtr local_player;
};

} // namespace relocatable_world_demo

namespace boost::typelayout::v1 {

template <>
struct region_relocation_traits<relocatable_world_demo::Entity> {
    static constexpr bool enabled =
        is_admitted_v<std::uint64_t,
            TransferProfile::whole_region_relocation> &&
        is_admitted_v<relocatable_world_demo::EntityKind,
            TransferProfile::whole_region_relocation> &&
        is_admitted_v<relocatable_world_demo::Position,
            TransferProfile::whole_region_relocation> &&
        is_admitted_v<std::int32_t,
            TransferProfile::whole_region_relocation> &&
        is_admitted_v<relocatable_world_demo::region_string,
            TransferProfile::whole_region_relocation> &&
        is_admitted_v<relocatable_world_demo::relative_ptr<
                relocatable_world_demo::Entity>,
            TransferProfile::whole_region_relocation>;
};

template <>
struct region_relocation_traits<relocatable_world_demo::WorldSnapshot> {
    static constexpr bool enabled =
        is_admitted_v<std::uint64_t,
            TransferProfile::whole_region_relocation> &&
        is_admitted_v<relocatable_world_demo::region_vector<
                relocatable_world_demo::Entity>,
            TransferProfile::whole_region_relocation> &&
        is_admitted_v<relocatable_world_demo::region_flat_map<
                std::uint64_t, std::uint32_t>,
            TransferProfile::whole_region_relocation> &&
        is_admitted_v<relocatable_world_demo::region_vector<
                relocatable_world_demo::EntityRelativePtr>,
            TransferProfile::whole_region_relocation> &&
        is_admitted_v<relocatable_world_demo::EntityRelativePtr,
            TransferProfile::whole_region_relocation>;
};

} // namespace boost::typelayout::v1

namespace relocatable_world_demo {

template <typename F>
constexpr void for_each_contract_type(F&& fn) {
    fn.template operator()<WorldSnapshot>("WorldSnapshot");
    fn.template operator()<Entity>("Entity");
    fn.template operator()<EntityRelativePtr>("EntityRelativePtr");
    fn.template operator()<EntityIndexEntry>("EntityIndexEntry");
}

inline constexpr bool world_contract_admitted_v =
    boost::typelayout::is_admitted_v<WorldSnapshot, whole_region_profile> &&
    boost::typelayout::is_admitted_v<Entity, whole_region_profile> &&
    boost::typelayout::is_admitted_v<EntityRelativePtr,
                                      whole_region_profile> &&
    boost::typelayout::is_admitted_v<EntityIndexEntry,
                                      whole_region_profile>;

} // namespace relocatable_world_demo

#endif // RELOCATABLE_WORLD_DEMO_WORLD_HPP
