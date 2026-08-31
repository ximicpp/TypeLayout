// checkpoint_envelope.hpp -- Shared byte envelope for relocatable examples.
//
// Copyright (c) 2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#ifndef BOOST_TYPELAYOUT_RELOCATABLE_REGION_SUPPORT_CHECKPOINT_ENVELOPE_HPP
#define BOOST_TYPELAYOUT_RELOCATABLE_REGION_SUPPORT_CHECKPOINT_ENVELOPE_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

namespace relocatable_region_support {

inline constexpr std::size_t checkpoint_envelope_header_size = 40;

enum class envelope_checksum {
    reserved_zero,
    fnv1a32_payload
};

struct checkpoint_envelope_descriptor {
    std::array<std::byte, 8> magic;
    std::array<std::byte, 4> format;
    std::array<std::byte, 8> schema;
    envelope_checksum checksum = envelope_checksum::reserved_zero;
};

struct decoded_checkpoint_envelope {
    std::span<const std::byte> payload;
    std::uint32_t root_offset;
};

class checkpoint_envelope_error : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

std::vector<std::byte> encode_checkpoint_envelope(
    std::span<const std::byte> payload,
    std::uint32_t root_offset,
    const checkpoint_envelope_descriptor& descriptor,
    std::size_t payload_capacity);

decoded_checkpoint_envelope decode_checkpoint_envelope(
    std::span<const std::byte> artifact,
    const checkpoint_envelope_descriptor& descriptor,
    std::size_t payload_capacity);

} // namespace relocatable_region_support

#endif // BOOST_TYPELAYOUT_RELOCATABLE_REGION_SUPPORT_CHECKPOINT_ENVELOPE_HPP
