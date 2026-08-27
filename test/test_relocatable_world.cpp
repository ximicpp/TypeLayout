#include "world.hpp"
#include "world_runtime.hpp"

#include <boost/typelayout.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

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

namespace {

struct DescriptorRepresentation {
    std::uint32_t offset_plus_one;
    std::uint32_t size;
};

static_assert(sizeof(DescriptorRepresentation) == 8);
static_assert(alignof(DescriptorRepresentation) == 4);

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

template <typename T>
T read_object(std::span<const std::byte> bytes, std::size_t offset) {
    static_assert(std::is_trivially_copyable_v<T>);
    require(offset % alignof(T) == 0, "stored object is misaligned");
    require(offset <= bytes.size() && sizeof(T) <= bytes.size() - offset,
            "stored object is out of bounds");
    std::array<std::byte, sizeof(T)> representation{};
    std::memcpy(representation.data(), bytes.data() + offset, sizeof(T));
    return std::bit_cast<T>(representation);
}

template <typename T>
DescriptorRepresentation descriptor_representation(const T& descriptor) {
    static_assert(sizeof(T) == sizeof(DescriptorRepresentation));
    static_assert(alignof(T) == alignof(DescriptorRepresentation));
    DescriptorRepresentation result{};
    std::memcpy(&result, &descriptor, sizeof(result));
    return result;
}

std::size_t decode_non_null(std::uint32_t offset_plus_one) {
    require(offset_plus_one != 0, "stored offset must be non-null");
    return static_cast<std::size_t>(offset_plus_one - 1);
}

std::uint32_t encode_offset(std::size_t offset) {
    require(offset < std::numeric_limits<std::uint32_t>::max(),
            "stored offset is not representable");
    return static_cast<std::uint32_t>(offset + 1);
}

std::string_view read_text(std::span<const std::byte> bytes,
                           const region_string& text) {
    const auto representation = descriptor_representation(text);
    const auto offset = decode_non_null(representation.offset_plus_one);
    require(offset <= bytes.size() &&
                representation.size <= bytes.size() - offset,
            "stored text is out of bounds");
    return {reinterpret_cast<const char*>(bytes.data() + offset),
            representation.size};
}

} // namespace

void test_unvalidated_canonical_representation() {
    RegionBuilder builder;
    const auto root = populate_canonical_world(builder);
    const auto root_offset = decode_non_null(root.raw_offset_plus_one());
    auto buffer = std::move(builder).finish(root);
    require(!buffer.is_validated(), "constructed buffer must be unvalidated");
    const auto bytes = buffer.used_bytes();
    require(bytes.size() <= region_capacity,
            "constructed buffer exceeds region capacity");

    const auto world = read_object<WorldSnapshot>(bytes, root_offset);
    require(world.tick == 42, "world tick must be 42");

    const auto entities = descriptor_representation(world.entities);
    const auto index = descriptor_representation(world.entity_index);
    const auto party = descriptor_representation(world.party);
    require(entities.size == 2, "world must contain two entities");
    require(index.size == 2, "world index must contain two entries");
    require(party.size == 2, "world party must contain two entries");

    const auto entities_offset = decode_non_null(entities.offset_plus_one);
    const auto hero_offset = entities_offset;
    const auto boss_offset = entities_offset + sizeof(Entity);
    const auto hero = read_object<Entity>(bytes, hero_offset);
    const auto boss = read_object<Entity>(bytes, boss_offset);

    require(hero.id == hero_id, "Hero ID must be 1001");
    require(hero.kind == EntityKind::player, "Hero kind must be player");
    require(hero.position.x == 10 && hero.position.y == 20,
            "Hero position must be (10, 20)");
    require(hero.hp == 120, "Hero HP must be 120");
    require(read_text(bytes, hero.name) == "Hero", "Hero name must match");

    require(boss.id == boss_id, "Boss ID must be 2001");
    require(boss.kind == EntityKind::boss, "Boss kind must be boss");
    require(boss.position.x == 30 && boss.position.y == 40,
            "Boss position must be (30, 40)");
    require(boss.hp == 300, "Boss HP must be 300");
    require(read_text(bytes, boss.name) == "Boss", "Boss name must match");

    const auto index_offset = decode_non_null(index.offset_plus_one);
    const auto first_index = read_object<EntityIndexEntry>(bytes, index_offset);
    const auto second_index = read_object<EntityIndexEntry>(
        bytes, index_offset + sizeof(EntityIndexEntry));
    require(first_index.key == hero_id && first_index.value == 0,
            "first index entry must be {1001, 0}");
    require(second_index.key == boss_id && second_index.value == 1,
            "second index entry must be {2001, 1}");

    const auto hero_pointer = encode_offset(hero_offset);
    const auto boss_pointer = encode_offset(boss_offset);
    const auto party_offset = decode_non_null(party.offset_plus_one);
    const auto party_hero = read_object<EntityRelativePtr>(bytes, party_offset);
    const auto party_boss = read_object<EntityRelativePtr>(
        bytes, party_offset + sizeof(EntityRelativePtr));

    require(hero.owner.raw_offset_plus_one() == 0,
            "Hero owner must be null");
    require(boss.owner.raw_offset_plus_one() == hero_pointer,
            "Boss owner must point to Hero");
    require(world.local_player.raw_offset_plus_one() == hero_pointer,
            "local player must point to Hero");
    require(party_hero.raw_offset_plus_one() == hero_pointer,
            "party[0] must point to Hero");
    require(party_boss.raw_offset_plus_one() == boss_pointer,
            "party[1] must point to Boss");
    require(hero.target.raw_offset_plus_one() == boss_pointer,
            "Hero target must point to Boss");
    require(boss.target.raw_offset_plus_one() == hero_pointer,
            "Boss target must point to Hero");
}

