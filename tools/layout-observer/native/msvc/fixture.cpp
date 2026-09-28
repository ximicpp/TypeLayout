#include "../fixtures.hpp"
#include "../json.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <type_traits>

#ifndef TYPELAYOUT_FIXTURE_CONFIGURATION
#error fixture build configuration must be supplied by CMake
#endif

// The collector reads this named PE data export; it never executes the input PE.
extern "C" __declspec(dllexport) const char typelayout_fixture_build[] =
    "typelayout-msvc-fixture-v1|" TYPELAYOUT_FIXTURE_CONFIGURATION "|" TYPELAYOUT_STRINGIFY(_MSC_FULL_VER);

namespace msvc_fixtures {
class Private {
    std::uint8_t tag{};
    std::int32_t count{};
public:
    static std::size_t count_offset() { return offsetof(Private, count); }
};
struct Empty {};
struct Overlapping { [[msvc::no_unique_address]] Empty empty; std::int32_t value; };
}

// Actual instances keep the required UDTs in full private PDBs in both builds.
fixtures::Sample native_sample{};
fixtures::Packed native_packed{};
fixtures::Reordered native_reordered{};
fixtures::Nested native_nested{};
fixtures::Array native_array{};
fixtures::Enum native_enum{};
fixtures::Base native_base{};
fixtures::Derived native_derived{};
fixtures::Bits native_bits{};
fixtures::Union native_union{};
fixtures::Polymorphic native_polymorphic{};
fixtures::Virtual native_virtual{};
msvc_fixtures::Private native_private{};
msvc_fixtures::Overlapping native_overlap{};

__declspec(noinline) void preserve(const void* value) {
    static const void* volatile last;
    last = value;
}

static_assert(std::is_standard_layout_v<fixtures::Sample>);
static_assert(sizeof(fixtures::Sample) == 12 && alignof(fixtures::Sample) == 4);
static_assert(sizeof(fixtures::Packed) == 7 && alignof(fixtures::Packed) == 1);

int main() {
    preserve(&native_sample); preserve(&native_packed); preserve(&native_reordered);
    preserve(&native_nested); preserve(&native_array); preserve(&native_enum);
    preserve(&native_base); preserve(&native_derived); preserve(&native_bits);
    preserve(&native_union); preserve(&native_polymorphic); preserve(&native_virtual); preserve(&native_private); preserve(&native_overlap);
    native_bits.first = 5; native_bits.second = 17;
    unsigned char first = 0;
    std::memcpy(&first, &native_bits, 1); // The two fields fill this byte, no padding.
    if (first != (5u | (17u << 3))) return 1;
    const auto derived_start = reinterpret_cast<std::uintptr_t>(&native_derived);
    const auto base_start = reinterpret_cast<std::uintptr_t>(static_cast<fixtures::Base*>(&native_derived));
    const auto extra_start = reinterpret_cast<std::uintptr_t>(&native_derived.extra);
    using observer::Json;
    const auto record = Json::object({{"configuration", TYPELAYOUT_FIXTURE_CONFIGURATION},
        {"compiler", TYPELAYOUT_STRINGIFY(_MSC_FULL_VER)},
        {"pointerBits", static_cast<std::int64_t>(sizeof(void*) * 8)},
        {"sampleSize", static_cast<std::int64_t>(sizeof(fixtures::Sample))},
        {"sampleAlign", static_cast<std::int64_t>(alignof(fixtures::Sample))},
        {"sampleCountOffset", static_cast<std::int64_t>(offsetof(fixtures::Sample, count))},
        {"packedSize", static_cast<std::int64_t>(sizeof(fixtures::Packed))},
        {"packedCodeOffset", static_cast<std::int64_t>(offsetof(fixtures::Packed, code))},
        {"nestedExtraOffset", static_cast<std::int64_t>(offsetof(fixtures::Nested, extra))},
        {"arraySize", static_cast<std::int64_t>(sizeof(fixtures::Array))},
        {"enumSize", static_cast<std::int64_t>(sizeof(fixtures::Enum))},
        {"bitsSize", static_cast<std::int64_t>(sizeof(fixtures::Bits))},
        {"bitsValueOffset", static_cast<std::int64_t>(offsetof(fixtures::Bits, value))},
        {"privateSize", static_cast<std::int64_t>(sizeof(msvc_fixtures::Private))},
        {"privateCountOffset", static_cast<std::int64_t>(msvc_fixtures::Private::count_offset())},
        {"derivedSize", static_cast<std::int64_t>(sizeof(fixtures::Derived))},
        {"derivedBaseOffset", static_cast<std::int64_t>(base_start - derived_start)},
        {"derivedExtraOffset", static_cast<std::int64_t>(extra_start - derived_start)}});
    record.write(std::cout); std::cout << '\n';
}
