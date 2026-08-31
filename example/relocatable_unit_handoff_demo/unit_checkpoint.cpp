// unit_checkpoint.cpp -- Canonical byte envelope for unit handoff.
//
// Copyright (c) 2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#include "unit_checkpoint.hpp"

#include <utility>

namespace relocatable_unit_handoff_demo {
namespace {

template <typename Function>
decltype(auto) translate_envelope_error(Function&& function) {
    try {
        return std::forward<Function>(function)();
    } catch (const relocatable_region_support::checkpoint_envelope_error&
                 error) {
        throw unit_checkpoint_error(unit_rejection_layer::envelope,
                                    error.what());
    }
}

} // namespace

std::vector<std::byte> encode_unit_checkpoint(
    std::span<const std::byte> payload,
    std::uint32_t root_offset) {
    return translate_envelope_error([&] {
        return relocatable_region_support::encode_checkpoint_envelope(
            payload, root_offset, unit_checkpoint_envelope,
            relocatable_world_demo::region_capacity);
    });
}

decoded_unit_checkpoint decode_unit_checkpoint_envelope(
    std::span<const std::byte> artifact) {
    return translate_envelope_error([&] {
        return relocatable_region_support::decode_checkpoint_envelope(
            artifact, unit_checkpoint_envelope,
            relocatable_world_demo::region_capacity);
    });
}

std::vector<std::byte> save_unit_checkpoint(const RegionBuffer& buffer) {
    if (!buffer.is_validated()) {
        throw std::logic_error("cannot save an unvalidated unit buffer");
    }
    return encode_unit_checkpoint(
        buffer.used_bytes(),
        relocatable_world_demo::RegionValidationAccess::root_offset(buffer));
}

RegionBuffer load_unit_checkpoint(std::span<const std::byte> artifact) {
    const auto decoded = decode_unit_checkpoint_envelope(artifact);
    auto buffer = relocatable_world_demo::RegionValidationAccess::copied_buffer(
        decoded.payload, decoded.root_offset);
    validate_and_freeze_unit(buffer);
    return buffer;
}

} // namespace relocatable_unit_handoff_demo
