// test_core.cpp -- Core engine regression tests.
//
// Guards the signature engine (grammar categories) and the
// is_byte_copy_safe admission predicate, including three regressions
// found on 2026-07-13:
//   1. Polymorphic types must be rejected: they are never trivially
//      copyable, so the accept-only fast path falls through to member
//      recursion, which cannot see the hidden vptr.
//   2. A member array of a relocatable opaque type must be accepted,
//      consistent with the same array at top level.
//   3. A trivially copyable struct embedding a pointer-bearing opaque
//      member must be rejected: the pointer token scan cannot see
//      inside O(Tag|N|A).
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#include <boost/typelayout.hpp>

#include <cstdint>
#include <cstdio>

using namespace boost::typelayout;

// ---- Test types (inline, per convention) ----

struct Flat { std::uint32_t a; double b; };
struct FlatSame { std::uint32_t a; double b; };
struct Bits { std::uint32_t a : 3; std::uint32_t b : 5; std::uint32_t c; };
union PodUnion { std::int32_t i; float f; char bytes[8]; };
union PtrUnion { std::int32_t i; void* p; };
enum class Color : std::uint16_t { R, G, B };
struct Nested { Flat f; std::int16_t s; };
struct NestedFlat { std::uint32_t a; double b; std::int16_t s; };
struct Base { std::int32_t x; };
struct Derived : Base { std::int32_t y; };
struct DerivedFlat { std::int32_t x; std::int32_t y; };
struct Empty {};
struct EBO : Empty { std::int32_t v; };
struct NUA { [[no_unique_address]] Empty e; std::int32_t v; };
struct Poly { virtual ~Poly() = default; std::int32_t v; };
struct HasPolyMember { Poly p; std::int32_t x; };
struct DerivedFromPoly : Poly { std::int32_t y; };
struct MultiDim { std::int32_t m[2][3]; };
struct WithPtr { std::int64_t id; void* p; };
struct WithFnPtr { void (*cb)(int); };
struct WithMemPtr { int Flat::*mp; };
struct WithRef { std::int32_t& r; };
struct AnonUnion { std::uint32_t tag; union { std::int32_t i; float f; }; };

// Relocatable opaque: byte-copy safe under a relocation model, not
// trivially copyable in the C++ sense.
struct RelocThing {
    std::int32_t off;
    RelocThing(const RelocThing&) {}
};

// Sealed opaque that internally holds a native pointer (HasPointer = true).
struct SealedHandle { void* impl; };

namespace boost { namespace typelayout { inline namespace v1 {
TYPELAYOUT_OPAQUE_TYPE_RELOCATABLE(RelocThing, "RelocThing")
TYPELAYOUT_REGISTER_OPAQUE(SealedHandle, "SealedHandle", true)
}}}

struct HasRelocMember { RelocThing r; std::int32_t pad; };
struct HasRelocArrayMember { RelocThing arr[4]; };
struct SealedCarrier { std::uint32_t id; SealedHandle h; };

// =========================================================================
// Signature engine: identity and flattening
// =========================================================================

static_assert(get_layout_signature<Flat>() == get_layout_signature<FlatSame>(),
    "identical definitions must produce identical signatures");

static_assert(get_layout_signature<Nested>() == get_layout_signature<NestedFlat>(),
    "nested struct must flatten to the same signature as inline fields");

static_assert(get_layout_signature<Derived>() == get_layout_signature<DerivedFlat>(),
    "base class must flatten to the same signature as inline fields");

static_assert(get_layout_signature<Flat>().value[0] == '[',
    "signature must start with the arch prefix");

// =========================================================================
// Signature engine: grammar category tokens
// =========================================================================

namespace {

constexpr auto bits_sig      = get_layout_signature<Bits>();
constexpr auto union_sig     = get_layout_signature<PodUnion>();
constexpr auto enum_sig      = get_layout_signature<Color>();
constexpr auto empty_sig     = get_layout_signature<Empty>();
constexpr auto ebo_sig       = get_layout_signature<EBO>();
constexpr auto nua_sig       = get_layout_signature<NUA>();
constexpr auto poly_sig      = get_layout_signature<Poly>();
constexpr auto multidim_sig  = get_layout_signature<MultiDim>();
constexpr auto fnptr_sig     = get_layout_signature<WithFnPtr>();
constexpr auto memptr_sig    = get_layout_signature<WithMemPtr>();
constexpr auto ref_sig       = get_layout_signature<WithRef>();
constexpr auto anon_sig      = get_layout_signature<AnonUnion>();
constexpr auto reloc_sig     = get_layout_signature<RelocThing>();
constexpr auto carrier_sig   = get_layout_signature<SealedCarrier>();

} // namespace

static_assert(bits_sig.contains(FixedString{":bits<3,"}) &&
              bits_sig.contains(FixedString{":bits<5,"}),
    "bit-fields must encode as bits<WIDTH,STORAGE>");

