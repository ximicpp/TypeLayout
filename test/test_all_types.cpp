// Boost.TypeLayout - Type Coverage Tests
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#include <cstdint>
#include <boost/typelayout/typelayout.hpp>

using namespace boost::typelayout;

// Fixed-width integers
static_assert(get_layout_signature<int8_t>() == "[64-le]i8[s:1,a:1]");
static_assert(get_layout_signature<uint8_t>() == "[64-le]u8[s:1,a:1]");
static_assert(get_layout_signature<int16_t>() == "[64-le]i16[s:2,a:2]");
static_assert(get_layout_signature<uint16_t>() == "[64-le]u16[s:2,a:2]");
static_assert(get_layout_signature<int32_t>() == "[64-le]i32[s:4,a:4]");
static_assert(get_layout_signature<uint32_t>() == "[64-le]u32[s:4,a:4]");
static_assert(get_layout_signature<int64_t>() == "[64-le]i64[s:8,a:8]");
static_assert(get_layout_signature<uint64_t>() == "[64-le]u64[s:8,a:8]");

// Floating point
static_assert(get_layout_signature<float>() == "[64-le]f32[s:4,a:4]");
static_assert(get_layout_signature<double>() == "[64-le]f64[s:8,a:8]");

// Characters
static_assert(get_layout_signature<char>() == "[64-le]char[s:1,a:1]");
static_assert(get_layout_signature<char8_t>() == "[64-le]char8[s:1,a:1]");
static_assert(get_layout_signature<char16_t>() == "[64-le]char16[s:2,a:2]");
static_assert(get_layout_signature<char32_t>() == "[64-le]char32[s:4,a:4]");
static_assert(get_layout_signature<bool>() == "[64-le]bool[s:1,a:1]");

// Standard integers (platform-dependent sizes)
static_assert(sizeof(signed char) == 1);
static_assert(sizeof(long) == 4 || sizeof(long) == 8);
static_assert(sizeof(long long) == 8);

// wchar_t: 2 bytes on Windows, 4 bytes on Linux/macOS
#if defined(_WIN32)
static_assert(get_layout_signature<wchar_t>() == "[64-le]wchar[s:2,a:2]");
#else
static_assert(get_layout_signature<wchar_t>() == "[64-le]wchar[s:4,a:4]");
#endif

// Pointers (8 bytes on 64-bit)
static_assert(get_layout_signature<void*>() == "[64-le]ptr[s:8,a:8]");
static_assert(get_layout_signature<int*>() == "[64-le]ptr[s:8,a:8]");
static_assert(get_layout_signature<const char*>() == "[64-le]ptr[s:8,a:8]");
static_assert(get_layout_signature<void**>() == "[64-le]ptr[s:8,a:8]");

// References
static_assert(get_layout_signature<int&>() == "[64-le]ref[s:8,a:8]");
static_assert(get_layout_signature<int&&>() == "[64-le]rref[s:8,a:8]");

// nullptr
static_assert(get_layout_signature<std::nullptr_t>() == "[64-le]nullptr[s:8,a:8]");

// Arrays
static_assert(get_layout_signature<char[16]>() == "[64-le]bytes[s:16,a:1]");
static_assert(get_layout_signature<int32_t[4]>() == "[64-le]array[s:16,a:4]<i32[s:4,a:4],4>");
static_assert(get_layout_signature<double[3]>() == "[64-le]array[s:24,a:8]<f64[s:8,a:8],3>");
static_assert(get_layout_signature<int32_t[2][3]>() == "[64-le]array[s:24,a:4]<array[s:12,a:4]<i32[s:4,a:4],3>,2>");

// Structs
struct SimpleStruct { int32_t a; int32_t b; };
static_assert(get_layout_signature<SimpleStruct>() == 
    "[64-le]struct[s:8,a:4]{@0[a]:i32[s:4,a:4],@4[b]:i32[s:4,a:4]}");

struct SimplePoint { int32_t x; int32_t y; };
static_assert(get_layout_signature<SimplePoint>() ==
    "[64-le]struct[s:8,a:4]{@0[x]:i32[s:4,a:4],@4[y]:i32[s:4,a:4]}");

// Classes with private members
class SimpleClass {
public:
    SimpleClass(int32_t a, int32_t b) : a_(a), b_(b) {}
    int32_t getA() const { return a_; }
private:
    int32_t a_;
    int32_t b_;
};
static_assert(get_layout_signature<SimpleClass>() == 
    "[64-le]struct[s:8,a:4]{@0[a_]:i32[s:4,a:4],@4[b_]:i32[s:4,a:4]}");
