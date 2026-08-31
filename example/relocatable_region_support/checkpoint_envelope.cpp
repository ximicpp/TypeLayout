// checkpoint_envelope.cpp -- Shared byte envelope for relocatable examples.
//
// Copyright (c) 2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#include "checkpoint_envelope.hpp"

#include <algorithm>
#include <limits>

namespace relocatable_region_support {
namespace {

constexpr std::size_t version_offset = 8;
constexpr std::size_t header_size_offset = 10;
constexpr std::size_t format_offset = 12;
constexpr std::size_t payload_size_offset = 16;
constexpr std::size_t root_offset_offset = 20;
constexpr std::size_t flags_offset = 24;
constexpr std::size_t checksum_offset = 28;
constexpr std::size_t schema_offset = 32;

[[noreturn]] void reject(const char* message) {
    throw checkpoint_envelope_error(message);
}

std::size_t checked_size(std::uint16_t value) {
    if (static_cast<std::uintmax_t>(value) >
        static_cast<std::uintmax_t>(std::numeric_limits<std::size_t>::max())) {
        reject("checkpoint integer is not representable");
    }
    return static_cast<std::size_t>(value);
}

std::size_t checked_size(std::uint32_t value) {
    if (static_cast<std::uintmax_t>(value) >
        static_cast<std::uintmax_t>(std::numeric_limits<std::size_t>::max())) {
        reject("checkpoint integer is not representable");
    }
    return static_cast<std::size_t>(value);
}

std::uint16_t read_u16_le(std::span<const std::byte> bytes,
                          std::size_t offset) {
    return static_cast<std::uint16_t>(
        std::to_integer<unsigned char>(bytes[offset])) |
        static_cast<std::uint16_t>(
            std::to_integer<unsigned char>(bytes[offset + 1])) << 8;
}

std::uint32_t read_u32_le(std::span<const std::byte> bytes,
                          std::size_t offset) {
    return static_cast<std::uint32_t>(
        std::to_integer<unsigned char>(bytes[offset])) |
        static_cast<std::uint32_t>(
            std::to_integer<unsigned char>(bytes[offset + 1])) << 8 |
        static_cast<std::uint32_t>(
            std::to_integer<unsigned char>(bytes[offset + 2])) << 16 |
        static_cast<std::uint32_t>(
            std::to_integer<unsigned char>(bytes[offset + 3])) << 24;
}

void write_u16_le(std::span<std::byte> bytes,
                  std::size_t offset,
                  std::uint16_t value) {
    bytes[offset] = std::byte{static_cast<unsigned char>(value)};
    bytes[offset + 1] = std::byte{static_cast<unsigned char>(value >> 8)};
}

void write_u32_le(std::span<std::byte> bytes,
                  std::size_t offset,
                  std::uint32_t value) {
    bytes[offset] = std::byte{static_cast<unsigned char>(value)};
    bytes[offset + 1] = std::byte{static_cast<unsigned char>(value >> 8)};
    bytes[offset + 2] = std::byte{static_cast<unsigned char>(value >> 16)};
    bytes[offset + 3] = std::byte{static_cast<unsigned char>(value >> 24)};
}

bool has_bytes(std::span<const std::byte> bytes,
               std::size_t offset,
               std::size_t count) {
    return offset <= bytes.size() && count <= bytes.size() - offset;
}

template <std::size_t N>
bool has_field(std::span<const std::byte> artifact,
               std::size_t offset,
               const std::array<std::byte, N>& expected) {
    return has_bytes(artifact, offset, expected.size()) &&
        std::equal(expected.begin(), expected.end(), artifact.begin() + offset);
}

std::uint32_t fnv1a32(std::span<const std::byte> bytes) {
    std::uint32_t hash = 2166136261u;
    for (const auto value : bytes) {
        hash ^= std::to_integer<unsigned char>(value);
        hash *= 16777619u;
    }
    return hash;
}

std::uint32_t encoded_checksum(std::span<const std::byte> payload,
                               envelope_checksum checksum) {
    switch (checksum) {
    case envelope_checksum::reserved_zero:
        return 0;
    case envelope_checksum::fnv1a32_payload:
        return fnv1a32(payload);
    }
    reject("checkpoint checksum policy is invalid");
}

} // namespace

std::vector<std::byte> encode_checkpoint_envelope(
    std::span<const std::byte> payload,
    std::uint32_t root_offset,
    const checkpoint_envelope_descriptor& descriptor,
    std::size_t payload_capacity) {
    if (payload.size() > payload_capacity) {
        reject("checkpoint payload exceeds region capacity");
    }
    if (payload.size() > std::numeric_limits<std::uint32_t>::max()) {
        reject("checkpoint payload size is not representable");
    }

    const auto encoded_payload_size = static_cast<std::uint32_t>(payload.size());
    const auto root = checked_size(root_offset);
    if (root >= payload.size()) {
        reject("checkpoint root is outside the used payload");
    }
    if (payload.size() > std::numeric_limits<std::size_t>::max() -
                             checkpoint_envelope_header_size) {
        reject("checkpoint artifact size overflows");
    }

    std::vector<std::byte> artifact(
        checkpoint_envelope_header_size + payload.size());
    std::copy(descriptor.magic.begin(), descriptor.magic.end(), artifact.begin());
    write_u16_le(artifact, version_offset, 1);
    write_u16_le(artifact, header_size_offset,
                 static_cast<std::uint16_t>(checkpoint_envelope_header_size));
    std::copy(descriptor.format.begin(), descriptor.format.end(),
              artifact.begin() + format_offset);
    write_u32_le(artifact, payload_size_offset, encoded_payload_size);
    write_u32_le(artifact, root_offset_offset, root_offset);
    write_u32_le(artifact, flags_offset, 0);
    write_u32_le(artifact, checksum_offset,
                 encoded_checksum(payload, descriptor.checksum));
    std::copy(descriptor.schema.begin(), descriptor.schema.end(),
              artifact.begin() + schema_offset);
    std::copy(payload.begin(), payload.end(),
              artifact.begin() + checkpoint_envelope_header_size);
    return artifact;
}

decoded_checkpoint_envelope decode_checkpoint_envelope(
    std::span<const std::byte> artifact,
    const checkpoint_envelope_descriptor& descriptor,
    std::size_t payload_capacity) {
    if (artifact.size() < checkpoint_envelope_header_size) {
        reject("checkpoint is shorter than its header");
    }
    if (!has_field(artifact, 0, descriptor.magic)) {
        reject("checkpoint magic does not match");
    }
    if (checked_size(read_u16_le(artifact, version_offset)) != 1) {
        reject("checkpoint version does not match");
    }
    if (checked_size(read_u16_le(artifact, header_size_offset)) !=
        checkpoint_envelope_header_size) {
        reject("checkpoint header size does not match");
    }
    if (!has_field(artifact, format_offset, descriptor.format)) {
        reject("checkpoint format does not match");
    }

    const auto payload_size = checked_size(
        read_u32_le(artifact, payload_size_offset));
    const auto root = checked_size(read_u32_le(artifact, root_offset_offset));
    if (checked_size(read_u32_le(artifact, flags_offset)) != 0) {
        reject("checkpoint flags are not zero");
    }
    const auto checksum = read_u32_le(artifact, checksum_offset);
    if (!has_field(artifact, schema_offset, descriptor.schema)) {
        reject("checkpoint schema does not match");
    }
    if (payload_size > payload_capacity) {
        reject("checkpoint payload exceeds region capacity");
    }
    if (payload_size > std::numeric_limits<std::size_t>::max() -
                           checkpoint_envelope_header_size) {
        reject("checkpoint artifact size overflows");
    }
    if (artifact.size() != checkpoint_envelope_header_size + payload_size) {
        reject("checkpoint artifact length does not match payload");
    }
    if (root >= payload_size) {
        reject("checkpoint root is outside the used payload");
    }

    const auto payload = artifact.subspan(
        checkpoint_envelope_header_size, payload_size);
    if (checksum != encoded_checksum(payload, descriptor.checksum)) {
        reject(descriptor.checksum == envelope_checksum::reserved_zero
                   ? "checkpoint reserved field is not zero"
                   : "checkpoint checksum does not match");
    }

    return {payload, static_cast<std::uint32_t>(root)};
}

} // namespace relocatable_region_support
