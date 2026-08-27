// world_runtime.cpp -- Canonical world construction for the relocatable world
// demo.
//
// Copyright (c) 2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#include "world_runtime.hpp"

#include "checkpoint.hpp"

#include <limits>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace relocatable_world_demo {

struct WorldRegionAccess {
    static const WorldSnapshot& root(const RegionBuffer& buffer) {
        require_validated(buffer);
        return *std::launder(reinterpret_cast<const WorldSnapshot*>(
            buffer.storage_->bytes + buffer.root_offset_));
    }

    static void set_tick(RegionBuffer& buffer, std::uint64_t tick) {
        require_validated(buffer);
        auto* root = std::launder(reinterpret_cast<WorldSnapshot*>(
            buffer.storage_->bytes + buffer.root_offset_));
        root->tick = tick;
    }

    static void set_entity_hp(RegionBuffer& buffer,
                              std::uint64_t id,
                              std::int32_t hp) {
        require_validated(buffer);
        auto& entity = const_cast<Entity&>(find_entity(buffer, id));
        entity.hp = hp;
    }

private:
    static void require_validated(const RegionBuffer& buffer) {
        if (!buffer.is_validated()) {
            throw std::logic_error("region buffer has not been validated");
        }
    }
};

namespace {

struct OwningInterval {
    std::uint32_t begin;
    std::uint32_t end;
    std::size_t alignment;
    std::string_view label;
};

[[noreturn]] void reject_region(const char* message) {
    throw checkpoint_error(rejection_layer::region, message);
}

[[noreturn]] void reject_graph(const char* message) {
    throw checkpoint_error(rejection_layer::graph, message);
}

} // namespace

class WorldRegionValidator {
public:
    static void validate(RegionBuffer& buffer) {
        WorldRegionValidator validator(buffer);
        validator.run();
    }

private:
    explicit WorldRegionValidator(RegionBuffer& buffer)
        : buffer_(buffer) {}

    void run() {
        if (!buffer_.storage_) {
            throw std::logic_error("cannot validate a moved-from region buffer");
        }
        if (buffer_.state_ != RegionBuffer::state::constructed_unvalidated &&
            buffer_.state_ != RegionBuffer::state::copied_bytes_unvalidated) {
            throw std::logic_error("region buffer is not awaiting validation");
        }
        if (buffer_.used_bytes_ > region_capacity) {
            reject_region("used payload exceeds region capacity");
        }

        copied_bytes_ =
            buffer_.state_ == RegionBuffer::state::copied_bytes_unvalidated;
        validate_root();
        validate_root_ranges();
        start_entities();
        validate_and_start_names();
        start_index_and_party();
        validate_index();
        validate_graph();
        buffer_.state_ = RegionBuffer::state::validated;
    }

    void validate_root() {
        const auto begin = static_cast<std::size_t>(buffer_.root_offset_);
        if (begin % alignof(WorldSnapshot) != 0) {
            reject_region("world root is misaligned");
        }
        if (begin > std::numeric_limits<std::size_t>::max() -
                        sizeof(WorldSnapshot)) {
            reject_region("world root extent overflows");
        }
        const auto end = begin + sizeof(WorldSnapshot);
        if (end > buffer_.used_bytes_ || end > region_capacity) {
            reject_region("world root is outside the used payload");
        }

        root_interval_ = {
            checked_storage_offset(begin),
            checked_storage_offset(end),
            alignof(WorldSnapshot),
            "root"
        };
        intervals_.push_back(root_interval_);
        if (copied_bytes_) {
            root_ = std::start_lifetime_as<WorldSnapshot>(base() + begin);
        } else {
            root_ = std::launder(reinterpret_cast<WorldSnapshot*>(
                base() + begin));
        }
    }

