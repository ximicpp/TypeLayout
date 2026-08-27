// world_runtime.hpp -- Canonical world construction for the relocatable world
// demo.
//
// Copyright (c) 2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#ifndef RELOCATABLE_WORLD_DEMO_WORLD_RUNTIME_HPP
#define RELOCATABLE_WORLD_DEMO_WORLD_RUNTIME_HPP

#include "region_storage.hpp"
#include "world.hpp"

namespace relocatable_world_demo {

region_handle<WorldSnapshot> populate_canonical_world(RegionBuilder& builder);
void validate_and_freeze_world(RegionBuffer& buffer);
RegionBuffer build_canonical_world();
const WorldSnapshot& world_root(const RegionBuffer& buffer);

} // namespace relocatable_world_demo

#endif // RELOCATABLE_WORLD_DEMO_WORLD_RUNTIME_HPP
