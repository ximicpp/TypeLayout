// unit_runtime.cpp -- Unit construction, validation, and registry operations.
//
// Copyright (c) 2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#include "unit_runtime.hpp"

#include <limits>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

namespace relocatable_unit_handoff_demo {
namespace {

using relocatable_world_demo::RegionDescriptorAccess;
using relocatable_world_demo::RegionValidationAccess;
using relocatable_world_demo::region_capacity;

struct OwningInterval {
    std::uint32_t begin;
    std::uint32_t end;
};

[[noreturn]] void reject_region(const char* message) {
    throw unit_checkpoint_error(unit_rejection_layer::region, message);
}

[[noreturn]] void reject_graph(const char* message) {
    throw unit_checkpoint_error(unit_rejection_layer::graph, message);
}

class UnitRegionValidator {
public:
    static void validate(RegionBuffer& buffer) {
        UnitRegionValidator validator(buffer);
        validator.run();
    }

private:
    explicit UnitRegionValidator(RegionBuffer& buffer)
        : buffer_(buffer) {}

    void run() {
        RegionValidationAccess::require_awaiting_validation(buffer_);
        if (used_bytes() > region_capacity) {
            reject_region("used payload exceeds region capacity");
        }

        validate_root();
        validate_root_ranges();
        resolve_effects();
        validate_strings();
        resolve_attributes_and_order();
        validate_attributes();
        validate_graph();
        RegionValidationAccess::mark_validated(buffer_);
    }

    void validate_root() {
        const auto begin = static_cast<std::size_t>(
            RegionValidationAccess::root_offset(buffer_));
        if (begin % alignof(UnitSnapshot) != 0) {
            reject_region("unit root is misaligned");
        }
        if (begin > std::numeric_limits<std::size_t>::max() -
                        sizeof(UnitSnapshot)) {
            reject_region("unit root extent overflows");
        }
        const auto end = begin + sizeof(UnitSnapshot);
        if (end > used_bytes() || end > region_capacity) {
            reject_region("unit root is outside the used payload");
        }
        reserve_interval(begin, end);
        root_ = std::launder(reinterpret_cast<UnitSnapshot*>(base() + begin));
    }

    void validate_root_ranges() {
        effects_interval_ = validate_range(
            RegionDescriptorAccess::data_offset(root_->effects),
            root_->effects.size(), sizeof(Effect), alignof(Effect));
        attributes_interval_ = validate_range(
            RegionDescriptorAccess::data_offset(root_->attributes),
            RegionDescriptorAccess::size(root_->attributes),
            sizeof(AttributeEntry), alignof(AttributeEntry));
        order_interval_ = validate_range(
            RegionDescriptorAccess::data_offset(root_->effect_order),
            root_->effect_order.size(), sizeof(EffectRelativePtr),
            alignof(EffectRelativePtr));
    }

    OwningInterval validate_range(std::uint32_t offset_plus_one,
                                  std::uint32_t count,
                                  std::size_t element_size,
                                  std::size_t alignment) {
        if ((offset_plus_one == 0) != (count == 0)) {
            reject_region("region descriptor null/count invariant failed");
        }
        if (count == 0) {
            return {0, 0};
        }

        const auto begin = static_cast<std::size_t>(offset_plus_one - 1);
        if (begin % alignment != 0) {
            reject_region("unit region range is misaligned");
        }
        const auto count_size = static_cast<std::size_t>(count);
        if (count_size >
            std::numeric_limits<std::size_t>::max() / element_size) {
            reject_region("unit region range extent overflows");
        }
        const auto extent = count_size * element_size;
        if (begin > std::numeric_limits<std::size_t>::max() - extent) {
            reject_region("unit region range end overflows");
        }
        const auto end = begin + extent;
        if (end > used_bytes() || end > region_capacity) {
            reject_region("unit region range is outside the used payload");
        }
        reserve_interval(begin, end);
        return {checked_offset(begin), checked_offset(end)};
    }

    void reserve_interval(std::size_t begin, std::size_t end) {
        const OwningInterval candidate{checked_offset(begin),
                                       checked_offset(end)};
        for (const auto interval : intervals_) {
            if (candidate.begin < interval.end &&
                interval.begin < candidate.end) {
                reject_region("owning unit region ranges overlap");
            }
        }
        intervals_.push_back(candidate);
    }

