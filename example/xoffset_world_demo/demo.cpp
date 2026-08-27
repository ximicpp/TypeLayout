#include "world.hpp"

#include "sigs/producer_ok.sig.hpp"
#include "sigs/producer_packed.sig.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

using XOffsetDatastructure::XBuffer;
using xoffset_world_demo::Entity;
using xoffset_world_demo::EntityKind;
using xoffset_world_demo::WorldSnapshot;

namespace {

constexpr std::uint64_t hero_id = 1001;
constexpr std::uint64_t boss_id = 2002;

[[noreturn]] void fail(std::string_view reason) {
    throw std::runtime_error(std::string(reason));
}

void require(bool condition, std::string_view reason) {
    if (!condition) {
        fail(reason);
    }
}

} // namespace

enum class AgreementResult { match, differ, incomplete };

struct NativePointerEntity {
    std::uint64_t id;
    xoffset_world_demo::Entity* target;
};

static_assert(boost::typelayout::source_context_v<NativePointerEntity> ==
              boost::typelayout::SourceContext::address_space_dependent);
static_assert(!boost::typelayout::is_byte_copy_safe_v<NativePointerEntity>);
static_assert(!boost::typelayout::is_admitted_v<
    NativePointerEntity,
    boost::typelayout::TransferProfile::whole_region_relocation>);

AgreementResult check_agreement(
    boost::typelayout::PlatformInfo producer);

const boost::typelayout::TypeEntry* find_fixture_entry(
    boost::typelayout::PlatformInfo producer,
    std::string_view key) {
    for (std::size_t i = 0; i < producer.type_count; ++i) {
        if (std::string_view(producer.types[i].name) == key) {
            return &producer.types[i];
        }
    }
    return nullptr;
}

template <typename T>
bool entry_matches(const boost::typelayout::TypeEntry& entry) {
    constexpr auto current = boost::typelayout::get_layout_signature<T>();
    return entry.byte_copy_safe &&
        std::string_view(entry.layout_sig) == std::string_view(current);
}

template <typename T>
bool fixture_entry_matches(
    boost::typelayout::PlatformInfo producer,
    std::string_view key) {
    const auto* entry = find_fixture_entry(producer, key);
    return entry != nullptr && entry_matches<T>(*entry);
}

AgreementResult check_agreement(
    boost::typelayout::PlatformInfo producer) {
    bool saw_missing = false;
    bool saw_difference = false;

    xoffset_world_demo::for_each_contract_type(
        [&]<typename T>(std::string_view key) {
            const auto* entry = find_fixture_entry(producer, key);
            if (entry == nullptr) {
                saw_missing = true;
            } else if (!entry_matches<T>(*entry)) {
                saw_difference = true;
            }
        });

    if (saw_missing) {
        return AgreementResult::incomplete;
    }
    return saw_difference ? AgreementResult::differ : AgreementResult::match;
}

XBuffer build_world() {
    auto buffer = XBuffer::create<WorldSnapshot>(4096);
    auto world = buffer.handle<WorldSnapshot>();

    world->entities.reserve(2);
    world->party.reserve(2);
    world->entity_index.reserve(2);

    world->entities.emplace_back();
    world->entities.emplace_back();
    world->party.emplace_back();
    world->party.emplace_back();

    world->entities[0].name = "Hero";
    world->entities[1].name = "Boss";

    world->tick = 42;
    world->entities[0].id = hero_id;
    world->entities[0].kind = EntityKind::player;
    world->entities[0].position = {10, 20};
    world->entities[0].hp = 120;
    world->entities[1].id = boss_id;
    world->entities[1].kind = EntityKind::boss;
    world->entities[1].position = {30, 40};
    world->entities[1].hp = 300;

    world->entity_index.emplace(hero_id, std::uint32_t{0});
    world->entity_index.emplace(boss_id, std::uint32_t{1});

    auto* frozen_world = world.get();
    auto* hero = &frozen_world->entities[0];
    auto* boss = &frozen_world->entities[1];
    const auto region = buffer.bytes();

    hero->owner.reset(nullptr, region);
    boss->owner.reset(hero, region);
    hero->target.reset(boss, region);
    boss->target.reset(hero, region);
    frozen_world->party[0].reset(hero, region);
    frozen_world->party[1].reset(boss, region);
    frozen_world->local_player.reset(hero, region);

    return buffer;
}

