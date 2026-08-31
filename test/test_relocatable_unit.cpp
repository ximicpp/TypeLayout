#include "unit_runtime.hpp"

#include <boost/typelayout.hpp>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

using namespace boost::typelayout;
using namespace relocatable_unit_handoff_demo;

struct NativePointerUnit {
    Effect* effect;
};

static_assert(std::is_standard_layout_v<UnitPosition>);
static_assert(std::is_trivially_copyable_v<UnitPosition>);
static_assert(std::is_implicit_lifetime_v<UnitPosition>);
static_assert(std::is_standard_layout_v<Effect>);
static_assert(std::is_trivially_copyable_v<Effect>);
static_assert(std::is_implicit_lifetime_v<Effect>);
static_assert(std::is_standard_layout_v<UnitSnapshot>);
static_assert(std::is_trivially_copyable_v<UnitSnapshot>);
static_assert(std::is_implicit_lifetime_v<UnitSnapshot>);
static_assert(is_admitted_v<UnitSnapshot, whole_region_profile>);
static_assert(is_admitted_v<Effect, whole_region_profile>);
static_assert(is_admitted_v<EffectRelativePtr, whole_region_profile>);
static_assert(is_admitted_v<AttributeEntry, whole_region_profile>);
static_assert(unit_contract_admitted_v);
static_assert(!is_admitted_v<NativePointerUnit, whole_region_profile>);

inline constexpr auto effect_pointer_signature =
    get_layout_signature<EffectRelativePtr>();
inline constexpr auto effect_signature = get_layout_signature<Effect>();
static_assert(effect_pointer_signature.length() != 0);
static_assert(effect_signature.length() != 0);