    void validate_root_ranges() {
        entities_interval_ = validate_range(
            root_->entities.data_.raw_offset_plus_one(),
            root_->entities.size_, sizeof(Entity), alignof(Entity),
            "entities");
        index_interval_ = validate_range(
            root_->entity_index.entries_.data_.raw_offset_plus_one(),
            root_->entity_index.entries_.size_, sizeof(EntityIndexEntry),
            alignof(EntityIndexEntry), "index entries");
        party_interval_ = validate_range(
            root_->party.data_.raw_offset_plus_one(), root_->party.size_,
            sizeof(EntityRelativePtr), alignof(EntityRelativePtr), "party");
    }

    OwningInterval validate_range(std::uint32_t offset_plus_one,
                                  std::uint32_t count,
                                  std::size_t element_size,
                                  std::size_t alignment,
                                  std::string_view label) {
        if ((offset_plus_one == 0) != (count == 0)) {
            reject_region("region descriptor null/count invariant failed");
        }
        if (count == 0) {
            return {0, 0, alignment, label};
        }

        const auto begin = static_cast<std::size_t>(offset_plus_one - 1);
        if (begin % alignment != 0) {
            reject_region("region range is misaligned");
        }
        const auto count_size = static_cast<std::size_t>(count);
        if (count_size >
            std::numeric_limits<std::size_t>::max() / element_size) {
            reject_region("region range extent overflows");
        }
        const auto extent = count_size * element_size;
        if (begin > std::numeric_limits<std::size_t>::max() - extent) {
            reject_region("region range end overflows");
        }
        const auto end = begin + extent;
        if (end > buffer_.used_bytes_ || end > region_capacity) {
            reject_region("region range is outside the used payload");
        }

        OwningInterval interval{
            checked_storage_offset(begin),
            checked_storage_offset(end),
            alignment,
            label
        };
        for (const auto& reserved : intervals_) {
            if (interval.begin < reserved.end &&
                reserved.begin < interval.end) {
                reject_region("owning region ranges overlap");
            }
        }
        intervals_.push_back(interval);
        return interval;
    }

    void start_entities() {
        entities_ = start_array<Entity>(entities_interval_,
                                        root_->entities.size_);
    }

    void validate_and_start_names() {
        for (std::uint32_t index = 0;
             index != root_->entities.size_; ++index) {
            const auto interval = validate_range(
                entities_[index].name.data_.raw_offset_plus_one(),
                entities_[index].name.size_, sizeof(char), alignof(char),
                "entity name");
            if (copied_bytes_ && entities_[index].name.size_ != 0) {
                static_cast<void>(std::start_lifetime_as_array<char>(
                    base() + interval.begin,
                    static_cast<std::size_t>(entities_[index].name.size_)));
            }
        }
    }

    void start_index_and_party() {
        index_ = start_array<EntityIndexEntry>(
            index_interval_, root_->entity_index.entries_.size_);
        party_ = start_array<EntityRelativePtr>(
            party_interval_, root_->party.size_);
    }

    void validate_index() const {
        const auto entity_count = root_->entities.size_;
        const auto index_count = root_->entity_index.entries_.size_;
        if (index_count != entity_count) {
            reject_region("index size differs from entity count");
        }

        for (std::uint32_t index = 1; index < index_count; ++index) {
            if (!(index_[index - 1].key < index_[index].key)) {
                reject_region("index keys are not strictly increasing");
            }
        }

        for (std::uint32_t left = 0; left != entity_count; ++left) {
            for (std::uint32_t right = left + 1;
                 right != entity_count; ++right) {
                if (entities_[left].id == entities_[right].id) {
                    reject_region("entity IDs are not unique");
                }
            }
        }

        std::vector<unsigned char> covered(entity_count, 0);
        for (std::uint32_t index = 0; index != index_count; ++index) {
            const auto value = index_[index].value;
            if (value >= entity_count) {
                reject_region("index value is outside the entity array");
            }
            covered[value] = 1;
        }
        for (const auto entry : covered) {
            if (entry == 0) {
                reject_region("entity is missing from index coverage");
            }
        }
        for (std::uint32_t index = 0; index != index_count; ++index) {
            const auto value = index_[index].value;
            if (index_[index].key != entities_[value].id) {
                reject_region("index key differs from the referenced entity ID");
            }
        }
    }