static_assert(LayoutSupported<SimpleClass>);

class MixedAccessClass {
public:
    int32_t pub_val;
    MixedAccessClass() : pub_val(0), prot_val(0), priv_val(0) {}
protected:
    int32_t prot_val;
private:
    int32_t priv_val;
};
static_assert(sizeof(MixedAccessClass) == 12);
static_assert(LayoutSupported<MixedAccessClass>);

class NonTrivialClass {
public:
    NonTrivialClass(uint64_t id) : id_(id), active_(true) {}
    ~NonTrivialClass() { active_ = false; }
    uint64_t getId() const { return id_; }
private:
    uint64_t id_;
    bool active_;
};
static_assert(get_layout_signature<NonTrivialClass>() ==
    "[64-le]struct[s:16,a:8]{@0[id_]:u64[s:8,a:8],@8[active_]:bool[s:1,a:1]}");

// Static members excluded from layout
class WithStaticMembers {
public:
    static int32_t counter;
    static constexpr float PI = 3.14159f;
    int32_t instance_val;
    double instance_data;
};
static_assert(get_layout_signature<WithStaticMembers>() ==
    "[64-le]struct[s:16,a:8]{@0[instance_val]:i32[s:4,a:4],@8[instance_data]:f64[s:8,a:8]}");

// Template instantiation
template<typename T> class GenericContainer {
public:
    GenericContainer(T val, uint32_t sz) : value_(val), size_(sz) {}
private:
    T value_;
    uint32_t size_;
};
static_assert(get_layout_signature<GenericContainer<int32_t>>() == 
    "[64-le]struct[s:8,a:4]{@0[value_]:i32[s:4,a:4],@4[size_]:u32[s:4,a:4]}");
static_assert(get_layout_signature<GenericContainer<double>>() == 
    "[64-le]struct[s:16,a:8]{@0[value_]:f64[s:8,a:8],@8[size_]:u32[s:4,a:4]}");

// Nested structs
struct Inner { uint16_t val; };
struct Outer { Inner inner; uint32_t extra; };
static_assert(get_layout_signature<Outer>() == 
    "[64-le]struct[s:8,a:4]{@0[inner]:struct[s:2,a:2]{@0[val]:u16[s:2,a:2]},@4[extra]:u32[s:4,a:4]}");

// Empty struct
struct EmptyStruct {};
static_assert(sizeof(EmptyStruct) == 1);

// Inheritance
struct Base1 { uint64_t id; };
struct Derived1 : Base1 { uint32_t value; };
static_assert(get_layout_signature<Derived1>() == 
    "[64-le]class[s:16,a:8,inherited]{@0[base]:struct[s:8,a:8]{@0[id]:u64[s:8,a:8]},@8[value]:u32[s:4,a:4]}");

// Virtual inheritance
struct VBase { uint64_t vb_id; };
struct VDerived1 : virtual VBase { float v1; };
struct VDerived2 : virtual VBase { double v2; };
struct Diamond : VDerived1, VDerived2 { int32_t d_val; };

// Polymorphic classes
class Polymorphic {
public:
    virtual ~Polymorphic() = default;
private:
    int32_t data;
};
static_assert(sizeof(Polymorphic) == 16);

// Bit-fields
struct BitField1 { uint8_t a : 1; uint8_t b : 2; uint8_t c : 3; uint8_t d : 2; };
static_assert(sizeof(BitField1) == 1);

struct BitField2 { uint32_t x : 10; uint32_t y : 10; uint32_t z : 12; };
static_assert(sizeof(BitField2) == 4);

// Enums
enum class ScopedU8 : uint8_t { A, B };
static_assert(get_layout_signature<ScopedU8>() == "[64-le]enum[s:1,a:1]<u8[s:1,a:1]>");

enum class ScopedI32 : int32_t { X = -1, Y = 0, Z = 1 };
static_assert(get_layout_signature<ScopedI32>() == "[64-le]enum[s:4,a:4]<i32[s:4,a:4]>");

// Unions
union TestUnion { int32_t i; float f; char bytes[4]; };
static_assert(get_layout_signature<TestUnion>() == "[64-le]union[s:4,a:4]{@0[i]:i32[s:4,a:4],@0[f]:f32[s:4,a:4],@0[bytes]:bytes[s:4,a:1]}");

