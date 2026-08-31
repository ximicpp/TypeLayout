#include "unit_checkpoint.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

using namespace relocatable_unit_handoff_demo;

namespace {

void expect(bool condition) {
    if (!condition) {
        std::abort();
    }
}

template <typename Exception, typename Function>
void expect_throws(Function&& function) {
    try {
        std::forward<Function>(function)();
    } catch (const Exception&) {
        return;
    }
    std::abort();
}

template <typename Function>
void expect_rejection(Function&& function, unit_rejection_layer layer) {
    try {
        std::forward<Function>(function)();
    } catch (const unit_checkpoint_error& error) {
        expect(error.layer() == layer);
        return;
    }
    std::abort();
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

void test_envelope_and_no_fixup_round_trip() {
    auto source = build_canonical_migrating_unit();
    const auto source_offsets = capture_unit_offsets(source);
    const auto artifact = save_unit_checkpoint(source);

    expect(artifact.size() ==
           unit_checkpoint_header_size + source.used_bytes().size());
    expect(std::equal(unit_checkpoint_magic.begin(),
                      unit_checkpoint_magic.end(), artifact.begin()));
    expect(read_u16_le(artifact, 8) == 1);
    expect(read_u16_le(artifact, 10) == unit_checkpoint_header_size);
    expect(std::equal(unit_checkpoint_format.begin(),
                      unit_checkpoint_format.end(), artifact.begin() + 12));
    expect(read_u32_le(artifact, 16) == source.used_bytes().size());
    expect(read_u32_le(artifact, 20) == source_offsets.root_offset);
    expect(read_u32_le(artifact, 24) == 0);
    expect(std::equal(unit_checkpoint_schema.begin(),
                      unit_checkpoint_schema.end(), artifact.begin() + 32));

    const auto decoded = decode_unit_checkpoint_envelope(artifact);
    expect(decoded.root_offset == source_offsets.root_offset);
    expect(std::equal(decoded.payload.begin(), decoded.payload.end(),
                      source.used_bytes().begin(), source.used_bytes().end()));

    auto loaded = load_unit_checkpoint(artifact);
    expect(source.used_bytes().data() != loaded.used_bytes().data());
    expect(std::equal(source.used_bytes().begin(), source.used_bytes().end(),
                      loaded.used_bytes().begin(), loaded.used_bytes().end()));
    expect(capture_unit_offsets(loaded) == source_offsets);
    expect(canonical_migrating_unit_matches(loaded, 300));
}

void test_envelope_rejections() {
    const auto canonical = save_unit_checkpoint(
        build_canonical_migrating_unit());

    {
        auto artifact = canonical;
        artifact.resize(unit_checkpoint_header_size - 1);
        expect_rejection([&] { load_unit_checkpoint(artifact); },
                         unit_rejection_layer::envelope);
    }
    {
        auto artifact = canonical;
        artifact.push_back(std::byte{0});
        expect_rejection([&] { load_unit_checkpoint(artifact); },
                         unit_rejection_layer::envelope);
    }
    for (const auto offset : {std::size_t{0}, std::size_t{12},
                              std::size_t{32}}) {
        auto artifact = canonical;
        artifact[offset] ^= std::byte{1};
        expect_rejection([&] { load_unit_checkpoint(artifact); },
                         unit_rejection_layer::envelope);
    }
    {
        auto artifact = canonical;
        write_u16_le(artifact, 8, 2);
        expect_rejection([&] { load_unit_checkpoint(artifact); },
                         unit_rejection_layer::envelope);
    }
    {
        auto artifact = canonical;
        write_u16_le(artifact, 10, 39);
        expect_rejection([&] { load_unit_checkpoint(artifact); },
                         unit_rejection_layer::envelope);
    }
    {
        auto artifact = canonical;
        write_u32_le(artifact, 24, 1);
        expect_rejection([&] { load_unit_checkpoint(artifact); },
                         unit_rejection_layer::envelope);
    }
    {
        auto artifact = canonical;
        write_u32_le(artifact, 28, read_u32_le(artifact, 28) ^ 1u);
        expect_rejection([&] { load_unit_checkpoint(artifact); },
                         unit_rejection_layer::envelope);
    }
    {
        auto artifact = canonical;
        artifact.back() ^= std::byte{1};
        expect_rejection([&] { load_unit_checkpoint(artifact); },
                         unit_rejection_layer::envelope);
    }
}

void test_valid_checksum_still_rejects_bad_region_and_graph() {
    auto source = build_canonical_migrating_unit();
    const auto offsets = capture_unit_offsets(source);
    const auto canonical = save_unit_checkpoint(source);
    const auto decoded = decode_unit_checkpoint_envelope(canonical);

    const auto misaligned_root = encode_unit_checkpoint(
        decoded.payload, decoded.root_offset + 1);
    expect_rejection([&] { load_unit_checkpoint(misaligned_root); },
                     unit_rejection_layer::region);

    std::vector<std::byte> corrupt_payload(
        decoded.payload.begin(), decoded.payload.end());
    const auto next_offset = static_cast<std::size_t>(
        offsets.effects_data - 1 + offsetof(Effect, next));
    const std::uint32_t invalid =
        static_cast<std::uint32_t>(relocatable_world_demo::region_capacity + 1);
    std::memcpy(corrupt_payload.data() + next_offset,
                &invalid, sizeof(invalid));
    const auto corrupt_graph = encode_unit_checkpoint(
        corrupt_payload, decoded.root_offset);
    const auto corrupt_decoded = decode_unit_checkpoint_envelope(corrupt_graph);
    expect(std::equal(corrupt_payload.begin(), corrupt_payload.end(),
                      corrupt_decoded.payload.begin(),
                      corrupt_decoded.payload.end()));
    expect_rejection([&] { load_unit_checkpoint(corrupt_graph); },
                     unit_rejection_layer::graph);
}

void test_encode_and_save_preconditions() {
    std::vector<std::byte> oversized(
        relocatable_world_demo::region_capacity + 1);
    expect_rejection(
        [&] { encode_unit_checkpoint(oversized, 0); },
        unit_rejection_layer::envelope);

    relocatable_world_demo::RegionBuilder builder;
    const auto root = populate_canonical_migrating_unit(builder);
    auto unvalidated = std::move(builder).finish(root);
    expect_throws<std::logic_error>([&] {
        static_cast<void>(save_unit_checkpoint(unvalidated));
    });
}

} // namespace

int main() {
    test_envelope_and_no_fixup_round_trip();
    test_envelope_rejections();
    test_valid_checksum_still_rejects_bad_region_and_graph();
    test_encode_and_save_preconditions();
}
