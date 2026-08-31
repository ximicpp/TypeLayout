// unit_checkpoint.hpp -- Canonical byte envelope for unit handoff.
//
// Copyright (c) 2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#ifndef BOOST_TYPELAYOUT_RELOCATABLE_UNIT_HANDOFF_DEMO_CHECKPOINT_HPP
#define BOOST_TYPELAYOUT_RELOCATABLE_UNIT_HANDOFF_DEMO_CHECKPOINT_HPP

#include "unit_runtime.hpp"

#include "../relocatable_region_support/checkpoint_envelope.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace relocatable_unit_handoff_demo {

inline constexpr std::size_t unit_checkpoint_header_size =
    relocatable_region_support::checkpoint_envelope_header_size;

inline constexpr std::array<std::byte, 8> unit_checkpoint_magic = {
    std::byte{'T'}, std::byte{'L'}, std::byte{'U'}, std::byte{'N'},
    std::byte{'I'}, std::byte{'T'}, std::byte{0}, std::byte{0}};
inline constexpr std::array<std::byte, 4> unit_checkpoint_format = {
    std::byte{'6'}, std::byte{'4'}, std::byte{'L'}, std::byte{'E'}};
inline constexpr std::array<std::byte, 8> unit_checkpoint_schema = {
    std::byte{'U'}, std::byte{'N'}, std::byte{'I'}, std::byte{'T'},
    std::byte{'V'}, std::byte{'1'}, std::byte{0}, std::byte{0}};

inline constexpr relocatable_region_support::checkpoint_envelope_descriptor
    unit_checkpoint_envelope{
        unit_checkpoint_magic,
        unit_checkpoint_format,
        unit_checkpoint_schema,
        relocatable_region_support::envelope_checksum::fnv1a32_payload,
    };

using decoded_unit_checkpoint =
    relocatable_region_support::decoded_checkpoint_envelope;

std::vector<std::byte> encode_unit_checkpoint(
    std::span<const std::byte> payload,
    std::uint32_t root_offset);
decoded_unit_checkpoint decode_unit_checkpoint_envelope(
    std::span<const std::byte> artifact);
std::vector<std::byte> save_unit_checkpoint(const RegionBuffer& buffer);
RegionBuffer load_unit_checkpoint(std::span<const std::byte> artifact);

} // namespace relocatable_unit_handoff_demo

#endif // BOOST_TYPELAYOUT_RELOCATABLE_UNIT_HANDOFF_DEMO_CHECKPOINT_HPP
