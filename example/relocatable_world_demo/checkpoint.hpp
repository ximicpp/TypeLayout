// checkpoint.hpp -- Canonical byte envelope for relocatable world checkpoints.
//
// Copyright (c) 2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#ifndef RELOCATABLE_WORLD_DEMO_CHECKPOINT_HPP
#define RELOCATABLE_WORLD_DEMO_CHECKPOINT_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

namespace relocatable_world_demo {

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

inline constexpr std::size_t checkpoint_header_size = 40;

inline constexpr std::array<std::byte, 8> checkpoint_magic = {
    std::byte{'T'}, std::byte{'L'}, std::byte{'W'}, std::byte{'O'},
    std::byte{'R'}, std::byte{'L'}, std::byte{'D'}, std::byte{0}};
inline constexpr std::array<std::byte, 4> checkpoint_format = {
    std::byte{'6'}, std::byte{'4'}, std::byte{'L'}, std::byte{'E'}};
inline constexpr std::array<std::byte, 8> checkpoint_schema = {
    std::byte{'W'}, std::byte{'O'}, std::byte{'R'}, std::byte{'L'},
    std::byte{'D'}, std::byte{'V'}, std::byte{'1'}, std::byte{0}};

struct decoded_checkpoint {
    std::span<const std::byte> payload;
    std::uint32_t root_offset;
};

std::vector<std::byte> encode_checkpoint(std::span<const std::byte> payload,
                                         std::uint32_t root_offset);
decoded_checkpoint decode_checkpoint_envelope(
    std::span<const std::byte> artifact);

} // namespace relocatable_world_demo

#endif // RELOCATABLE_WORLD_DEMO_CHECKPOINT_HPP
