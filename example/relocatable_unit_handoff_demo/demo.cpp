// A self-contained unit-granularity handoff built from TypeLayout contracts and
// demo-local region representations. It does not implement a network protocol.

#include "agreement.hpp"
#include "sigs/unit_producer_ok.sig.hpp"
#include "sigs/unit_producer_packed.sig.hpp"
#include "unit_checkpoint.hpp"

#include <boost/typelayout.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string_view>
#include <vector>

namespace {

using namespace relocatable_unit_handoff_demo;

struct NativePointerUnit {
    Effect* effect;
};

static_assert(!boost::typelayout::is_admitted_v<
    NativePointerUnit,
    boost::typelayout::TransferProfile::whole_region_relocation>);

bool agreement_details_match(
    const std::array<named_agreement, 4>& details,
    const std::array<bool, 4>& expected) {
    constexpr std::array<std::string_view, 4> keys{
        "UnitSnapshot", "Effect", "EffectRelativePtr", "AttributeEntry"};
    for (std::size_t index = 0; index < keys.size(); ++index) {
        if (details[index].key != keys[index] ||
            details[index].matches != expected[index]) {
            return false;
        }
    }
    return true;
}

bool foreign_handle_rejected() {
    relocatable_world_demo::RegionBuilder first;
    const auto foreign_effect = first.make_object<Effect>();
    relocatable_world_demo::RegionBuilder second;
    const auto root = second.make_object<UnitSnapshot>();
    try {
        second.bind(root, &UnitSnapshot::selected_effect, foreign_effect);
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

bool corrupt_offset_rejected_at_graph(
    const std::vector<std::byte>& checkpoint,
    const UnitOffsets& offsets) {
    const auto decoded = decode_unit_checkpoint_envelope(checkpoint);
    std::vector<std::byte> payload(
        decoded.payload.begin(), decoded.payload.end());
    const auto next_offset = static_cast<std::size_t>(
        offsets.effects_data - 1 + offsetof(Effect, next));
    const std::uint32_t invalid =
        static_cast<std::uint32_t>(relocatable_world_demo::region_capacity + 1);
    std::memcpy(payload.data() + next_offset, &invalid, sizeof(invalid));
    const auto reencoded = encode_unit_checkpoint(payload, decoded.root_offset);
    try {
        auto unexpected = load_unit_checkpoint(reencoded);
        static_cast<void>(unexpected);
    } catch (const unit_checkpoint_error& error) {
        return error.layer() == unit_rejection_layer::graph;
    }
    return false;
}

} // namespace

int main() {
    try {
        if (!unit_contract_admitted_v) {
            return 1;
        }
        const auto normal =
            boost::typelayout::platform::unit_producer_ok::get_platform_info();
        const auto packed = boost::typelayout::platform::
            unit_producer_packed::get_platform_info();
        if (check_current_unit_agreement(normal) != agreement_result::match ||
            !agreement_details_match(current_unit_agreement_details(normal),
                                     {true, true, true, true}) ||
            check_current_unit_agreement(packed) != agreement_result::differ ||
            !agreement_details_match(current_unit_agreement_details(packed),
                                     {true, false, true, true})) {
            return 1;
        }

        auto source = build_canonical_migrating_unit();
        const auto offsets = capture_unit_offsets(source);
        const auto* source_base = source.used_bytes().data();
        const auto checkpoint = save_unit_checkpoint(source);
        auto loaded = load_unit_checkpoint(checkpoint);
        if (loaded.used_bytes().data() == source_base ||
            capture_unit_offsets(loaded) != offsets ||
            !canonical_migrating_unit_matches(loaded, 300)) {
            return 1;
        }

        UnitRegistry destination;
        destination.attach(owner_unit_id, build_canonical_owner_unit());
        destination.attach(migrating_unit_id, std::move(loaded));
        const auto* migrated = destination.resolve(migrating_unit_id);
        if (migrated == nullptr ||
            destination.resolve(migrated->owner_id) == nullptr ||
            destination.resolve(migrated->target_id) != nullptr) {
            return 1;
        }
        destination.set_hp(migrating_unit_id, 250);
        if (destination.resolve(migrating_unit_id)->hp != 250 ||
            destination.resolve(owner_unit_id)->hp != 500) {
            return 1;
        }

        constexpr bool native_pointer_rejected =
            !boost::typelayout::is_admitted_v<
                NativePointerUnit,
                boost::typelayout::TransferProfile::whole_region_relocation>;
        if (!native_pointer_rejected || !foreign_handle_rejected() ||
            !corrupt_offset_rejected_at_graph(checkpoint, offsets)) {
            return 1;
        }

        std::cout
            << "Unit contract: Admission PASS 4/4, Agreement MATCH\n"
            << "Transfer: source base != destination base, raw offsets "
               "unchanged\n"
            << "Containers: string=yes vector=yes flat_map=yes\n"
            << "Pointers: nullable=yes shared=yes cycle=yes "
               "pointer_vector=yes\n"
            << "Registry: owner 9001 RESOLVED, target 2001 UNRESOLVED\n"
            << "Business: unit 1001 attached, hp 300 -> 250\n"
            << '\n'
            << "Negative[native pointer]: Admission FAIL\n"
            << "Negative[packed Effect]: Agreement DIFFER\n"
            << "Negative[foreign handle]: builder REJECT\n"
            << "Negative[corrupt Effect::next]: graph REJECT before "
               "dereference\n";
        return 0;
    } catch (...) {
        return 1;
    }
}