    void validate_graph() const {
        for (std::uint32_t index = 0;
             index != root_->entities.size_; ++index) {
            validate_entity_link(entities_[index].owner);
            validate_entity_link(entities_[index].target);
        }
        for (std::uint32_t index = 0; index != root_->party.size_; ++index) {
            validate_entity_link(party_[index]);
        }
        validate_entity_link(root_->local_player);
    }

    void validate_entity_link(const EntityRelativePtr& pointer) const {
        const auto encoded = pointer.raw_offset_plus_one();
        if (encoded == 0) {
            return;
        }
        const auto offset = static_cast<std::size_t>(encoded - 1);
        if (offset >= buffer_.used_bytes_ || offset >= region_capacity) {
            reject_graph("entity link is outside the region");
        }
        if (offset % alignof(Entity) != 0) {
            reject_graph("entity link is misaligned");
        }
        if (root_->entities.size_ == 0 ||
            offset < entities_interval_.begin ||
            offset >= entities_interval_.end) {
            reject_graph("entity link does not target an entity");
        }
        const auto displacement = offset - entities_interval_.begin;
        if (displacement % sizeof(Entity) != 0) {
            reject_graph("entity link targets the middle of an entity");
        }
        const auto index = displacement / sizeof(Entity);
        if (index >= root_->entities.size_) {
            reject_graph("entity link does not target an entity start");
        }
    }

    template <typename T>
    T* start_array(const OwningInterval& interval, std::uint32_t count) {
        if (count == 0) {
            return nullptr;
        }
        if (copied_bytes_) {
            return std::start_lifetime_as_array<T>(
                base() + interval.begin, static_cast<std::size_t>(count));
        }
        return std::launder(reinterpret_cast<T*>(base() + interval.begin));
    }

    static std::uint32_t checked_storage_offset(std::size_t value) {
        if (value > std::numeric_limits<std::uint32_t>::max()) {
            reject_region("region offset is not representable");
        }
        return static_cast<std::uint32_t>(value);
    }

    std::byte* base() const noexcept {
        return buffer_.storage_->bytes;
    }

    RegionBuffer& buffer_;
    bool copied_bytes_ = false;
    std::vector<OwningInterval> intervals_;
    OwningInterval root_interval_{};
    OwningInterval entities_interval_{};
    OwningInterval index_interval_{};
    OwningInterval party_interval_{};
    WorldSnapshot* root_ = nullptr;
    Entity* entities_ = nullptr;
    EntityIndexEntry* index_ = nullptr;
    EntityRelativePtr* party_ = nullptr;
};

region_handle<WorldSnapshot> populate_canonical_world(RegionBuilder& builder) {
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

    builder.set(entities, 1, &Entity::id, boss_id);
    builder.set(entities, 1, &Entity::kind, EntityKind::boss);
    builder.set(entities, 1, &Entity::position, Position{30, 40});
    builder.set(entities, 1, &Entity::hp, std::int32_t{300});

    builder.assign(entities, 0, &Entity::name, "Hero");
    builder.assign(entities, 1, &Entity::name, "Boss");

    builder.set(index_entries, 0, EntityIndexEntry{hero_id, 0});
    builder.set(index_entries, 1, EntityIndexEntry{boss_id, 1});

    const auto hero_handle = builder.element_handle(entities, 0);
    const auto boss_handle = builder.element_handle(entities, 1);

    builder.bind(entities, 0, &Entity::owner, region_handle<Entity>{});
    builder.bind(entities, 1, &Entity::owner, hero_handle);
    builder.bind(entities, 0, &Entity::target, boss_handle);
    builder.bind(entities, 1, &Entity::target, hero_handle);
    builder.bind(party, 0, hero_handle);
    builder.bind(party, 1, boss_handle);
    builder.bind(root, &WorldSnapshot::local_player, hero_handle);

    return root;
}

void validate_and_freeze_world(RegionBuffer& buffer) {
    WorldRegionValidator::validate(buffer);
}

