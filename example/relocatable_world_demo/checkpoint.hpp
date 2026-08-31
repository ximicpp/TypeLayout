// checkpoint.hpp -- Canonical byte envelope for relocatable world checkpoints.
//
// Copyright (c) 2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#ifndef BOOST_TYPELAYOUT_RELOCATABLE_WORLD_DEMO_CHECKPOINT_HPP
#define BOOST_TYPELAYOUT_RELOCATABLE_WORLD_DEMO_CHECKPOINT_HPP

#include "../relocatable_region_support/checkpoint_envelope.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

namespace relocatable_world_demo {

class RegionBuffer;

enum class rejection_layer {
    envelope,
    region,
    graph
};

class checkpoint_error : public std::runtime_error {
public:
    checkpoint_error(rejection_layer layer, const char* message)
        : std::runtime_error(message), layer_(layer) {}

    rejection_layer layer() const noexcept { return layer_; }

private:
    rejection_layer layer_;
};

inline constexpr std::size_t checkpoint_header_size =
    relocatable_region_support::checkpoint_envelope_header_size;

inline constexpr std::array<std::byte, 8> checkpoint_magic = {
    std::byte{'T'}, std::byte{'L'}, std::byte{'W'}, std::byte{'O'},
    std::byte{'R'}, std::byte{'L'}, std::byte{'D'}, std::byte{0}};
inline constexpr std::array<std::byte, 4> checkpoint_format = {
    std::byte{'6'}, std::byte{'4'}, std::byte{'L'}, std::byte{'E'}};
inline constexpr std::array<std::byte, 8> checkpoint_schema = {
    std::byte{'W'}, std::byte{'O'}, std::byte{'R'}, std::byte{'L'},
    std::byte{'D'}, std::byte{'V'}, std::byte{'1'}, std::byte{0}};

inline constexpr relocatable_region_support::checkpoint_envelope_descriptor
    world_checkpoint_envelope{
        checkpoint_magic,
        checkpoint_format,
        checkpoint_schema,
        relocatable_region_support::envelope_checksum::reserved_zero,
    };

using decoded_checkpoint =
    relocatable_region_support::decoded_checkpoint_envelope;

std::vector<std::byte> encode_checkpoint(std::span<const std::byte> payload,
                                         std::uint32_t root_offset);
decoded_checkpoint decode_checkpoint_envelope(
    std::span<const std::byte> artifact);
std::vector<std::byte> save_checkpoint(const RegionBuffer& buffer);
RegionBuffer load_checkpoint(std::span<const std::byte> artifact);

} // namespace relocatable_world_demo

#endif // BOOST_TYPELAYOUT_RELOCATABLE_WORLD_DEMO_CHECKPOINT_HPP
