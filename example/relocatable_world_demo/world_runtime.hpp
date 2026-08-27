// world_runtime.hpp -- Canonical world construction for the relocatable world
// demo.
//
// Copyright (c) 2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#ifndef RELOCATABLE_WORLD_DEMO_WORLD_RUNTIME_HPP
#define RELOCATABLE_WORLD_DEMO_WORLD_RUNTIME_HPP

#include "region_storage.hpp"
#include "world.hpp"

#include <array>
#include <cstdint>

namespace relocatable_world_demo {

region_handle<WorldSnapshot> populate_canonical_world(RegionBuilder& builder);
void validate_and_freeze_world(RegionBuffer& buffer);
RegionBuffer build_canonical_world();
const WorldSnapshot& world_root(const RegionBuffer& buffer);
std::array<std::uint32_t, 7> capture_world_offsets(
    const RegionBuffer& buffer);
std::int32_t party_total_hp(const RegionBuffer& buffer);
const Entity& find_entity(const RegionBuffer& buffer, std::uint64_t id);
void set_world_tick(RegionBuffer& buffer, std::uint64_t tick);
void set_entity_hp(RegionBuffer& buffer,
                   std::uint64_t id,
                   std::int32_t hp);
bool canonical_graph_matches(const RegionBuffer& buffer);

} // namespace relocatable_world_demo

#endif // RELOCATABLE_WORLD_DEMO_WORLD_RUNTIME_HPP
