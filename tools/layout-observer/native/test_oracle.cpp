#include "fixtures.hpp"
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <type_traits>

// Independent native evidence: ordinary offsetof/sizeof/alignof and a bitfield
// value-bit check. No observation descriptor or reflection API is included.
static_assert(std::is_standard_layout_v<fixtures::Sample>);
static_assert(sizeof(fixtures::Sample) == 12 && alignof(fixtures::Sample) == 4);
static_assert(offsetof(fixtures::Sample, tag) == 0 && offsetof(fixtures::Sample, count) == 4 && offsetof(fixtures::Sample, code) == 8);
static_assert(sizeof(fixtures::Packed) == 7 && alignof(fixtures::Packed) == 1);
static_assert(offsetof(fixtures::Packed, count) == 1 && offsetof(fixtures::Packed, code) == 5);
static_assert(sizeof(fixtures::Nested) == 16 && offsetof(fixtures::Nested, extra) == 12);
static_assert(sizeof(fixtures::Array) == 12);
static_assert(sizeof(fixtures::Empty) == 1 && offsetof(fixtures::EmptyMember, value) >= 1);
static_assert(!std::is_trivially_copyable_v<fixtures::Nontrivial>);

int main() {
    if constexpr (std::endian::native == std::endian::little) {
        fixtures::Bits bits{};
        bits.first = 5; bits.second = 17;
        unsigned char first = 0;
        // Both bitfields fill this complete byte; no indeterminate padding read.
        std::memcpy(&first, &bits, 1);
        if (first != (5u | (17u << 3))) return 1;
    }
    fixtures::Sample elements[2]{};
    const auto stride = reinterpret_cast<const unsigned char*>(&elements[1]) - reinterpret_cast<const unsigned char*>(&elements[0]);
    if (stride != sizeof(fixtures::Sample)) return 1;
    std::cout << "{\"sampleSize\":" << sizeof(fixtures::Sample)
        << ",\"sampleAlign\":" << alignof(fixtures::Sample)
        << ",\"sampleCountOffset\":" << offsetof(fixtures::Sample, count)
        << ",\"packedSize\":" << sizeof(fixtures::Packed)
        << ",\"packedCodeOffset\":" << offsetof(fixtures::Packed, code)
        << ",\"nestedExtraOffset\":" << offsetof(fixtures::Nested, extra)
        << ",\"emptyValueOffset\":" << offsetof(fixtures::EmptyMember, value) << "}\n";
}
