#include "world.hpp"
#include "world_runtime.hpp"

#include <boost/typelayout.hpp>

#include <cassert>
#include <type_traits>

using namespace relocatable_world_demo;

template <typename T>
inline constexpr bool stored_type_contract_v =
    std::is_standard_layout_v<T> &&
    std::is_trivially_copyable_v<T> &&
    std::is_implicit_lifetime_v<T> &&
    alignof(T) <= 64;

static_assert(world_contract_admitted_v);
static_assert(stored_type_contract_v<char>);
static_assert(stored_type_contract_v<Position>);
static_assert(stored_type_contract_v<EntityKind>);
static_assert(stored_type_contract_v<region_string>);
static_assert(stored_type_contract_v<EntityRelativePtr>);
static_assert(stored_type_contract_v<Entity>);
static_assert(stored_type_contract_v<EntityIndexEntry>);
static_assert(stored_type_contract_v<region_vector<Entity>>);
static_assert(stored_type_contract_v<
    region_flat_map<std::uint64_t, std::uint32_t>>);
static_assert(stored_type_contract_v<region_vector<EntityRelativePtr>>);
static_assert(stored_type_contract_v<WorldSnapshot>);
static_assert(!std::is_copy_assignable_v<Entity>);
static_assert(!std::is_move_assignable_v<Entity>);
static_assert(!std::is_copy_assignable_v<WorldSnapshot>);
static_assert(!std::is_move_assignable_v<WorldSnapshot>);
static_assert(sizeof(EntityRelativePtr) == 4);
static_assert(alignof(EntityRelativePtr) == 4);
static_assert(sizeof(region_string) == 8 && alignof(region_string) == 4);
static_assert(sizeof(region_vector<Entity>) == 8 &&
              alignof(region_vector<Entity>) == 4);
static_assert(sizeof(region_flat_map<std::uint64_t, std::uint32_t>) == 8 &&
              alignof(region_flat_map<std::uint64_t, std::uint32_t>) == 4);
static_assert(sizeof(region_vector<EntityRelativePtr>) == 8 &&
              alignof(region_vector<EntityRelativePtr>) == 4);
static_assert(!boost::typelayout::get_layout_signature<EntityRelativePtr>()
    .contains(boost::typelayout::FixedString{"O("}));

int main() {
    RegionBuilder builder;
    const auto root = populate_canonical_world(builder);
    const auto& world = builder.get(root);
    assert(world.tick == 42);
    assert(world.entities.size() == 2);
    assert(world.entity_index.size() == 2);
    assert(world.party.size() == 2);
    assert(world.local_player.raw_offset_plus_one() != 0);
    static_cast<void>(world);
    auto buffer = std::move(builder).finish(root);
    assert(!buffer.is_validated());
    assert(buffer.used_bytes().size() <= region_capacity);
}