    void resolve_effects() {
        effects_ = resolve_array<Effect>(
            effects_interval_, root_->effects.size());
    }

    void validate_strings() {
        static_cast<void>(validate_range(
            RegionDescriptorAccess::data_offset(root_->name),
            root_->name.size(), sizeof(char), alignof(char)));
        for (std::uint32_t index = 0; index < root_->effects.size(); ++index) {
            static_cast<void>(validate_range(
                RegionDescriptorAccess::data_offset(effects_[index].label),
                effects_[index].label.size(), sizeof(char), alignof(char)));
        }
    }

    void resolve_attributes_and_order() {
        attributes_ = resolve_array<AttributeEntry>(
            attributes_interval_,
            RegionDescriptorAccess::size(root_->attributes));
        order_ = resolve_array<EffectRelativePtr>(
            order_interval_, root_->effect_order.size());
    }

    void validate_attributes() const {
        const auto count = RegionDescriptorAccess::size(root_->attributes);
        for (std::uint32_t index = 1; index < count; ++index) {
            if (!(attributes_[index - 1].key < attributes_[index].key)) {
                reject_region("unit attribute keys are not strictly increasing");
            }
        }
    }

    void validate_graph() const {
        validate_effect_pointer(root_->selected_effect);
        for (std::uint32_t index = 0; index < root_->effect_order.size();
             ++index) {
            validate_effect_pointer(order_[index]);
        }
        for (std::uint32_t index = 0; index < root_->effects.size(); ++index) {
            validate_effect_pointer(effects_[index].next);
        }
    }

    void validate_effect_pointer(const EffectRelativePtr& pointer) const {
        const auto raw = pointer.raw_offset_plus_one();
        if (raw == 0) {
            return;
        }
        if (root_->effects.size() == 0) {
            reject_graph("effect pointer has no target allocation");
        }
        const auto offset = static_cast<std::uint32_t>(raw - 1);
        if (offset < effects_interval_.begin ||
            offset >= effects_interval_.end ||
            (offset - effects_interval_.begin) % sizeof(Effect) != 0) {
            reject_graph("effect pointer does not name an effect element");
        }
    }

    template <typename T>
    T* resolve_array(OwningInterval interval, std::uint32_t count) const {
        if (count == 0) {
            return nullptr;
        }
        return std::launder(reinterpret_cast<T*>(base() + interval.begin));
    }

    std::byte* base() const {
        return RegionValidationAccess::base(buffer_);
    }

    std::size_t used_bytes() const {
        return RegionValidationAccess::used_bytes(buffer_);
    }

    static std::uint32_t checked_offset(std::size_t value) {
        if (value > std::numeric_limits<std::uint32_t>::max()) {
            reject_region("unit region offset is not representable");
        }
        return static_cast<std::uint32_t>(value);
    }