namespace {

template <typename Exception, typename Function>
void require_throws(Function&& function, const char* message) {
    try {
        std::forward<Function>(function)();
    } catch (const Exception&) {
        return;
    }
    throw std::runtime_error(message);
}

void test_empty_world_validation() {
    RegionBuilder builder;
    const auto root = builder.make_object<WorldSnapshot>();
    auto buffer = std::move(builder).finish(root);

    require_throws<std::logic_error>([&] {
        static_cast<void>(world_root(buffer));
    }, "typed root access must fail before validation");

    validate_and_freeze_world(buffer);
    require(buffer.is_validated(), "empty world must validate");

    const auto& world = world_root(buffer);
    const auto view = buffer.view();
    require(view.elements(world.entities).empty(),
            "empty world must expose no entities");
    require(view.map(world.entity_index).begin() ==
                view.map(world.entity_index).end(),
            "empty world must expose an empty index");
    require(view.elements(world.party).empty(),
            "empty world must expose an empty party");
    require(view.resolve(world.local_player) == nullptr,
            "empty world must expose a null local player");
}

void test_one_entity_empty_name_validation() {
    static_assert(alignof(char) == 1,
                  "character payload alignment must be vacuous");

    RegionBuilder builder;
    const auto root = builder.make_object<WorldSnapshot>();
    const auto entities = builder.make_array<Entity>(1);
    const auto index_entries = builder.make_array<EntityIndexEntry>(1);
    const auto party = builder.make_array<EntityRelativePtr>(1);

    builder.bind(root, &WorldSnapshot::entities, entities);
    builder.bind(root, &WorldSnapshot::entity_index, index_entries);
    builder.bind(root, &WorldSnapshot::party, party);

    builder.set(entities, 0, &Entity::id, hero_id);
    builder.set(entities, 0, &Entity::kind, EntityKind::player);
    builder.set(entities, 0, &Entity::position, Position{1, 2});
    builder.set(entities, 0, &Entity::hp, std::int32_t{3});
    builder.assign(entities, 0, &Entity::name, "");
    builder.bind(entities, 0, &Entity::owner, region_handle<Entity>{});
    builder.bind(entities, 0, &Entity::target, region_handle<Entity>{});
    builder.set(index_entries, 0, EntityIndexEntry{hero_id, 0});

    const auto entity_handle = builder.element_handle(entities, 0);
    builder.bind(party, 0, entity_handle);
    builder.bind(root, &WorldSnapshot::local_player, entity_handle);

    auto buffer = std::move(builder).finish(root);
    validate_and_freeze_world(buffer);

    const auto& world = world_root(buffer);
    const auto view = buffer.view();
    const auto entity_view = view.elements(world.entities);
    const auto party_view = view.elements(world.party);
    const auto index_view = view.map(world.entity_index);
    require(entity_view.size() == 1,
            "one-entity world must expose its entity range");
    require(view.text(entity_view[0].name).empty(),
            "null/zero name must expose an empty string");
    require(party_view.size() == 1 &&
                view.resolve(party_view[0]) == &entity_view[0],
            "one-entity world must expose its party link");
    require(index_view.find(hero_id) != index_view.end() &&
                index_view.find(hero_id)->value == 0,
            "one-entity world must expose its index entry");
}

void test_canonical_typed_access_and_descriptor_provenance() {
    auto buffer = build_canonical_world();
    require(buffer.is_validated(), "canonical world must be validated");

    const auto& world = world_root(buffer);
    const auto view = buffer.view();
    const auto entities = view.elements(world.entities);
    const auto party = view.elements(world.party);
    const auto index = view.map(world.entity_index);

    require(world.tick == 42, "canonical typed root must expose tick 42");
    require(entities.size() == 2,
            "canonical typed view must expose both entities");
    require(view.text(entities[0].name) == "Hero" &&
                view.text(entities[1].name) == "Boss",
            "canonical typed view must expose both names");
    require(party.size() == 2,
            "canonical typed view must expose both party entries");
    require(index.begin() != index.end() &&
                index.find(hero_id) != index.end() &&
                index.find(hero_id)->value == 0 &&
                index.find(boss_id) != index.end() &&
                index.find(boss_id)->value == 1 &&
                index.find(9999) == index.end(),
            "canonical typed map must support binary-search lookup");
    require(view.resolve(entities[0].owner) == nullptr,
            "Hero owner must remain null");
    require(view.resolve(entities[1].owner) == &entities[0],
            "Boss owner must resolve to Hero");
    require(view.resolve(entities[0].target) == &entities[1] &&
                view.resolve(entities[1].target) == &entities[0],
            "canonical target cycle must resolve");
    require(view.resolve(party[0]) == &entities[0] &&
                view.resolve(party[1]) == &entities[1],
            "canonical party links must resolve");
    require(view.resolve(world.local_player) == &entities[0],
            "canonical local player must resolve to Hero");

    const auto stack_entities = world.entities;
    require_throws<std::invalid_argument>([&] {
        static_cast<void>(view.elements(stack_entities));
    }, "stack descriptor must be rejected before offset resolution");

    auto second_buffer = build_canonical_world();
    const auto& second_world = world_root(second_buffer);
    require_throws<std::invalid_argument>([&] {
        static_cast<void>(view.elements(second_world.entities));
    }, "foreign descriptor must be rejected before offset resolution");
}

void test_builder_capabilities_expire_without_mutation() {
    RegionBuilder builder;
    const auto root = builder.make_object<WorldSnapshot>();
    const auto entities = builder.make_array<Entity>(2);
    const auto index_entries = builder.make_array<EntityIndexEntry>(2);
    const auto party = builder.make_array<EntityRelativePtr>(2);

    builder.bind(root, &WorldSnapshot::entities, entities);
    builder.bind(root, &WorldSnapshot::entity_index, index_entries);
    builder.bind(root, &WorldSnapshot::party, party);
    builder.set(root, &WorldSnapshot::tick, std::uint64_t{42});

    builder.set(entities, 0, &Entity::id, hero_id);
    builder.set(entities, 0, &Entity::kind, EntityKind::player);
    builder.set(entities, 0, &Entity::position, Position{10, 20});
    builder.set(entities, 0, &Entity::hp, std::int32_t{120});
    builder.assign(entities, 0, &Entity::name, "Hero");

    builder.set(entities, 1, &Entity::id, boss_id);
    builder.set(entities, 1, &Entity::kind, EntityKind::boss);
    builder.set(entities, 1, &Entity::position, Position{30, 40});
    builder.set(entities, 1, &Entity::hp, std::int32_t{300});
    builder.assign(entities, 1, &Entity::name, "Boss");

    builder.set(index_entries, 0, EntityIndexEntry{hero_id, 0});
    builder.set(index_entries, 1, EntityIndexEntry{boss_id, 1});
    const auto hero = builder.element_handle(entities, 0);
    const auto boss = builder.element_handle(entities, 1);
    builder.bind(entities, 0, &Entity::owner, region_handle<Entity>{});
    builder.bind(entities, 1, &Entity::owner, hero);
    builder.bind(entities, 0, &Entity::target, boss);
    builder.bind(entities, 1, &Entity::target, hero);
    builder.bind(party, 0, hero);
    builder.bind(party, 1, boss);
    builder.bind(root, &WorldSnapshot::local_player, hero);

    auto buffer = std::move(builder).finish(root);
    validate_and_freeze_world(buffer);
    const std::vector<std::byte> before(buffer.used_bytes().begin(),
                                        buffer.used_bytes().end());

    require_throws<std::logic_error>([&] {
        builder.set(root, &WorldSnapshot::tick, std::uint64_t{99});
    }, "expired builder must reject ordinary root writes");
    require_throws<std::logic_error>([&] {
        builder.set(entities, 0, &Entity::hp, std::int32_t{999});
    }, "expired builder must reject ordinary entity writes");
    require_throws<std::logic_error>([&] {
        builder.set(index_entries, 0, EntityIndexEntry{boss_id, 0});
    }, "expired builder must reject ordinary index writes");
    require_throws<std::logic_error>([&] {
        builder.assign(entities, 0, &Entity::name, "Mutated");
    }, "expired builder must reject name writes");
    require_throws<std::logic_error>([&] {
        builder.bind(entities, 0, &Entity::target, hero);
    }, "expired builder must reject entity-link writes");
    require_throws<std::logic_error>([&] {
        builder.bind(root, &WorldSnapshot::local_player, boss);
    }, "expired builder must reject root-link writes");
    require_throws<std::logic_error>([&] {
        builder.bind(party, 0, boss);
    }, "expired builder must reject party-link writes");
    require_throws<std::logic_error>([&] {
        builder.bind(root, &WorldSnapshot::entities, entities);
    }, "expired builder must reject vector descriptor writes");
    require_throws<std::logic_error>([&] {
        builder.bind(root, &WorldSnapshot::entity_index, index_entries);
    }, "expired builder must reject map descriptor writes");

    require(buffer.used_bytes().size() == before.size() &&
                std::equal(before.begin(), before.end(),
                           buffer.used_bytes().begin()),
            "expired builder operations must leave payload bytes unchanged");

    const auto& world = world_root(buffer);
    const auto view = buffer.view();
    const auto entity_view = view.elements(world.entities);
    const auto party_view = view.elements(world.party);
    const auto index_view = view.map(world.entity_index);
    require(world.tick == 42 && entity_view.size() == 2 &&
                entity_view[0].id == hero_id &&
                entity_view[0].hp == 120 &&
                entity_view[1].id == boss_id &&
                entity_view[1].hp == 300,
            "expired builder operations must leave scalar values valid");
    require(view.text(entity_view[0].name) == "Hero" &&
                view.text(entity_view[1].name) == "Boss",
            "expired builder operations must leave names valid");
    require(index_view.find(hero_id) != index_view.end() &&
                index_view.find(hero_id)->value == 0 &&
                index_view.find(boss_id) != index_view.end() &&
                index_view.find(boss_id)->value == 1,
            "expired builder operations must leave the index valid");
    require(party_view.size() == 2 &&
                view.resolve(party_view[0]) == &entity_view[0] &&
                view.resolve(party_view[1]) == &entity_view[1] &&
                view.resolve(entity_view[0].target) == &entity_view[1] &&
                view.resolve(entity_view[1].target) == &entity_view[0] &&
                view.resolve(world.local_player) == &entity_view[0],
            "expired builder operations must leave graph links valid");
}

} // namespace

int main() {
    test_unvalidated_canonical_representation();
    test_empty_world_validation();
    test_one_entity_empty_name_validation();
    test_canonical_typed_access_and_descriptor_provenance();
    test_builder_capabilities_expire_without_mutation();
}