namespace {

void expect(bool condition) {
    if (!condition) {
        std::abort();
    }
}

template <typename Exception, typename Function>
void expect_throws(Function&& function) {
    try {
        std::forward<Function>(function)();
    } catch (const Exception&) {
        return;
    }
    std::abort();
}

void test_canonical_unit_region() {
    auto source = build_canonical_migrating_unit();
    expect(canonical_migrating_unit_matches(source, 300));
    const auto source_offsets = capture_unit_offsets(source);

    const std::vector<std::byte> payload(
        source.used_bytes().begin(), source.used_bytes().end());
    auto relocated = relocatable_world_demo::RegionValidationAccess::
        copied_buffer(payload, source_offsets.root_offset);
    expect(source.used_bytes().data() != relocated.used_bytes().data());
    validate_and_freeze_unit(relocated);
    expect(canonical_migrating_unit_matches(relocated, 300));
    expect(capture_unit_offsets(relocated) == source_offsets);
}

void test_registry_resolution_and_mutation() {
    UnitRegistry registry;
    registry.attach(owner_unit_id, build_canonical_owner_unit());
    const auto* owner_before = registry.resolve(owner_unit_id);
    expect(owner_before != nullptr && owner_before->hp == 500);

    registry.attach(migrating_unit_id, build_canonical_migrating_unit());
    expect(registry.size() == 2);
    const auto* migrated = registry.resolve(migrating_unit_id);
    expect(migrated != nullptr);
    expect(registry.resolve(migrated->owner_id) == owner_before);
    expect(registry.resolve(migrated->target_id) == nullptr);

    registry.set_hp(migrating_unit_id, 250);
    expect(registry.resolve(migrating_unit_id)->hp == 250);
    expect(registry.resolve(owner_unit_id)->hp == 500);
    expect_throws<std::out_of_range>([&] {
        registry.set_hp(unresolved_target_id, 1);
    });
    expect_throws<std::invalid_argument>([&] {
        registry.attach(migrating_unit_id,
                        build_canonical_migrating_unit());
    });
    expect_throws<std::invalid_argument>([&] {
        registry.attach(unresolved_target_id,
                        build_canonical_owner_unit());
    });
}

void test_foreign_effect_handle_rejected() {
    relocatable_world_demo::RegionBuilder first;
    const auto foreign_effect = first.make_object<Effect>();

    relocatable_world_demo::RegionBuilder second;
    const auto root = second.make_object<UnitSnapshot>();
    expect_throws<std::invalid_argument>([&] {
        second.bind(root, &UnitSnapshot::selected_effect, foreign_effect);
    });
}

void test_corrupt_effect_pointer_rejected_as_graph() {
    auto source = build_canonical_migrating_unit();
    const auto offsets = capture_unit_offsets(source);
    std::vector<std::byte> payload(
        source.used_bytes().begin(), source.used_bytes().end());
    const auto next_offset = static_cast<std::size_t>(
        offsets.effects_data - 1 + offsetof(Effect, next));
    const std::uint32_t invalid =
        static_cast<std::uint32_t>(relocatable_world_demo::region_capacity + 1);
    std::memcpy(payload.data() + next_offset, &invalid, sizeof(invalid));

    auto corrupt = relocatable_world_demo::RegionValidationAccess::
        copied_buffer(payload, offsets.root_offset);
    try {
        validate_and_freeze_unit(corrupt);
    } catch (const unit_checkpoint_error& error) {
        expect(error.layer() == unit_rejection_layer::graph);
        expect(!corrupt.is_validated());
        expect_throws<std::logic_error>([&] {
            static_cast<void>(corrupt.view());
        });
        return;
    }
    std::abort();
}

void write_u32(std::vector<std::byte>& payload,
               std::size_t offset,
               std::uint32_t value) {
    expect(offset <= payload.size() && sizeof(value) <= payload.size() - offset);
    std::memcpy(payload.data() + offset, &value, sizeof(value));
}

void expect_validation_layer(std::vector<std::byte> payload,
                             std::uint32_t root_offset,
                             unit_rejection_layer layer) {
    auto corrupt = relocatable_world_demo::RegionValidationAccess::
        copied_buffer(payload, root_offset);
    try {
        validate_and_freeze_unit(corrupt);
    } catch (const unit_checkpoint_error& error) {
        expect(error.layer() == layer);
        expect(!corrupt.is_validated());
        return;
    }
    std::abort();
}

void test_structural_corruptions_rejected() {
    auto source = build_canonical_migrating_unit();
    const auto offsets = capture_unit_offsets(source);
    const std::vector<std::byte> canonical(
        source.used_bytes().begin(), source.used_bytes().end());

    expect_validation_layer(canonical, offsets.root_offset + 1,
                            unit_rejection_layer::region);

    {
        auto payload = canonical;
        write_u32(payload,
                  offsets.root_offset + offsetof(UnitSnapshot, effects),
                  static_cast<std::uint32_t>(
                      relocatable_world_demo::region_capacity + 1));
        expect_validation_layer(std::move(payload), offsets.root_offset,
                                unit_rejection_layer::region);
    }
    {
        auto payload = canonical;
        write_u32(payload,
                  offsets.root_offset + offsetof(UnitSnapshot, name),
                  offsets.effects_data);
        expect_validation_layer(std::move(payload), offsets.root_offset,
                                unit_rejection_layer::region);
    }
    {
        auto payload = canonical;
        write_u32(payload,
                  offsets.attributes_data - 1 + sizeof(AttributeEntry),
                  std::uint32_t{1});
        expect_validation_layer(std::move(payload), offsets.root_offset,
                                unit_rejection_layer::region);
    }
    {
        auto payload = canonical;
        write_u32(payload,
                  offsets.effects_data - 1 + offsetof(Effect, next),
                  offsets.effects_data + 1);
        expect_validation_layer(std::move(payload), offsets.root_offset,
                                unit_rejection_layer::graph);
    }
}

} // namespace

int main() {
    const std::string_view pointer_text{
        effect_pointer_signature.value,
        effect_pointer_signature.length(),
    };
    const std::string_view effect_text{
        effect_signature.value,
        effect_signature.length(),
    };
    expect(pointer_text.find("opaque") == std::string_view::npos);
    expect(effect_text.find("opaque") == std::string_view::npos);
    test_canonical_unit_region();
    test_registry_resolution_and_mutation();
    test_foreign_effect_handle_rejected();
    test_corrupt_effect_pointer_rejected_as_graph();
    test_structural_corruptions_rejected();
}