void validate_world_graph(XBuffer& buffer) {
    const auto region = buffer.bytes();
    const auto region_begin =
        reinterpret_cast<std::uintptr_t>(region.data());
    if (region.size() >
        std::numeric_limits<std::uintptr_t>::max() - region_begin) {
        fail("graph validation: region end overflows uintptr_t");
    }
    const auto region_end = region_begin + region.size();

    auto world_handle = buffer.handle<WorldSnapshot>();
    auto* world = world_handle.get();
    require(world != nullptr, "graph validation: world root is missing");

    std::vector<std::uintptr_t> entity_starts;
    entity_starts.reserve(world->entities.size());
    for (std::uint32_t i = 0; i < world->entities.size(); ++i) {
        entity_starts.push_back(
            reinterpret_cast<std::uintptr_t>(&world->entities[i]));
    }

    const auto validate_link =
        [&](const xoffset_world_demo::relative_ptr<Entity>& link,
            std::string_view label) {
            const auto delta = link.raw_delta();
            if (delta == 0) {
                return;
            }

            const auto anchor = reinterpret_cast<std::uintptr_t>(&link);
            const auto signed_delta = static_cast<std::int64_t>(delta);
            std::uintptr_t candidate = 0;
            if (signed_delta > 0) {
                const auto magnitude =
                    static_cast<std::uintptr_t>(signed_delta);
                if (anchor >
                    std::numeric_limits<std::uintptr_t>::max() - magnitude) {
                    throw std::runtime_error(
                        std::string(label) +
                        ": positive delta addition overflows uintptr_t");
                }
                candidate = anchor + magnitude;
            } else {
                const auto magnitude = static_cast<std::uintptr_t>(
                    -signed_delta);
                if (anchor < magnitude) {
                    throw std::runtime_error(
                        std::string(label) +
                        ": negative delta subtraction underflows uintptr_t");
                }
                candidate = anchor - magnitude;
            }

            if (candidate < region_begin || candidate >= region_end) {
                throw std::runtime_error(
                    std::string(label) + ": target is outside the region");
            }
            if (candidate % alignof(Entity) != 0) {
                throw std::runtime_error(
                    std::string(label) + ": target is misaligned for Entity");
            }

            bool matches_entity_start = false;
            for (const auto entity_start : entity_starts) {
                if (candidate == entity_start) {
                    matches_entity_start = true;
                    break;
                }
            }
            if (!matches_entity_start) {
                throw std::runtime_error(
                    std::string(label) +
                    ": target is not an exact live Entity start");
            }
        };

    for (std::uint32_t i = 0; i < world->entities.size(); ++i) {
        validate_link(world->entities[i].owner, "Entity.owner");
        validate_link(world->entities[i].target, "Entity.target");
    }
    for (std::uint32_t i = 0; i < world->party.size(); ++i) {
        validate_link(world->party[i], "party entry");
    }
    validate_link(world->local_player, "local_player");

    if (world->entity_index.size() != world->entities.size()) {
        fail("graph validation: entity_index size does not match entities");
    }
    for (std::uint32_t i = 0; i < world->entities.size(); ++i) {
        const auto id = world->entities[i].id;
        for (std::uint32_t earlier = 0; earlier < i; ++earlier) {
            if (world->entities[earlier].id == id) {
                fail("graph validation: duplicate entity ID");
            }
        }

        const auto entry = world->entity_index.find(id);
        if (entry == world->entity_index.end()) {
            fail("graph validation: entity ID is missing from entity_index");
        }
        if (entry->second != i) {
            fail("graph validation: entity_index maps ID to the wrong index");
        }
    }
}

std::array<std::int32_t, 7> capture_deltas(XBuffer& buffer) {
    auto world = buffer.handle<WorldSnapshot>();
    require(world->entities.size() == 2,
        "delta capture: expected exactly two entities");
    require(world->party.size() == 2,
        "delta capture: expected exactly two party entries");

    return {
        world->entities[0].owner.raw_delta(),
        world->entities[0].target.raw_delta(),
        world->entities[1].owner.raw_delta(),
        world->entities[1].target.raw_delta(),
        world->party[0].raw_delta(),
        world->party[1].raw_delta(),
        world->local_player.raw_delta(),
    };
}

