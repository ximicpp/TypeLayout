// A self-contained teaching example inspired by offset-based arena and
// checkpoint designs, including XOffsetDatastructure. It is not
// XOffsetDatastructure and does not implement or validate its wire format.

#include "agreement.hpp"
#include "checkpoint.hpp"
#include "sigs/producer_ok.sig.hpp"
#include "sigs/producer_packed.sig.hpp"
#include "world_runtime.hpp"

#include <boost/typelayout.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <string_view>
#include <vector>

namespace {

using namespace relocatable_world_demo;

struct NativePointerEntity {
    std::uint64_t id;
    Entity* target;
};

static_assert(!boost::typelayout::is_admitted_v<
    NativePointerEntity,
    boost::typelayout::TransferProfile::whole_region_relocation>);

constexpr std::array<std::string_view, 4> agreement_keys{
    "WorldSnapshot",
    "Entity",
    "EntityRelativePtr",
    "EntityIndexEntry",
};

bool agreement_details_match(
    const std::array<named_agreement, 4>& details,
    const std::array<bool, 4>& expected) {
    for (std::size_t index = 0; index < details.size(); ++index) {
        if (details[index].key != agreement_keys[index] ||
            details[index].matches != expected[index]) {
            return false;
        }
    }
    return true;
}

std::uint32_t read_u32_le(std::span<const std::byte> bytes,
                          std::size_t offset) {
    if (offset > bytes.size() || 4 > bytes.size() - offset) {
        throw checkpoint_error(rejection_layer::envelope,
                               "checkpoint field is out of bounds");
    }
    return static_cast<std::uint32_t>(
               std::to_integer<unsigned char>(bytes[offset])) |
        static_cast<std::uint32_t>(
            std::to_integer<unsigned char>(bytes[offset + 1])) << 8 |
        static_cast<std::uint32_t>(
            std::to_integer<unsigned char>(bytes[offset + 2])) << 16 |
        static_cast<std::uint32_t>(
            std::to_integer<unsigned char>(bytes[offset + 3])) << 24;
}

void write_u32_le(std::span<std::byte> bytes,
                  std::size_t offset,
                  std::uint32_t value) {
    if (offset > bytes.size() || 4 > bytes.size() - offset) {
        throw checkpoint_error(rejection_layer::envelope,
                               "checkpoint field is out of bounds");
    }
    bytes[offset] = std::byte{static_cast<unsigned char>(value)};
    bytes[offset + 1] = std::byte{static_cast<unsigned char>(value >> 8)};
    bytes[offset + 2] = std::byte{static_cast<unsigned char>(value >> 16)};
    bytes[offset + 3] = std::byte{static_cast<unsigned char>(value >> 24)};
}

bool corrupt_offset_rejected_at_graph(std::vector<std::byte> checkpoint) {
    const auto root_offset = read_u32_le(checkpoint, 20);
    const auto local_player_offset = checkpoint_header_size +
        static_cast<std::size_t>(root_offset) +
        offsetof(WorldSnapshot, local_player);
    write_u32_le(checkpoint, local_player_offset, 0xffffffffu);

    try {
        auto unexpected = load_checkpoint(checkpoint);
        static_cast<void>(unexpected);
    } catch (const checkpoint_error& error) {
        return error.layer() == rejection_layer::graph;
    }
    return false;
}

} // namespace

int main() {
    try {
        if (!world_contract_admitted_v) {
            return 1;
        }

        const auto normal =
            boost::typelayout::platform::producer_ok::get_platform_info();
        if (check_current_agreement(normal) != agreement_result::match ||
            !agreement_details_match(current_agreement_details(normal),
                                     {true, true, true, true})) {
            return 1;
        }

        auto source_a = build_canonical_world();
        const auto offsets_a = capture_world_offsets(source_a);
        const auto* base_a = source_a.used_bytes().data();
        const auto checkpoint_a = save_checkpoint(source_a);

        auto loaded_b = load_checkpoint(checkpoint_a);
        if (loaded_b.used_bytes().data() == base_a ||
            capture_world_offsets(loaded_b) != offsets_a ||
            !canonical_graph_matches(loaded_b)) {
            return 1;
        }

        const auto initial_party_hp = party_total_hp(loaded_b);
        const auto initial_tick = world_root(loaded_b).tick;
        const auto initial_boss_hp = find_entity(loaded_b, boss_id).hp;
        if (initial_party_hp != 420 || initial_tick != 42 ||
            initial_boss_hp != 300) {
            return 1;
        }

        set_world_tick(loaded_b, 43);
        set_entity_hp(loaded_b, boss_id, 250);
        if (world_root(loaded_b).tick != 43 ||
            find_entity(loaded_b, boss_id).hp != 250 ||
            capture_world_offsets(loaded_b) != offsets_a ||
            !canonical_graph_matches(loaded_b)) {
            return 1;
        }

        auto loaded_c = load_checkpoint(save_checkpoint(loaded_b));
        if (world_root(loaded_c).tick != 43 ||
            find_entity(loaded_c, boss_id).hp != 250 ||
            capture_world_offsets(loaded_c) != offsets_a ||
            !canonical_graph_matches(loaded_c)) {
            return 1;
        }

        constexpr bool native_pointer_rejected =
            !boost::typelayout::is_admitted_v<
                NativePointerEntity,
                boost::typelayout::TransferProfile::whole_region_relocation>;
        if (!native_pointer_rejected) {
            return 1;
        }
        const auto packed =
            boost::typelayout::platform::producer_packed::get_platform_info();
        if (check_current_agreement(packed) != agreement_result::differ ||
            !agreement_details_match(current_agreement_details(packed),
                                     {true, false, true, true})) {
            return 1;
        }
        if (!corrupt_offset_rejected_at_graph(checkpoint_a)) {
            return 1;
        }

        std::cout
            << "Admission[whole_region_relocation]: PASS\n"
            << "Agreement[producer_ok, 4 types]: MATCH\n"
            << "Relocation: base changed, raw offsets unchanged\n"
            << "Graph: null + shared + cycle + pointer container PASS\n"
            << "Business: party_hp=420, tick=42->43, boss_hp=300->250\n"
            << "Reload: mutation persisted\n"
            << '\n'
            << "Negative[native pointer]: Admission FAIL, load skipped\n"
            << "Negative[packed Entity]: Agreement DIFFER, load skipped\n"
            << "Negative[corrupt region offset]: graph REJECT before "
               "dereference\n";
        return 0;
    } catch (...) {
        return 1;
    }
}
