// test_relocatable_checkpoint.cpp -- Checkpoint envelope tests.
//
// Copyright (c) 2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#include "checkpoint.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

using namespace relocatable_world_demo;

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::uint16_t read_u16_le(std::span<const std::byte> bytes,
                          std::size_t offset) {
    require(offset <= bytes.size() && 2 <= bytes.size() - offset,
            "u16 read is out of bounds");
    return static_cast<std::uint16_t>(
        std::to_integer<unsigned char>(bytes[offset])) |
        static_cast<std::uint16_t>(
            std::to_integer<unsigned char>(bytes[offset + 1])) << 8;
}

std::uint32_t read_u32_le(std::span<const std::byte> bytes,
                          std::size_t offset) {
    require(offset <= bytes.size() && 4 <= bytes.size() - offset,
            "u32 read is out of bounds");
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
    require(offset <= bytes.size() && 2 <= bytes.size() - offset,
            "u16 write is out of bounds");
    bytes[offset] = std::byte{static_cast<unsigned char>(value)};
    bytes[offset + 1] = std::byte{static_cast<unsigned char>(value >> 8)};
}

void write_u32_le(std::span<std::byte> bytes,
                  std::size_t offset,
                  std::uint32_t value) {
    require(offset <= bytes.size() && 4 <= bytes.size() - offset,
            "u32 write is out of bounds");
    bytes[offset] = std::byte{static_cast<unsigned char>(value)};
    bytes[offset + 1] = std::byte{static_cast<unsigned char>(value >> 8)};
    bytes[offset + 2] = std::byte{static_cast<unsigned char>(value >> 16)};
    bytes[offset + 3] = std::byte{static_cast<unsigned char>(value >> 24)};
}

template <typename Function>
void require_envelope_rejection(Function&& function, const char* message) {
    try {
        std::forward<Function>(function)();
    } catch (const checkpoint_error& error) {
        require(error.layer() == rejection_layer::envelope, message);
        return;
    }
    throw std::runtime_error(message);
}

std::vector<std::byte> valid_checkpoint() {
    std::array<std::byte, 64> payload{};
    return encode_checkpoint(payload, 0);
}

void test_exact_v1_envelope() {
    std::array<std::byte, 64> payload{};
    payload[7] = std::byte{0x5a};
    const auto bytes = encode_checkpoint(payload, 0);

    require(bytes.size() == checkpoint_header_size + payload.size(),
            "checkpoint length must include its 40-byte header");
    require(std::memcmp(bytes.data(), "TLWORLD\0", 8) == 0,
            "checkpoint magic must be TLWORLD");
    require(read_u16_le(bytes, 8) == 1, "checkpoint version must be one");
    require(read_u16_le(bytes, 10) == 40,
            "checkpoint header size must be 40");
    require(std::memcmp(bytes.data() + 12, "64LE", 4) == 0,
            "checkpoint format must be 64LE");
    require(read_u32_le(bytes, 16) == payload.size(),
            "checkpoint used payload size must be encoded");
    require(read_u32_le(bytes, 20) == 0,
            "checkpoint root offset must be encoded");
    require(read_u32_le(bytes, 24) == 0,
            "checkpoint flags must be zero");
    require(read_u32_le(bytes, 28) == 0,
            "checkpoint reserved field must be zero");
    require(std::memcmp(bytes.data() + 32, "WORLDV1\0", 8) == 0,
            "checkpoint schema must be WORLDV1");

    const auto decoded = decode_checkpoint_envelope(bytes);
    require(decoded.root_offset == 0,
            "decoded checkpoint must retain the root offset");
    require(decoded.payload.size() == payload.size(),
            "decoded checkpoint must return only the payload span");
    require(std::memcmp(decoded.payload.data(), payload.data(), payload.size()) == 0,
            "decoded payload must exactly match the encoded bytes");
}

void test_envelope_rejections() {
    {
        auto bytes = valid_checkpoint();
        bytes[0] = std::byte{'X'};
        require_envelope_rejection([&] { decode_checkpoint_envelope(bytes); },
                                   "bad magic must be an envelope rejection");
    }
    {
        auto bytes = valid_checkpoint();
        write_u16_le(bytes, 8, 2);
        require_envelope_rejection([&] { decode_checkpoint_envelope(bytes); },
                                   "bad version must be an envelope rejection");
    }
    {
        auto bytes = valid_checkpoint();
        write_u16_le(bytes, 10, 39);
        require_envelope_rejection([&] { decode_checkpoint_envelope(bytes); },
                                   "bad header size must be an envelope rejection");
    }
    {
        auto bytes = valid_checkpoint();
        bytes[12] = std::byte{'3'};
        require_envelope_rejection([&] { decode_checkpoint_envelope(bytes); },
                                   "bad format tag must be an envelope rejection");
    }
    {
        auto bytes = valid_checkpoint();
        write_u32_le(bytes, 24, 1);
        require_envelope_rejection([&] { decode_checkpoint_envelope(bytes); },
                                   "non-zero flags must be an envelope rejection");
    }
    {
        auto bytes = valid_checkpoint();
        write_u32_le(bytes, 28, 1);
        require_envelope_rejection([&] { decode_checkpoint_envelope(bytes); },
                                   "non-zero reserved field must be an envelope rejection");
    }
    {
        auto bytes = valid_checkpoint();
        bytes[32] = std::byte{'X'};
        require_envelope_rejection([&] { decode_checkpoint_envelope(bytes); },
                                   "bad schema tag must be an envelope rejection");
    }
    {
        auto bytes = valid_checkpoint();
        bytes.pop_back();
        require_envelope_rejection([&] { decode_checkpoint_envelope(bytes); },
                                   "truncated checkpoint must be an envelope rejection");
    }
    {
        auto bytes = valid_checkpoint();
        bytes.push_back(std::byte{0});
        require_envelope_rejection([&] { decode_checkpoint_envelope(bytes); },
                                   "trailing checkpoint byte must be an envelope rejection");
    }
    {
        auto bytes = valid_checkpoint();
        write_u32_le(bytes, 16, 4097);
        require_envelope_rejection([&] { decode_checkpoint_envelope(bytes); },
                                   "oversized payload must be an envelope rejection");
    }
    {
        auto bytes = valid_checkpoint();
        write_u32_le(bytes, 20, 64);
        require_envelope_rejection([&] { decode_checkpoint_envelope(bytes); },
                                   "root at used payload end must be an envelope rejection");
    }
}

void test_encoder_rejects_out_of_envelope_bounds() {
    std::array<std::byte, 4097> oversized_payload{};
    require_envelope_rejection([&] {
        static_cast<void>(encode_checkpoint(oversized_payload, 0));
    }, "encoder must reject payloads over the fixed capacity");

    std::array<std::byte, 64> payload{};
    require_envelope_rejection([&] {
        static_cast<void>(encode_checkpoint(payload, 64));
    }, "encoder must reject a root at the payload end");
}

} // namespace

int main() {
    test_exact_v1_envelope();
    test_envelope_rejections();
    test_encoder_rejects_out_of_envelope_bounds();
}