std::int32_t party_total_hp(WorldSnapshot& world) {
    std::int32_t total = 0;
    for (auto& member_link : world.party) {
        auto* member = member_link.get();
        require(member != nullptr,
            "business query: party contains a null member");
        total += member->hp;
    }
    return total;
}

void run_positive_relocation() {
    auto source_a = build_world();
    validate_world_graph(source_a);
    const auto pre_save_deltas = capture_deltas(source_a);

    const auto checkpoint_a = source_a.save_verified<WorldSnapshot>();
    auto source_a_after_save = source_a.handle<WorldSnapshot>();
    require(source_a_after_save.get() != nullptr,
        "relocation: source A root was lost during save");
    validate_world_graph(source_a);
    const auto source_a_deltas = capture_deltas(source_a);
    require(source_a_deltas == pre_save_deltas,
        "relocation: source A deltas changed during save");
    const auto source_a_base =
        reinterpret_cast<std::uintptr_t>(source_a.get_address());

    auto loaded_b = XBuffer::load_verified<WorldSnapshot>(checkpoint_a);
    const auto loaded_b_base =
        reinterpret_cast<std::uintptr_t>(loaded_b.get_address());
    require(source_a_base != loaded_b_base,
        "relocation: source and loaded regions have the same base");
    validate_world_graph(loaded_b);
    const auto loaded_b_deltas = capture_deltas(loaded_b);
    require(loaded_b_deltas == source_a_deltas,
        "relocation: raw relative deltas changed after load");

    {
        auto world_b = loaded_b.handle<WorldSnapshot>();
        require(world_b->entities.size() == 2,
            "graph: expected exactly two entities");
        require(world_b->party.size() == 2,
            "graph: expected exactly two party entries");
        auto* hero = &world_b->entities[0];
        auto* boss = &world_b->entities[1];
        require(hero->owner.get() == nullptr,
            "graph: Hero.owner is not null");
        require(boss->owner.get() == hero,
            "graph: Boss.owner does not resolve to Hero");
        require(hero->target.get() == boss,
            "graph: Hero.target does not resolve to Boss");
        require(boss->target.get() == hero,
            "graph: Boss.target does not complete the cycle");
        require(world_b->party[0].get() == hero &&
                world_b->party[1].get() == boss,
            "graph: party container links resolve incorrectly");
        require(world_b->local_player.get() == hero &&
                boss->owner.get() == world_b->party[0].get(),
            "graph: shared Hero target is not preserved");

        const auto initial_party_hp = party_total_hp(*world_b);
        require(initial_party_hp == 420,
            "business query: initial party HP is not 420");
        require(world_b->tick == 42,
            "business mutation: initial tick is not 42");
        world_b->tick = 43;

        const auto boss_entry = world_b->entity_index.find(boss_id);
        require(boss_entry != world_b->entity_index.end(),
            "business mutation: Boss ID is missing from entity_index");
        auto& indexed_boss = world_b->entities[boss_entry->second];
        require(indexed_boss.id == boss_id && indexed_boss.hp == 300,
            "business mutation: indexed Boss does not have HP 300");
        indexed_boss.hp = 250;
    }

    const auto checkpoint_b = loaded_b.save_verified<WorldSnapshot>();
    auto loaded_b_after_save = loaded_b.handle<WorldSnapshot>();
    require(loaded_b_after_save.get() != nullptr,
        "reload: source B root was lost during save");
    validate_world_graph(loaded_b);

    auto loaded_c = XBuffer::load_verified<WorldSnapshot>(checkpoint_b);
    validate_world_graph(loaded_c);
    auto world_c = loaded_c.handle<WorldSnapshot>();
    require(world_c->tick == 43,
        "reload: tick mutation did not persist");
    const auto reloaded_boss_entry = world_c->entity_index.find(boss_id);
    require(reloaded_boss_entry != world_c->entity_index.end(),
        "reload: Boss ID is missing from entity_index");
    require(world_c->entities[reloaded_boss_entry->second].hp == 250,
        "reload: Boss HP mutation did not persist");

    std::printf("Relocation: base changed, raw deltas unchanged\n");
    std::printf("Graph: null + shared + cycle + pointer container PASS\n");
    std::printf("Business: party_hp=420, tick=42->43, boss_hp=300->250\n");
    std::printf("Reload: mutation persisted\n");
}

