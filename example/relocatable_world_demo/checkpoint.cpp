// checkpoint.cpp -- Canonical byte envelope for relocatable world checkpoints.
//
// Copyright (c) 2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#include "checkpoint.hpp"

#include "region_storage.hpp"
#include "world_runtime.hpp"

#include <utility>

namespace relocatable_world_demo {
namespace {

template <typename Function>
decltype(auto) translate_envelope_error(Function&& function) {
    try {
        return std::forward<Function>(function)();
    } catch (const relocatable_region_support::checkpoint_envelope_error&
                 error) {
        throw checkpoint_error(rejection_layer::envelope, error.what());
    }
}

} // namespace

std::vector<std::byte> encode_checkpoint(std::span<const std::byte> payload,
                                         std::uint32_t root_offset) {
    return translate_envelope_error([&] {
        return relocatable_region_support::encode_checkpoint_envelope(
            payload, root_offset, world_checkpoint_envelope, region_capacity);
    });
}

decoded_checkpoint decode_checkpoint_envelope(
    std::span<const std::byte> artifact) {
    return translate_envelope_error([&] {
        return relocatable_region_support::decode_checkpoint_envelope(
            artifact, world_checkpoint_envelope, region_capacity);
    });
}

std::vector<std::byte> save_checkpoint(const RegionBuffer& buffer) {
    if (!buffer.is_validated()) {
        throw std::logic_error("cannot save an unvalidated region buffer");
    }
    return encode_checkpoint(
        buffer.used_bytes(), RegionValidationAccess::root_offset(buffer));
}

RegionBuffer load_checkpoint(std::span<const std::byte> artifact) {
    const auto decoded = decode_checkpoint_envelope(artifact);
    auto buffer = RegionValidationAccess::copied_buffer(
        decoded.payload, decoded.root_offset);
    validate_and_freeze_world(buffer);
    return buffer;
}

} // namespace relocatable_world_demo