static_assert(union_sig.contains(FixedString{"union[s:8,a:4]"}),
    "unions must encode as union[s:SIZE,a:ALIGN]");

static_assert(enum_sig.contains(FixedString{"enum[s:2,a:2]<u16[s:2,a:2]>"}),
    "enums must encode size, alignment, and underlying type");

static_assert(empty_sig.contains(FixedString{"record[s:1,a:1]{}"}),
    "standalone empty types keep sizeof == 1");

static_assert(ebo_sig.contains(FixedString{"@0:record[s:0,a:1]{}"}),
    "empty base (EBO) must be patched to s:0 when embedded");

static_assert(nua_sig.contains(FixedString{"@0:record[s:0,a:1]{}"}),
    "[[no_unique_address]] empty member must be patched to s:0");

static_assert(poly_sig.contains(FixedString{",vptr]"}),
    "polymorphic records must carry the vptr flag");

static_assert(multidim_sig.contains(FixedString{"<array["}),
    "multidimensional arrays must nest array signatures");

static_assert(fnptr_sig.contains(FixedString{"fnptr["}),
    "function pointers must encode as fnptr");

static_assert(memptr_sig.contains(FixedString{"memptr["}),
    "member pointers must encode as memptr");

static_assert(ref_sig.contains(FixedString{":ref["}),
    "reference members must encode as ref");

static_assert(anon_sig.contains(FixedString{"union["}),
    "anonymous union members must embed a union signature");

static_assert(reloc_sig.contains(FixedString{"O(RelocThing|4|4)"}),
    "opaque types must encode as O(Tag|size|align)");

static_assert(carrier_sig.contains(FixedString{"O(SealedHandle|"}),
    "opaque members must stay sealed inside the parent record");

// =========================================================================
// Admission predicate: safe types
// =========================================================================

static_assert(is_byte_copy_safe_v<Flat>);
static_assert(is_byte_copy_safe_v<Bits>);
static_assert(is_byte_copy_safe_v<PodUnion>);
static_assert(is_byte_copy_safe_v<Color>);
static_assert(is_byte_copy_safe_v<Nested>);
static_assert(is_byte_copy_safe_v<Derived>);
static_assert(is_byte_copy_safe_v<Empty>);
static_assert(is_byte_copy_safe_v<EBO>);
static_assert(is_byte_copy_safe_v<NUA>);
static_assert(is_byte_copy_safe_v<MultiDim>);
static_assert(is_byte_copy_safe_v<AnonUnion>);
static_assert(is_byte_copy_safe_v<int>);
static_assert(is_byte_copy_safe_v<int[4]>);
static_assert(is_byte_copy_safe_v<Flat[2][3]>);
static_assert(is_byte_copy_safe_v<RelocThing>);
static_assert(is_byte_copy_safe_v<HasRelocMember>);

// Regression 2: member array of a relocatable opaque type, consistent
// with the same array at top level.
static_assert(is_byte_copy_safe_v<RelocThing[4]>);
static_assert(is_byte_copy_safe_v<HasRelocArrayMember>,
    "member arrays of relocatable opaque types must be accepted");

// =========================================================================
// Admission predicate: unsafe types
// =========================================================================

// Regression 1: polymorphic types, directly and transitively.
static_assert(!is_byte_copy_safe_v<Poly>,
    "polymorphic types have a vptr, never byte-copy safe");
static_assert(!is_byte_copy_safe_v<HasPolyMember>,
    "a polymorphic member poisons the containing struct");
static_assert(!is_byte_copy_safe_v<DerivedFromPoly>,
    "a polymorphic base poisons the derived type");

// Regression 3: pointer-bearing opaque member sealed from the token scan.
static_assert(!is_byte_copy_safe_v<SealedHandle>);
static_assert(!is_byte_copy_safe_v<SealedCarrier>,
    "a pointer-bearing opaque member poisons the containing struct");

static_assert(!is_byte_copy_safe_v<WithPtr>);
static_assert(!is_byte_copy_safe_v<WithFnPtr>);
static_assert(!is_byte_copy_safe_v<WithMemPtr>);
static_assert(!is_byte_copy_safe_v<WithRef>);
static_assert(!is_byte_copy_safe_v<PtrUnion>);
static_assert(!is_byte_copy_safe_v<int*>);
static_assert(!is_byte_copy_safe_v<void*>);
static_assert(!is_byte_copy_safe_v<int*[4]>);

int main() {
    static constexpr auto flat_sig = get_layout_signature<Flat>();
    std::printf("[PASS] test_core: all static_asserts held at compile time\n");
    std::printf("  sample signature Flat: %s\n", flat_sig.value);
    std::printf("  sample signature Poly: %s (byte_copy_safe = %d)\n",
                poly_sig.value, static_cast<int>(is_byte_copy_safe_v<Poly>));
    return 0;
}