void run_native_pointer_negative() {
    require(boost::typelayout::source_context_v<NativePointerEntity> ==
            boost::typelayout::SourceContext::address_space_dependent,
        "native pointer: source context is not address-space-dependent");
    require(!boost::typelayout::is_byte_copy_safe_v<NativePointerEntity>,
        "native pointer: representation unexpectedly became byte-copy safe");
    require(!boost::typelayout::is_admitted_v<
            NativePointerEntity,
            boost::typelayout::TransferProfile::whole_region_relocation>,
        "native pointer: whole-region Admission unexpectedly passed");

    std::printf("Negative[native pointer]: Admission FAIL, load skipped\n");
}

void run_packed_agreement_negative() {
    const auto packed_info =
        boost::typelayout::platform::producer_packed::get_platform_info();
    require(check_agreement(packed_info) == AgreementResult::differ,
        "packed Entity: Agreement unexpectedly matched");
    require(
        fixture_entry_matches<xoffset_world_demo::WorldSnapshot>(
            packed_info, "WorldSnapshot") &&
        !fixture_entry_matches<xoffset_world_demo::Entity>(
            packed_info, "Entity") &&
        fixture_entry_matches<xoffset_world_demo::EntityRelativePtr>(
            packed_info, "EntityRelativePtr") &&
        fixture_entry_matches<xoffset_world_demo::EntityIndexEntry>(
            packed_info, "EntityIndexEntry"),
        "packed Entity: Agreement difference pattern is not Entity-only");

    std::printf(
        "Negative[producer packing ABI drift]: Agreement DIFFER, load skipped\n");
}

void run_corrupt_delta_negative() {
    auto source = build_world();
    validate_world_graph(source);

    auto wire = source.save_verified<WorldSnapshot>();
    auto world_after_save = source.handle<WorldSnapshot>();
    require(world_after_save.get() != nullptr,
        "corrupt rel32: source root was lost during save");
    validate_world_graph(source);

    const auto live_region = source.bytes();
    const auto* local_player_bytes = reinterpret_cast<const std::byte*>(
        &world_after_save->local_player);
    require(local_player_bytes >= live_region.data() &&
            local_player_bytes + sizeof(std::int32_t) <=
                live_region.data() + live_region.size(),
        "corrupt rel32: local_player is outside the live region");
    const auto payload_offset = static_cast<std::size_t>(
        local_player_bytes - live_region.data());
    const auto wire_offset =
        sizeof(XOffsetDatastructure::XWireHeaderV1) + payload_offset;
    require(wire_offset + sizeof(std::int32_t) <= wire.size(),
        "corrupt rel32: local_player is outside the wire payload");

    const auto corrupt_delta = std::numeric_limits<std::int32_t>::max();
    std::memcpy(wire.data() + wire_offset,
        &corrupt_delta, sizeof(corrupt_delta));

    auto loaded = XBuffer::load_verified<WorldSnapshot>(wire);
    bool graph_rejected = false;
    try {
        validate_world_graph(loaded);
    } catch (const std::runtime_error&) {
        graph_rejected = true;
    }
    require(graph_rejected,
        "corrupt rel32: graph validator unexpectedly accepted the delta");

    std::printf(
        "Negative[corrupt rel32]: graph REJECT before dereference\n");
}

int main() {
    static_assert(xoffset_world_demo::world_contract_admitted_v);

    const auto ok = check_agreement(
        boost::typelayout::platform::producer_ok::get_platform_info());
    if (ok != AgreementResult::match) {
        return 1;
    }

    xoffset_world_demo::relative_ptr<xoffset_world_demo::Entity> null_entity;
    const auto& const_null_entity = null_entity;
    if (null_entity.get() != nullptr || const_null_entity.get() != nullptr) {
        std::fprintf(stderr, "relative_ptr null resolution failed\n");
        return 1;
    }

    std::printf("Admission[whole_region_relocation]: PASS\n");
    std::printf("Agreement[producer_ok, 4 types]: MATCH\n");
    run_positive_relocation();
    run_native_pointer_negative();
    run_packed_agreement_negative();
    run_corrupt_delta_negative();
    return 0;
}
