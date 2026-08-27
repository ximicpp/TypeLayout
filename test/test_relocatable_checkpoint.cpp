// test_relocatable_checkpoint.cpp -- Checkpoint envelope tests.
//
// Copyright (c) 2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#include "checkpoint.hpp"
#include "world.hpp"
#include "world_runtime.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <limits>
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

void write_u64_le(std::span<std::byte> bytes,
                  std::size_t offset,
                  std::uint64_t value) {
    require(offset <= bytes.size() && 8 <= bytes.size() - offset,
            "u64 write is out of bounds");
    for (std::size_t index = 0; index != 8; ++index) {
        bytes[offset + index] = std::byte{
            static_cast<unsigned char>(value >> (index * 8))};
    }
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

template <typename Exception, typename Function>
void require_throws(Function&& function, const char* message) {
    try {
        std::forward<Function>(function)();
    } catch (const Exception&) {
        return;
    }
    throw std::runtime_error(message);
}

std::vector<std::byte> valid_checkpoint() {
    std::array<std::byte, 64> payload{};
    return encode_checkpoint(payload, 0);
}

std::size_t payload_size(std::span<const std::byte> artifact) {
    return read_u32_le(artifact, 16);
}

std::size_t root_payload_offset(std::span<const std::byte> artifact) {
    return read_u32_le(artifact, 20);
}

std::size_t payload_field(std::size_t payload_offset) {
    return checkpoint_header_size + payload_offset;
}

std::size_t decode_non_null(std::uint32_t offset_plus_one) {
    require(offset_plus_one != 0, "fixture offset must be non-null");
    return static_cast<std::size_t>(offset_plus_one - 1);
}

std::size_t root_field(std::span<const std::byte> artifact,
                       std::size_t member_offset) {
    return payload_field(root_payload_offset(artifact) + member_offset);
}

std::size_t descriptor_target(std::span<const std::byte> artifact,
                              std::size_t descriptor_field) {
    return decode_non_null(read_u32_le(artifact, descriptor_field));
}

std::size_t entities_descriptor(std::span<const std::byte> artifact) {
    return root_field(artifact, offsetof(WorldSnapshot, entities));
}

std::size_t index_descriptor(std::span<const std::byte> artifact) {
    return root_field(artifact, offsetof(WorldSnapshot, entity_index));
}

std::size_t party_descriptor(std::span<const std::byte> artifact) {
    return root_field(artifact, offsetof(WorldSnapshot, party));
}

std::size_t entity_payload_offset(std::span<const std::byte> artifact,
                                  std::size_t index) {
    return descriptor_target(artifact, entities_descriptor(artifact)) +
        index * sizeof(Entity);
}

std::size_t index_payload_offset(std::span<const std::byte> artifact,
                                 std::size_t index) {
    return descriptor_target(artifact, index_descriptor(artifact)) +
        index * sizeof(EntityIndexEntry);
}

template <typename Mutator>
void require_mutated_rejection(rejection_layer expected,
                               Mutator&& mutate,
                               const char* message) {
    auto artifact = save_checkpoint(build_canonical_world());
    std::forward<Mutator>(mutate)(artifact);
    try {
        static_cast<void>(load_checkpoint(artifact));
    } catch (const checkpoint_error& error) {
        require(error.layer() == expected, message);
        return;
    }
    throw std::runtime_error(message);
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

void test_world_checkpoint_orchestration() {
    RegionBuilder builder;
    const auto unvalidated_root = builder.make_object<WorldSnapshot>();
    auto unvalidated = std::move(builder).finish(unvalidated_root);
    require_throws<std::logic_error>([&] {
        static_cast<void>(save_checkpoint(unvalidated));
    }, "saving an unvalidated world must fail");

    auto source = build_canonical_world();
    const auto artifact = save_checkpoint(source);
    auto loaded = load_checkpoint(artifact);
    require(loaded.is_validated(),
            "loaded canonical checkpoint must be validated");

    const auto& world = world_root(loaded);
    const auto view = loaded.view();
    const auto entities = view.elements(world.entities);
    require(world.tick == 42 && entities.size() == 2,
            "loaded checkpoint must expose its typed root and entity range");
    require(view.text(entities[0].name) == "Hero" &&
                view.text(entities[1].name) == "Boss",
            "loaded checkpoint must expose its copied character payloads");
}

void test_root_region_rejections() {
    require_mutated_rejection(rejection_layer::region, [](auto& artifact) {
        write_u32_le(artifact, 20, 1);
    }, "misaligned root must be a region rejection");

    require_mutated_rejection(rejection_layer::region, [](auto& artifact) {
        write_u32_le(artifact, 20,
                     static_cast<std::uint32_t>(payload_size(artifact) - 1));
    }, "incomplete root extent must be a region rejection");
}

void test_entity_range_rejections() {
    require_mutated_rejection(rejection_layer::region, [](auto& artifact) {
        write_u32_le(artifact, entities_descriptor(artifact), 0);
    }, "null entities with a nonzero count must be a region rejection");

    require_mutated_rejection(rejection_layer::region, [](auto& artifact) {
        write_u32_le(artifact, entities_descriptor(artifact) + 4, 0);
    }, "non-null entities with a zero count must be a region rejection");

    require_mutated_rejection(rejection_layer::region, [](auto& artifact) {
        write_u32_le(artifact, entities_descriptor(artifact), 2);
    }, "misaligned entities must be a region rejection");

    require_mutated_rejection(rejection_layer::region, [](auto& artifact) {
        write_u32_le(artifact, entities_descriptor(artifact),
                     static_cast<std::uint32_t>(payload_size(artifact) + 1));
    }, "out-of-bounds entities must be a region rejection");

    require_mutated_rejection(rejection_layer::region, [](auto& artifact) {
        write_u32_le(artifact, entities_descriptor(artifact) + 4,
                     std::numeric_limits<std::uint32_t>::max());
    }, "oversized entity extent must be a region rejection");
}

void test_index_range_rejections() {
    require_mutated_rejection(rejection_layer::region, [](auto& artifact) {
        write_u32_le(artifact, index_descriptor(artifact), 0);
    }, "null index with a nonzero count must be a region rejection");

    require_mutated_rejection(rejection_layer::region, [](auto& artifact) {
        write_u32_le(artifact, index_descriptor(artifact) + 4, 0);
    }, "non-null index with a zero count must be a region rejection");

    require_mutated_rejection(rejection_layer::region, [](auto& artifact) {
        write_u32_le(artifact, index_descriptor(artifact), 2);
    }, "misaligned index entries must be a region rejection");

    require_mutated_rejection(rejection_layer::region, [](auto& artifact) {
        write_u32_le(artifact, index_descriptor(artifact) + 4,
                     std::numeric_limits<std::uint32_t>::max());
    }, "oversized index-entry extent must be a region rejection");
}

void test_party_range_rejections() {
    require_mutated_rejection(rejection_layer::region, [](auto& artifact) {
        write_u32_le(artifact, party_descriptor(artifact), 0);
    }, "null party with a nonzero count must be a region rejection");

    require_mutated_rejection(rejection_layer::region, [](auto& artifact) {
        write_u32_le(artifact, party_descriptor(artifact) + 4, 0);
    }, "non-null party with a zero count must be a region rejection");

    require_mutated_rejection(rejection_layer::region, [](auto& artifact) {
        write_u32_le(artifact, party_descriptor(artifact), 2);
    }, "misaligned party must be a region rejection");

    require_mutated_rejection(rejection_layer::region, [](auto& artifact) {
        write_u32_le(artifact, party_descriptor(artifact),
                     static_cast<std::uint32_t>(payload_size(artifact) + 1));
    }, "out-of-bounds party must be a region rejection");

    require_mutated_rejection(rejection_layer::region, [](auto& artifact) {
        write_u32_le(artifact, party_descriptor(artifact) + 4,
                     std::numeric_limits<std::uint32_t>::max());
    }, "oversized party extent must be a region rejection");
}

void test_name_range_rejections() {
    require_mutated_rejection(rejection_layer::region, [](auto& artifact) {
        const auto name = payload_field(entity_payload_offset(artifact, 0) +
                                        offsetof(Entity, name));
        write_u32_le(artifact, name, 0);
    }, "null name with a nonzero size must be a region rejection");

    require_mutated_rejection(rejection_layer::region, [](auto& artifact) {
        const auto name = payload_field(entity_payload_offset(artifact, 0) +
                                        offsetof(Entity, name));
        write_u32_le(artifact, name + 4, 0);
    }, "non-null name with a zero size must be a region rejection");

    require_mutated_rejection(rejection_layer::region, [](auto& artifact) {
        const auto name = payload_field(entity_payload_offset(artifact, 0) +
                                        offsetof(Entity, name));
        write_u32_le(artifact, name,
                     static_cast<std::uint32_t>(payload_size(artifact) + 2));
    }, "out-of-bounds name must be a region rejection");

    require_mutated_rejection(rejection_layer::region, [](auto& artifact) {
        const auto name = payload_field(entity_payload_offset(artifact, 0) +
                                        offsetof(Entity, name));
        write_u32_le(artifact, name,
                     static_cast<std::uint32_t>(payload_size(artifact)));
        write_u32_le(artifact, name + 4, 2);
    }, "name offset-plus-size beyond payload must be a region rejection");

    require_mutated_rejection(rejection_layer::region, [](auto& artifact) {
        const auto name = payload_field(entity_payload_offset(artifact, 0) +
                                        offsetof(Entity, name));
        write_u32_le(artifact, name,
                     static_cast<std::uint32_t>(
                         root_payload_offset(artifact) + 1));
    }, "name overlapping the root must be a region rejection");

    require_mutated_rejection(rejection_layer::region, [](auto& artifact) {
        const auto name = payload_field(entity_payload_offset(artifact, 0) +
                                        offsetof(Entity, name));
        write_u32_le(artifact, name,
                     static_cast<std::uint32_t>(
                         entity_payload_offset(artifact, 0) + 1));
    }, "name overlapping entities must be a region rejection");

    require_mutated_rejection(rejection_layer::region, [](auto& artifact) {
        const auto first_name = payload_field(
            entity_payload_offset(artifact, 0) + offsetof(Entity, name));
        const auto second_name = payload_field(
            entity_payload_offset(artifact, 1) + offsetof(Entity, name));
        write_u32_le(artifact, second_name,
                     read_u32_le(artifact, first_name));
    }, "overlapping names must be a region rejection");
}

void test_owning_range_overlap_rejection() {
    require_mutated_rejection(rejection_layer::region, [](auto& artifact) {
        write_u32_le(artifact, party_descriptor(artifact),
                     read_u32_le(artifact, index_descriptor(artifact)));
    }, "overlapping index and party ranges must be a region rejection");
}

void test_index_semantic_rejections() {
    require_mutated_rejection(rejection_layer::region, [](auto& artifact) {
        write_u64_le(artifact, payload_field(index_payload_offset(artifact, 0)),
                     3001);
    }, "unsorted index keys must be a region rejection");

    require_mutated_rejection(rejection_layer::region, [](auto& artifact) {
        write_u64_le(artifact, payload_field(index_payload_offset(artifact, 1)),
                     hero_id);
    }, "duplicate index keys must be a region rejection");

    require_mutated_rejection(rejection_layer::region, [](auto& artifact) {
        write_u32_le(artifact, index_descriptor(artifact) + 4, 1);
    }, "index size different from entity count must be a region rejection");

    require_mutated_rejection(rejection_layer::region, [](auto& artifact) {
        write_u64_le(artifact,
                     payload_field(entity_payload_offset(artifact, 1) +
                                   offsetof(Entity, id)),
                     hero_id);
    }, "duplicate entity IDs must be a region rejection");

    require_mutated_rejection(rejection_layer::region, [](auto& artifact) {
        write_u32_le(artifact,
                     payload_field(index_payload_offset(artifact, 1) +
                                   offsetof(EntityIndexEntry, value)),
                     2);
    }, "index value outside entities must be a region rejection");

    require_mutated_rejection(rejection_layer::region, [](auto& artifact) {
        write_u64_le(artifact, payload_field(index_payload_offset(artifact, 0)),
                     1000);
    }, "index key differing from entity ID must be a region rejection");

    require_mutated_rejection(rejection_layer::region, [](auto& artifact) {
        write_u32_le(artifact,
                     payload_field(index_payload_offset(artifact, 1) +
                                   offsetof(EntityIndexEntry, value)),
                     0);
    }, "entity missing from index coverage must be a region rejection");
}

void test_graph_rejections() {
    require_mutated_rejection(rejection_layer::graph, [](auto& artifact) {
        write_u32_le(artifact,
                     root_field(artifact, offsetof(WorldSnapshot, local_player)),
                     static_cast<std::uint32_t>(payload_size(artifact) + 1));
    }, "out-of-region local player must be a graph rejection");

    require_mutated_rejection(rejection_layer::graph, [](auto& artifact) {
        const auto target = payload_field(entity_payload_offset(artifact, 0) +
                                          offsetof(Entity, target));
        write_u32_le(artifact, target,
                     static_cast<std::uint32_t>(
                         entity_payload_offset(artifact, 0) + 2));
    }, "misaligned entity link must be a graph rejection");

    require_mutated_rejection(rejection_layer::graph, [](auto& artifact) {
        const auto target = payload_field(entity_payload_offset(artifact, 0) +
                                          offsetof(Entity, target));
        write_u32_le(artifact, target,
                     static_cast<std::uint32_t>(
                         entity_payload_offset(artifact, 0) +
                         alignof(Entity) + 1));
    }, "entity link into an entity middle must be a graph rejection");
}

} // namespace

int main() {
    test_exact_v1_envelope();
    test_envelope_rejections();
    test_encoder_rejects_out_of_envelope_bounds();
    test_world_checkpoint_orchestration();
    test_root_region_rejections();
    test_entity_range_rejections();
    test_index_range_rejections();
    test_party_range_rejections();
    test_name_range_rejections();
    test_owning_range_overlap_rejection();
    test_index_semantic_rejections();
    test_graph_rejections();
}
