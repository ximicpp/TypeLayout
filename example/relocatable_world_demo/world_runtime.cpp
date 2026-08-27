// world_runtime.cpp -- Canonical world construction for the relocatable world
// demo.
//
// Copyright (c) 2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#include "world_runtime.hpp"

namespace relocatable_world_demo {

region_handle<WorldSnapshot> populate_canonical_world(RegionBuilder& builder) {
    const auto root = builder.make_object<WorldSnapshot>();
    const auto entities = builder.make_array<Entity>(2);
    const auto index_entries = builder.make_array<EntityIndexEntry>(2);
    const auto party = builder.make_array<EntityRelativePtr>(2);

    builder.bind(builder.get(root).entities, entities);
    builder.bind(builder.get(root).entity_index, index_entries);
    builder.bind(builder.get(root).party, party);

    auto& hero = builder.at(entities, 0);
    hero.id = hero_id;
    hero.kind = EntityKind::player;
    hero.position = {10, 20};
    hero.hp = 120;

    auto& boss = builder.at(entities, 1);
    boss.id = boss_id;
    boss.kind = EntityKind::boss;
    boss.position = {30, 40};
    boss.hp = 300;

    builder.assign(hero.name, "Hero");
    builder.assign(boss.name, "Boss");

    builder.at(index_entries, 0) = {hero_id, 0};
    builder.at(index_entries, 1) = {boss_id, 1};

    const auto hero_handle = builder.element_handle(entities, 0);
    const auto boss_handle = builder.element_handle(entities, 1);

    builder.bind(hero.owner, region_handle<Entity>{});
    builder.bind(boss.owner, hero_handle);
    builder.bind(hero.target, boss_handle);
    builder.bind(boss.target, hero_handle);
    builder.bind(builder.at(party, 0), hero_handle);
    builder.bind(builder.at(party, 1), boss_handle);
    builder.bind(builder.get(root).local_player, hero_handle);

    return root;
}

} // namespace relocatable_world_demo