union BigUnion { double d; uint64_t u; char buf[16]; };
static_assert(get_layout_signature<BigUnion>() == "[64-le]union[s:16,a:8]{@0[d]:f64[s:8,a:8],@0[u]:u64[s:8,a:8],@0[buf]:bytes[s:16,a:1]}");

// alignas
struct alignas(16) Aligned16 { int32_t x; int32_t y; };
static_assert(alignof(Aligned16) == 16);
static_assert(sizeof(Aligned16) == 16);

// Empty base optimization
struct EmptyBase {};
struct EBODerived : EmptyBase { int32_t value; };
static_assert(sizeof(EBODerived) == 4);

// Member pointers
struct TestClass { int32_t member; void method() {} virtual void vmethod() {} };
using DataMemberPtr = int32_t TestClass::*;
static_assert(get_layout_signature<DataMemberPtr>() == "[64-le]memptr[s:8,a:8]");

// CV qualifiers stripped
static_assert(get_layout_signature<const int32_t>() == "[64-le]i32[s:4,a:4]");
static_assert(get_layout_signature<volatile int32_t>() == "[64-le]i32[s:4,a:4]");
static_assert(get_layout_signature<const volatile int32_t>() == "[64-le]i32[s:4,a:4]");

// Template containers
template<typename T> struct Container { T value; uint32_t size; };
static_assert(get_layout_signature<Container<int32_t>>() == 
    "[64-le]struct[s:8,a:4]{@0[value]:i32[s:4,a:4],@4[size]:u32[s:4,a:4]}");

// Struct with pointers
struct WithPointers { int32_t* ptr; const char* str; void* data; };
static_assert(get_layout_signature<WithPointers>() ==
    "[64-le]struct[s:24,a:8]{@0[ptr]:ptr[s:8,a:8],@8[str]:ptr[s:8,a:8],@16[data]:ptr[s:8,a:8]}");

// Struct with arrays
struct WithArrays { int32_t values[4]; char name[16]; };
static_assert(get_layout_signature<WithArrays>() ==
    "[64-le]struct[s:32,a:4]{@0[values]:array[s:16,a:4]<i32[s:4,a:4],4>,@16[name]:bytes[s:16,a:1]}");

// std::byte
static_assert(get_layout_signature<std::byte>() == "[64-le]byte[s:1,a:1]");
static_assert(get_layout_signature<std::byte[8]>() == "[64-le]array[s:8,a:1]<byte[s:1,a:1],8>");

// Function pointers
using VoidFn = void(*)();
static_assert(get_layout_signature<VoidFn>() == "[64-le]fnptr[s:8,a:8]");
using IntFn = int(*)(int, int);
static_assert(get_layout_signature<IntFn>() == "[64-le]fnptr[s:8,a:8]");
using NoexceptFn = void(*)() noexcept;
static_assert(get_layout_signature<NoexceptFn>() == "[64-le]fnptr[s:8,a:8]");

struct WithFnPtr { void (*callback)(int); void* user_data; };
static_assert(get_layout_signature<WithFnPtr>() ==
    "[64-le]struct[s:16,a:8]{@0[callback]:fnptr[s:8,a:8],@8[user_data]:ptr[s:8,a:8]}");

// Cross-type compatibility
struct TypeA { int32_t x; int32_t y; };
struct TypeB { int32_t x; int32_t y; };
struct TypeC { int32_t a; int32_t b; };
struct TypeD { int32_t x; int64_t y; };

static_assert(signatures_match<TypeA, TypeB>());
static_assert(!signatures_match<TypeA, TypeC>());
static_assert(!signatures_match<TypeA, TypeD>());

// Concepts
static_assert(LayoutCompatible<TypeA, TypeB>);
static_assert(!LayoutCompatible<TypeA, TypeC>);
static_assert(LayoutMatch<SimplePoint, "[64-le]struct[s:8,a:4]{@0[x]:i32[s:4,a:4],@4[y]:i32[s:4,a:4]}">);
static_assert(LayoutMatch<int32_t, "[64-le]i32[s:4,a:4]">);

template<typename T>
    requires LayoutMatch<T, "[64-le]struct[s:8,a:4]{@0[x]:i32[s:4,a:4],@4[y]:i32[s:4,a:4]}">
constexpr bool requires_point_layout() { return true; }
static_assert(requires_point_layout<SimplePoint>());

