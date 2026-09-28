#pragma once

#include <cstdint>

namespace fixtures {
#pragma pack(push, 4)
struct Sample { std::uint8_t tag; std::int32_t count; std::int16_t code; };
#pragma pack(pop)
#pragma pack(push, 1)
struct Packed { std::uint8_t tag; std::int32_t count; std::int16_t code; };
#pragma pack(pop)
struct Reordered { std::int32_t count; std::int16_t code; std::uint8_t tag; };
struct Nested { Sample payload; std::int32_t extra; };
struct Array { std::int32_t values[3]; };
enum class Code : std::uint16_t { first = 1, second = 2 };
struct Enum { Code value; };
struct Base { std::int32_t base; };
struct Derived : Base { std::int32_t extra; };
struct Empty {};
struct EmptyMember { Empty empty; std::int32_t value; };
struct EmptyBase : Empty { std::int32_t value; };
struct Overlap { [[no_unique_address]] Empty empty; std::int32_t value; };
struct Bits { std::uint32_t first : 3; std::uint32_t second : 5; std::uint32_t value; };
union Union { std::int32_t integer; float real; };
struct Opaque { std::int32_t hidden; };
struct OpaqueHost { Opaque payload; };
struct Nontrivial { std::int32_t value; ~Nontrivial() {} };
struct Polymorphic { virtual ~Polymorphic() = default; std::int32_t value; };
struct Virtual : virtual Base { std::int32_t value; };
struct Pointer { std::int32_t* pointer; };
struct Boolean { bool value; };
struct Utf16 { char16_t value; };
} // namespace fixtures