    RegionBuffer& buffer_;
    UnitSnapshot* root_ = nullptr;
    Effect* effects_ = nullptr;
    AttributeEntry* attributes_ = nullptr;
    EffectRelativePtr* order_ = nullptr;
    OwningInterval effects_interval_{};
    OwningInterval attributes_interval_{};
    OwningInterval order_interval_{};
    std::vector<OwningInterval> intervals_;
};

void populate_common_scalars(RegionBuilder& builder,
                             relocatable_world_demo::region_handle<UnitSnapshot>
                                 root,
                             UnitId id,
                             UnitId owner,
                             UnitId target,
                             UnitPosition position,
                             std::int32_t hp,
                             std::string_view name) {
    builder.set(root, &UnitSnapshot::id, id);
    builder.set(root, &UnitSnapshot::owner_id, owner);
    builder.set(root, &UnitSnapshot::target_id, target);
    builder.set(root, &UnitSnapshot::position, position);
    builder.set(root, &UnitSnapshot::hp, hp);
    builder.assign(root, &UnitSnapshot::name, name);
}

void bind_empty_collections(RegionBuilder& builder,
                            relocatable_world_demo::region_handle<UnitSnapshot>
                                root) {
    builder.bind(root, &UnitSnapshot::effects,
                 builder.make_array<Effect>(0));
    builder.bind(root, &UnitSnapshot::attributes,
                 builder.make_array<AttributeEntry>(0));
    builder.bind(root, &UnitSnapshot::effect_order,
                 builder.make_array<EffectRelativePtr>(0));
}

} // namespace

relocatable_world_demo::region_handle<UnitSnapshot>
populate_canonical_migrating_unit(RegionBuilder& builder) {
    const auto root = builder.make_object<UnitSnapshot>();
    populate_common_scalars(builder, root, migrating_unit_id, owner_unit_id,
                            unresolved_target_id, UnitPosition{17, 29}, 300,
                            "Ranger");

    const auto effects = builder.make_array<Effect>(2);
    builder.set(effects, 0, &Effect::id, std::uint32_t{1});
    builder.set(effects, 0, &Effect::kind, EffectKind::shield);
    builder.set(effects, 0, &Effect::magnitude, std::int32_t{25});
    builder.assign(effects, 0, &Effect::label, "Shield");
    builder.set(effects, 1, &Effect::id, std::uint32_t{2});
    builder.set(effects, 1, &Effect::kind, EffectKind::haste);
    builder.set(effects, 1, &Effect::magnitude, std::int32_t{10});
    builder.assign(effects, 1, &Effect::label, "Haste");
    const auto effect_a = builder.element_handle(effects, 0);
    const auto effect_b = builder.element_handle(effects, 1);
    builder.bind(effects, 0, &Effect::next, effect_b);
    builder.bind(effects, 1, &Effect::next, effect_a);
    builder.bind(root, &UnitSnapshot::effects, effects);

    const auto attributes = builder.make_array<AttributeEntry>(2);
    builder.set(attributes, 0, AttributeEntry{1, 42});
    builder.set(attributes, 1, AttributeEntry{2, 18});
    builder.bind(root, &UnitSnapshot::attributes, attributes);

    const auto order = builder.make_array<EffectRelativePtr>(4);
    builder.bind(order, 0, effect_a);
    builder.bind(order, 1, effect_b);
    builder.bind(order, 2, effect_a);
    builder.bind(order, 3,
                 relocatable_world_demo::region_handle<Effect>{});
    builder.bind(root, &UnitSnapshot::effect_order, order);
    builder.bind(root, &UnitSnapshot::selected_effect, effect_a);
    return root;
}

relocatable_world_demo::region_handle<UnitSnapshot>
populate_canonical_owner_unit(RegionBuilder& builder) {
    const auto root = builder.make_object<UnitSnapshot>();
    populate_common_scalars(builder, root, owner_unit_id, 0, 0,
                            UnitPosition{5, 7}, 500, "SquadLeader");
    bind_empty_collections(builder, root);
    return root;
}

void validate_and_freeze_unit(RegionBuffer& buffer) {
    UnitRegionValidator::validate(buffer);
}

RegionBuffer build_canonical_migrating_unit() {
    RegionBuilder builder;
    const auto root = populate_canonical_migrating_unit(builder);
    auto buffer = std::move(builder).finish(root);
    validate_and_freeze_unit(buffer);
    return buffer;
}

RegionBuffer build_canonical_owner_unit() {
    RegionBuilder builder;
    const auto root = populate_canonical_owner_unit(builder);
    auto buffer = std::move(builder).finish(root);
    validate_and_freeze_unit(buffer);
    return buffer;
}

const UnitSnapshot& unit_root(const RegionBuffer& buffer) {
    if (!buffer.is_validated()) {
        throw std::logic_error("unit region has not been validated");
    }
    return *std::launder(reinterpret_cast<const UnitSnapshot*>(
        RegionValidationAccess::base(buffer) +
        RegionValidationAccess::root_offset(buffer)));
}

UnitOffsets capture_unit_offsets(const RegionBuffer& buffer) {
    const auto& root = unit_root(buffer);
    const auto view = buffer.view();
    const auto effects = view.elements(root.effects);
    const auto order = view.elements(root.effect_order);
    if (effects.size() != 2 || order.size() != 4) {
        throw std::logic_error("canonical unit offset shape differs");
    }

    UnitOffsets result{};
    result.root_offset = RegionValidationAccess::root_offset(buffer);
    result.name_data = RegionDescriptorAccess::data_offset(root.name);
    result.effects_data = RegionDescriptorAccess::data_offset(root.effects);
    result.attributes_data =
        RegionDescriptorAccess::data_offset(root.attributes);
    result.effect_order_data =
        RegionDescriptorAccess::data_offset(root.effect_order);
    result.selected_effect = root.selected_effect.raw_offset_plus_one();
    for (std::size_t index = 0; index < effects.size(); ++index) {
        result.effect_labels[index] =
            RegionDescriptorAccess::data_offset(effects[index].label);
        result.effect_next[index] = effects[index].next.raw_offset_plus_one();
    }
    for (std::size_t index = 0; index < order.size(); ++index) {
        result.effect_order[index] = order[index].raw_offset_plus_one();
    }
    return result;
}

bool canonical_migrating_unit_matches(const RegionBuffer& buffer,
                                      std::int32_t expected_hp) {
    const auto& root = unit_root(buffer);
    const auto view = buffer.view();
    if (root.id != migrating_unit_id || root.owner_id != owner_unit_id ||
        root.target_id != unresolved_target_id || root.hp != expected_hp ||
        root.position.x != 17 || root.position.y != 29 ||
        view.text(root.name) != "Ranger") {
        return false;
    }

    const auto effects = view.elements(root.effects);
    const auto order = view.elements(root.effect_order);
    const auto attributes = view.map(root.attributes);
    if (effects.size() != 2 || order.size() != 4 || attributes.size() != 2 ||
        effects[0].id != 1 || effects[0].kind != EffectKind::shield ||
        effects[0].magnitude != 25 || view.text(effects[0].label) != "Shield" ||
        effects[1].id != 2 || effects[1].kind != EffectKind::haste ||
        effects[1].magnitude != 10 || view.text(effects[1].label) != "Haste") {
        return false;
    }
    const auto attack = attributes.find(1);
    const auto armor = attributes.find(2);
    if (attack == attributes.end() || attack->value != 42 ||
        armor == attributes.end() || armor->value != 18) {
        return false;
    }

    const auto* effect_a = std::addressof(effects[0]);
    const auto* effect_b = std::addressof(effects[1]);
    return view.resolve(root.selected_effect) == effect_a &&
        view.resolve(effects[0].next) == effect_b &&
        view.resolve(effects[1].next) == effect_a &&
        view.resolve(order[0]) == effect_a &&
        view.resolve(order[1]) == effect_b &&
        view.resolve(order[2]) == effect_a && order[3].is_null() &&
        view.resolve(order[3]) == nullptr;
}

void set_unit_hp(RegionBuffer& buffer, std::int32_t hp) {
    if (!buffer.is_validated()) {
        throw std::logic_error("unit region has not been validated");
    }
    auto* root = std::launder(reinterpret_cast<UnitSnapshot*>(
        RegionValidationAccess::base(buffer) +
        RegionValidationAccess::root_offset(buffer)));
    root->hp = hp;
}

void UnitRegistry::attach(UnitId expected_id, RegionBuffer buffer) {
    if (!buffer.is_validated()) {
        throw std::invalid_argument("registry requires a validated unit buffer");
    }
    if (unit_root(buffer).id != expected_id) {
        throw std::invalid_argument("unit root ID differs from registry key");
    }
    if (!buffers_.emplace(expected_id, std::move(buffer)).second) {
        throw std::invalid_argument("unit registry ID is already attached");
    }
}

const UnitSnapshot* UnitRegistry::resolve(UnitId id) const {
    const auto found = buffers_.find(id);
    return found == buffers_.end() ? nullptr : std::addressof(unit_root(found->second));
}

void UnitRegistry::set_hp(UnitId id, std::int32_t hp) {
    const auto found = buffers_.find(id);
    if (found == buffers_.end()) {
        throw std::out_of_range("unit ID is not attached");
    }
    set_unit_hp(found->second, hp);
}

} // namespace relocatable_unit_handoff_demo