RegionBuffer build_canonical_world() {
    RegionBuilder builder;
    const auto root = populate_canonical_world(builder);
    auto buffer = std::move(builder).finish(root);
    validate_and_freeze_world(buffer);
    return buffer;
}

const WorldSnapshot& world_root(const RegionBuffer& buffer) {
    return WorldRegionAccess::root(buffer);
}

const Entity& find_entity(const RegionBuffer& buffer, std::uint64_t id) {
    const auto& world = world_root(buffer);
    const auto view = buffer.view();
    const auto index = view.map(world.entity_index);
    const auto found = index.find(id);
    if (found == index.end()) {
        throw std::out_of_range("entity ID was not found");
    }

    const auto entities = view.elements(world.entities);
    if (found->value >= entities.size()) {
        throw std::logic_error("validated entity index is out of range");
    }
    return entities[found->value];
}

std::array<std::uint32_t, 7> capture_world_offsets(
    const RegionBuffer& buffer) {
    const auto& world = world_root(buffer);
    const auto view = buffer.view();
    const auto party = view.elements(world.party);
    if (party.size() != 2) {
        throw std::logic_error("canonical party must contain two entries");
    }

    const auto& hero = find_entity(buffer, hero_id);
    const auto& boss = find_entity(buffer, boss_id);
    return {
        hero.owner.raw_offset_plus_one(),
        hero.target.raw_offset_plus_one(),
        boss.owner.raw_offset_plus_one(),
        boss.target.raw_offset_plus_one(),
        party[0].raw_offset_plus_one(),
        party[1].raw_offset_plus_one(),
        world.local_player.raw_offset_plus_one(),
    };
}

std::int32_t party_total_hp(const RegionBuffer& buffer) {
    const auto& world = world_root(buffer);
    const auto view = buffer.view();
    std::int64_t total = 0;
    for (const auto& pointer : view.elements(world.party)) {
        if (const auto* entity = view.resolve(pointer)) {
            total += entity->hp;
        }
    }
    if (total < std::numeric_limits<std::int32_t>::min() ||
        total > std::numeric_limits<std::int32_t>::max()) {
        throw std::overflow_error("party HP total is not representable");
    }
    return static_cast<std::int32_t>(total);
}

void set_world_tick(RegionBuffer& buffer, std::uint64_t tick) {
    WorldRegionAccess::set_tick(buffer, tick);
}

void set_entity_hp(RegionBuffer& buffer,
                   std::uint64_t id,
                   std::int32_t hp) {
    WorldRegionAccess::set_entity_hp(buffer, id, hp);
}

bool canonical_graph_matches(const RegionBuffer& buffer) {
    const auto& world = world_root(buffer);
    const auto view = buffer.view();
    const auto entities = view.elements(world.entities);
    const auto party = view.elements(world.party);
    const auto index = view.map(world.entity_index);
    if (entities.size() != 2 || party.size() != 2 ||
        world.entity_index.size() != 2) {
        return false;
    }

    const auto hero_entry = index.find(hero_id);
    const auto boss_entry = index.find(boss_id);
    if (hero_entry == index.end() || boss_entry == index.end() ||
        hero_entry->value >= entities.size() ||
        boss_entry->value >= entities.size()) {
        return false;
    }

    const auto& hero = entities[hero_entry->value];
    const auto& boss = entities[boss_entry->value];
    return hero.id == hero_id && boss.id == boss_id &&
        hero.kind == EntityKind::player && boss.kind == EntityKind::boss &&
        view.text(hero.name) == "Hero" && view.text(boss.name) == "Boss" &&
        view.resolve(hero.owner) == nullptr &&
        view.resolve(boss.owner) == &hero &&
        view.resolve(hero.target) == &boss &&
        view.resolve(boss.target) == &hero &&
        view.resolve(party[0]) == &hero &&
        view.resolve(party[1]) == &boss &&
        view.resolve(world.local_player) == &hero;
}

} // namespace relocatable_world_demo
