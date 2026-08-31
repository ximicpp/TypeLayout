// unit_runtime.hpp -- Unit construction, validation, and registry operations.
//
// Copyright (c) 2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#ifndef BOOST_TYPELAYOUT_RELOCATABLE_UNIT_HANDOFF_DEMO_UNIT_RUNTIME_HPP
#define BOOST_TYPELAYOUT_RELOCATABLE_UNIT_HANDOFF_DEMO_UNIT_RUNTIME_HPP

#include "unit.hpp"

#include "../relocatable_region_support/region_storage.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <unordered_map>

namespace relocatable_unit_handoff_demo {

using relocatable_world_demo::RegionBuffer;
using relocatable_world_demo::RegionBuilder;

enum class unit_rejection_layer {
    envelope,
    region,
    graph
};

class unit_checkpoint_error : public std::runtime_error {
public:
    unit_checkpoint_error(unit_rejection_layer layer, const char* message)
        : std::runtime_error(message), layer_(layer) {}

    unit_rejection_layer layer() const noexcept { return layer_; }

private:
    unit_rejection_layer layer_;
};

struct UnitOffsets {
    std::uint32_t root_offset{};
    std::uint32_t name_data{};
    std::uint32_t effects_data{};
    std::uint32_t attributes_data{};
    std::uint32_t effect_order_data{};
    std::uint32_t selected_effect{};
    std::array<std::uint32_t, 2> effect_labels{};
    std::array<std::uint32_t, 2> effect_next{};
    std::array<std::uint32_t, 4> effect_order{};

    bool operator==(const UnitOffsets&) const = default;
};

relocatable_world_demo::region_handle<UnitSnapshot>
populate_canonical_migrating_unit(RegionBuilder& builder);
relocatable_world_demo::region_handle<UnitSnapshot>
populate_canonical_owner_unit(RegionBuilder& builder);

void validate_and_freeze_unit(RegionBuffer& buffer);
RegionBuffer build_canonical_migrating_unit();
RegionBuffer build_canonical_owner_unit();
const UnitSnapshot& unit_root(const RegionBuffer& buffer);
UnitOffsets capture_unit_offsets(const RegionBuffer& buffer);
bool canonical_migrating_unit_matches(const RegionBuffer& buffer,
                                      std::int32_t expected_hp);
void set_unit_hp(RegionBuffer& buffer, std::int32_t hp);

class UnitRegistry {
public:
    void attach(UnitId expected_id, RegionBuffer buffer);
    const UnitSnapshot* resolve(UnitId id) const;
    void set_hp(UnitId id, std::int32_t hp);
    std::size_t size() const noexcept { return buffers_.size(); }

private:
    std::unordered_map<UnitId, RegionBuffer> buffers_;
};

} // namespace relocatable_unit_handoff_demo

#endif // BOOST_TYPELAYOUT_RELOCATABLE_UNIT_HANDOFF_DEMO_UNIT_RUNTIME_HPP
