// unit.hpp -- Stored unit schema for the relocatable handoff demo.
//
// Copyright (c) 2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#ifndef BOOST_TYPELAYOUT_RELOCATABLE_UNIT_HANDOFF_DEMO_UNIT_HPP
#define BOOST_TYPELAYOUT_RELOCATABLE_UNIT_HANDOFF_DEMO_UNIT_HPP

#include "../relocatable_region_support/region.hpp"

#include <boost/typelayout.hpp>

#include <cstdint>
#include <string_view>

namespace relocatable_unit_handoff_demo {

using relocatable_world_demo::region_flat_map;
using relocatable_world_demo::region_key_value;
using relocatable_world_demo::region_string;
using relocatable_world_demo::region_vector;
using relocatable_world_demo::relative_ptr;

using UnitId = std::uint64_t;

inline constexpr UnitId migrating_unit_id = 1001;
inline constexpr UnitId owner_unit_id = 9001;
inline constexpr UnitId unresolved_target_id = 2001;
inline constexpr auto whole_region_profile =
    boost::typelayout::TransferProfile::whole_region_relocation;

struct UnitPosition {
    std::int32_t x;
    std::int32_t y;
};

enum class EffectKind : std::uint8_t {
    shield,
    haste
};

struct Effect;
using EffectRelativePtr = relative_ptr<Effect>;

#if defined(TYPELAYOUT_RELOCATABLE_UNIT_PACKED_EFFECT)
#pragma pack(push, 1)
#endif
struct Effect {
    std::uint32_t id;
    EffectKind kind;
    std::int32_t magnitude;
    region_string label;
    EffectRelativePtr next;
};
#if defined(TYPELAYOUT_RELOCATABLE_UNIT_PACKED_EFFECT)
#pragma pack(pop)
#endif

using AttributeEntry = region_key_value<std::uint32_t, std::int32_t>;

struct UnitSnapshot {
    UnitId id;
    UnitId owner_id;
    UnitId target_id;
    UnitPosition position;
    std::int32_t hp;
    region_string name;
    region_vector<Effect> effects;
    region_flat_map<std::uint32_t, std::int32_t> attributes;
    region_vector<EffectRelativePtr> effect_order;
    EffectRelativePtr selected_effect;
};

} // namespace relocatable_unit_handoff_demo

namespace boost::typelayout::v1 {

template <>
struct region_relocation_traits<
    relocatable_unit_handoff_demo::Effect> {
    static constexpr bool enabled =
        is_admitted_v<std::uint32_t,
            TransferProfile::whole_region_relocation> &&
        is_admitted_v<relocatable_unit_handoff_demo::EffectKind,
            TransferProfile::whole_region_relocation> &&
        is_admitted_v<std::int32_t,
            TransferProfile::whole_region_relocation> &&
        is_admitted_v<relocatable_world_demo::region_string,
            TransferProfile::whole_region_relocation> &&
        is_admitted_v<relocatable_unit_handoff_demo::EffectRelativePtr,
            TransferProfile::whole_region_relocation>;
};

template <>
struct region_relocation_traits<
    relocatable_unit_handoff_demo::UnitSnapshot> {
    static constexpr bool enabled =
        is_admitted_v<relocatable_unit_handoff_demo::UnitId,
            TransferProfile::whole_region_relocation> &&
        is_admitted_v<relocatable_unit_handoff_demo::UnitPosition,
            TransferProfile::whole_region_relocation> &&
        is_admitted_v<std::int32_t,
            TransferProfile::whole_region_relocation> &&
        is_admitted_v<relocatable_world_demo::region_string,
            TransferProfile::whole_region_relocation> &&
        is_admitted_v<relocatable_world_demo::region_vector<
                relocatable_unit_handoff_demo::Effect>,
            TransferProfile::whole_region_relocation> &&
        is_admitted_v<relocatable_world_demo::region_flat_map<
                std::uint32_t, std::int32_t>,
            TransferProfile::whole_region_relocation> &&
        is_admitted_v<relocatable_world_demo::region_vector<
                relocatable_unit_handoff_demo::EffectRelativePtr>,
            TransferProfile::whole_region_relocation> &&
        is_admitted_v<relocatable_unit_handoff_demo::EffectRelativePtr,
            TransferProfile::whole_region_relocation>;
};

} // namespace boost::typelayout::v1

namespace relocatable_unit_handoff_demo {

template <typename F>
constexpr void for_each_unit_contract_type(F&& fn) {
    fn.template operator()<UnitSnapshot>("UnitSnapshot");
    fn.template operator()<Effect>("Effect");
    fn.template operator()<EffectRelativePtr>("EffectRelativePtr");
    fn.template operator()<AttributeEntry>("AttributeEntry");
}

inline constexpr bool unit_contract_admitted_v =
    boost::typelayout::is_admitted_v<UnitSnapshot, whole_region_profile> &&
    boost::typelayout::is_admitted_v<Effect, whole_region_profile> &&
    boost::typelayout::is_admitted_v<EffectRelativePtr,
                                      whole_region_profile> &&
    boost::typelayout::is_admitted_v<AttributeEntry, whole_region_profile>;

} // namespace relocatable_unit_handoff_demo

#endif // BOOST_TYPELAYOUT_RELOCATABLE_UNIT_HANDOFF_DEMO_UNIT_HPP