// Core API
static_assert(get_arch_prefix() == "[64-le]" || get_arch_prefix() == "[64-be]" ||
              get_arch_prefix() == "[32-le]" || get_arch_prefix() == "[32-be]");
static_assert(hashes_match<TypeA, TypeB>());
static_assert(!hashes_match<TypeA, TypeC>());
static_assert(get_layout_hash<int32_t>() == get_layout_hash<int32_t>());
static_assert(get_layout_hash<int32_t>() != get_layout_hash<int64_t>());

TYPELAYOUT_BIND(SimplePoint, "[64-le]struct[s:8,a:4]{@0[x]:i32[s:4,a:4],@4[y]:i32[s:4,a:4]}");
TYPELAYOUT_BIND(int32_t, "[64-le]i32[s:4,a:4]");

// Extended concepts
constexpr uint64_t EXPECTED_INT32_HASH = get_layout_hash<int32_t>();
static_assert(LayoutHashMatch<int32_t, EXPECTED_INT32_HASH>);

constexpr uint64_t EXPECTED_POINT_HASH = get_layout_hash<SimplePoint>();
static_assert(LayoutHashMatch<SimplePoint, EXPECTED_POINT_HASH>);

static_assert(LayoutHashCompatible<TypeA, TypeB>);
static_assert(!LayoutHashCompatible<TypeA, TypeC>);

template<typename T, typename U>
    requires LayoutHashCompatible<T, U>
constexpr bool hash_compatible_types() { return true; }
static_assert(hash_compatible_types<TypeA, TypeB>());

// Verification API
constexpr auto point_verification = get_layout_verification<SimplePoint>();
static_assert(point_verification.fnv1a != 0);
static_assert(point_verification.djb2 != 0);
static_assert(point_verification.length > 0);

static_assert(verifications_match<TypeA, TypeB>());
static_assert(!verifications_match<TypeA, TypeC>());

constexpr auto int32_v = get_layout_verification<int32_t>();
constexpr auto int64_v = get_layout_verification<int64_t>();
static_assert(int32_v != int64_v);

// Collision detection (types with different layouts)
static_assert(no_hash_collision<int8_t, int16_t, int32_t, int64_t>());
static_assert(no_hash_collision<SimplePoint, Inner, Outer, TypeC>());
static_assert(no_hash_collision<float, double, bool, char>());

static_assert(no_verification_collision<int8_t, int16_t, int32_t, int64_t>());
static_assert(no_verification_collision<SimplePoint, Inner, Outer, TypeC>());

// Edge cases
static_assert(no_hash_collision<int32_t>());
static_assert(no_verification_collision<int32_t>());
static_assert(no_hash_collision<>());
static_assert(no_verification_collision<>());

// C string API
static_assert(get_layout_signature_cstr<int32_t>() != nullptr);
static_assert(get_layout_signature_cstr<int32_t>()[0] == '[');

// Hash algorithm properties
constexpr auto simple_v = get_layout_verification<SimpleStruct>();
static_assert(simple_v.fnv1a != 0 && simple_v.djb2 != 0);

// Variable templates (new API)
static_assert(layout_hash_v<int32_t> == get_layout_hash<int32_t>());
static_assert(layout_hash_v<SimplePoint> == get_layout_hash<SimplePoint>());
static_assert(layout_signature_v<int32_t> == get_layout_signature<int32_t>());

// Negative tests (should NOT match)
struct DifferentSize1 { int32_t x; };
struct DifferentSize2 { int64_t x; };
static_assert(!signatures_match<DifferentSize1, DifferentSize2>());
static_assert(!hashes_match<DifferentSize1, DifferentSize2>());
static_assert(!LayoutCompatible<DifferentSize1, DifferentSize2>);
static_assert(!LayoutHashCompatible<DifferentSize1, DifferentSize2>);

struct DifferentAlign1 { alignas(4) int32_t x; };
struct DifferentAlign2 { alignas(8) int32_t x; };
static_assert(!signatures_match<DifferentAlign1, DifferentAlign2>());

struct DifferentFieldCount1 { int32_t x; };
struct DifferentFieldCount2 { int32_t x; int32_t y; };
static_assert(!signatures_match<DifferentFieldCount1, DifferentFieldCount2>());

// Platform traits (documentation)
static_assert(detail::int8_is_signed_char || !detail::int8_is_signed_char);
static_assert(detail::int64_is_long || !detail::int64_is_long);
static_assert(detail::int64_is_long_long || !detail::int64_is_long_long);

int main() { return 0; }
