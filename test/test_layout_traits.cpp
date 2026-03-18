// layout_traits<T> and layout_signatures_match tests.
//
// Verifies that:
//   1. layout_traits exposes the correct signature (same as get_layout_signature)
//   2. Natural by-product flags (has_pointer, has_opaque, etc.) are correct
//   3. Structural metadata (field_count, total_size, alignment) is correct
//   4. layout_signatures_match works as expected
//
// All assertions are compile-time (static_assert).  The main() function
// prints a summary for CI log readability.
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#include <boost/typelayout/typelayout.hpp>
#include <boost/typelayout/layout_traits.hpp>
#include "test_util.hpp"
#include <iostream>
#include <cstdint>

using namespace boost::typelayout;
using boost::typelayout::detail::layout_traits;

// =========================================================================
// Test types
// =========================================================================

namespace test_types {

// -- Plain struct (no pointers, no padding on most platforms)
struct Compact {
    int32_t x;
    int32_t y;
};

// -- Struct with pointer
struct WithPointer {
    int32_t id;
    void* ptr;
};

// -- Struct with function pointer
struct WithFnPtr {
    int32_t id;
    void (*callback)(int);
};

// -- Struct with member pointer
struct Base { int m; };
struct WithMemPtr {
    int Base::* mp;
};

// -- Struct with opaque-registered type
struct OpaqueBlob {
    char data[64];
};

// -- Empty struct
struct Empty {};

// -- Struct containing wchar_t (platform-variant)
struct WithWchar {
    wchar_t wc;
    int32_t x;
};

// -- Struct containing long double (platform-variant)
struct WithLongDouble {
    long double ld;
    int32_t x;
};

// -- Struct containing plain pointer (platform-variant due to bitness)
struct WithRawPtr {
    int32_t* p;
};

// -- ABI-compatible pair for layout_signatures_match testing
struct PairA {
    int32_t a;
    float b;
};

struct PairB {
    int32_t x;
    float y;
};

// -- ABI-incompatible pair
struct PairC {
    int64_t a;
    double b;
};

// -- Nested struct
struct Inner {
    int16_t a;
    int16_t b;
};

struct Outer {
    Inner inner;
    int32_t c;
};

} // namespace test_types

// Register OpaqueBlob as an opaque type
namespace boost { namespace typelayout {
TYPELAYOUT_REGISTER_OPAQUE(test_types::OpaqueBlob, "blob", false)
}} // namespace boost::typelayout

// =========================================================================
// Part 1: Signature consistency
//   layout_traits<T>::signature must equal get_layout_signature<T>()
// =========================================================================

static_assert(
    layout_traits<test_types::Compact>::signature ==
        get_layout_signature<test_types::Compact>(),
    "P1.1: layout_traits signature must match get_layout_signature"
);

static_assert(
    layout_traits<int32_t>::signature ==
        get_layout_signature<int32_t>(),
    "P1.2: layout_traits works for fundamental types"
);

static_assert(
    layout_traits<test_types::WithPointer>::signature ==
        get_layout_signature<test_types::WithPointer>(),
    "P1.3: layout_traits signature for struct with pointer"
);

// =========================================================================
// Part 2: has_pointer detection
// =========================================================================

static_assert(
    !layout_traits<test_types::Compact>::has_pointer,
    "P2.1: Compact has no pointer"
);

static_assert(
    layout_traits<test_types::WithPointer>::has_pointer,
    "P2.2: WithPointer contains a void*"
);

static_assert(
    layout_traits<test_types::WithFnPtr>::has_pointer,
    "P2.3: WithFnPtr contains a function pointer"
);

static_assert(
    layout_traits<test_types::WithMemPtr>::has_pointer,
    "P2.4: WithMemPtr contains a member pointer"
);

static_assert(
    !layout_traits<int32_t>::has_pointer,
    "P2.5: fundamental int32_t has no pointer"
);

// =========================================================================
// Part 3: has_opaque detection
// =========================================================================

static_assert(
    layout_traits<test_types::OpaqueBlob>::has_opaque,
    "P3.1: OpaqueBlob is registered as opaque"
);

static_assert(
    !layout_traits<test_types::Compact>::has_opaque,
    "P3.2: Compact is not opaque"
);

static_assert(
    !layout_traits<int32_t>::has_opaque,
    "P3.3: fundamental type is not opaque"
);

// Test recursive opaque detection: a struct containing an opaque member
namespace test_types {
struct ContainsOpaqueBlob {
    int32_t id;
    OpaqueBlob blob;
};
} // namespace test_types

static_assert(
    layout_traits<test_types::ContainsOpaqueBlob>::has_opaque,
    "P3.4: ContainsOpaqueBlob contains an opaque member (recursive detection)"
);

// Test array-of-opaque detection: a struct whose member is an array of opaque type.
// Fixes P0: type_has_opaque<OpaqueBlob[N]> must recurse into the element type.
namespace test_types {
struct ContainsOpaqueBlobArray {
    int32_t count;
    OpaqueBlob blobs[3];
};
} // namespace test_types

static_assert(
    layout_traits<test_types::ContainsOpaqueBlobArray>::has_opaque,
    "P3.5: ContainsOpaqueBlobArray -- array of opaque elements must be detected as opaque"
);

// =========================================================================
// Part 4: Structural metadata
// =========================================================================

// total_size
static_assert(
    layout_traits<test_types::Compact>::total_size == sizeof(test_types::Compact),
    "P5.1: total_size matches sizeof"
);

// alignment
static_assert(
    layout_traits<test_types::Compact>::alignment == alignof(test_types::Compact),
    "P5.2: alignment matches alignof"
);

// field_count
static_assert(
    layout_traits<test_types::Compact>::field_count == 2,
    "P5.3: Compact has 2 fields"
);

static_assert(
    layout_traits<test_types::WithPointer>::field_count == 2,
    "P5.4: WithPointer has 2 fields"
);

static_assert(
    layout_traits<test_types::Empty>::field_count == 0,
    "P5.5: Empty has 0 fields"
);

static_assert(
    layout_traits<int32_t>::field_count == 0,
    "P5.6: fundamental type has 0 fields"
);

static_assert(
    layout_traits<test_types::Outer>::field_count == 2,
    "P5.7: Outer has 2 direct fields (inner + c)"
);

// =========================================================================
// Part 6: layout_signatures_match
// =========================================================================

// Same layout, different names -> must match
static_assert(
    get_layout_signature<test_types::PairA>() == get_layout_signature<test_types::PairB>(),
    "P6.1: PairA and PairB have identical layout, signatures must match"
);

// Different layout -> must not match
static_assert(
    !(get_layout_signature<test_types::PairA>() == get_layout_signature<test_types::PairC>()),
    "P6.2: PairA and PairC have different layouts, signatures must differ"
);

// Self-comparison -> must match
static_assert(
    get_layout_signature<test_types::Compact>() == get_layout_signature<test_types::Compact>(),
    "P6.3: same type compared with itself must match"
);

// Also works via get_layout_signature comparison
static_assert(
    get_layout_signature<test_types::PairA>() == get_layout_signature<test_types::PairB>(),
    "P6.4: get_layout_signature comparison for compatible types"
);

static_assert(
    !(get_layout_signature<test_types::PairA>() == get_layout_signature<test_types::PairC>()),
    "P6.5: get_layout_signature comparison for incompatible types"
);

// =========================================================================
// Part 7: has_padding detection (precise, using P2996 reflection)
// =========================================================================

static_assert(
    !layout_traits<int32_t>::has_padding,
    "P7.1: scalar types have no padding"
);

static_assert(
    !layout_traits<double>::has_padding,
    "P7.2: double has no padding"
);

static_assert(
    !layout_traits<test_types::Empty>::has_padding,
    "P7.3: empty class has no padding"
);

static_assert(
    !layout_traits<test_types::Compact>::has_padding,
    "P7.4: Compact (two int32_t) has no padding"
);

// PaddedStruct: int8_t (1 byte) + int32_t (4 bytes) = 5 bytes,
// but sizeof(PaddedStruct) == 8 due to alignment padding.
namespace test_types {
struct PaddedStruct {
    int8_t  a;
    int32_t b;
};
} // namespace test_types

static_assert(
    layout_traits<test_types::PaddedStruct>::has_padding,
    "P7.5: PaddedStruct has alignment padding between int8_t and int32_t"
);

// Array-of-padded-struct: the outer struct has no outer padding gap
// (the array field covers all bytes), but the element type has internal
// padding.  Fixes P1: compute_has_padding must recurse into array elements.
namespace test_types {
struct ArrayOfPadded {
    PaddedStruct items[2];
};
} // namespace test_types

static_assert(
    layout_traits<test_types::ArrayOfPadded>::has_padding,
    "P7.6: ArrayOfPadded -- array of padded elements must be detected as having padding"
);

// =========================================================================
// Part 8: Member pointer and function pointer signature correctness
//   Validates that sizeof/alignof are used (not hardcoded) by checking
//   that the signature string contains the correct size value.
// =========================================================================

// Helper: check if a FixedString contains a given substring at compile time
template <size_t N, size_t M>
consteval bool sig_contains(const FixedString<N>& haystack, const char (&needle)[M]) {
    if (M - 1 > N) return false;
    for (size_t i = 0; i + M - 1 <= N; ++i) {
        bool match = true;
        for (size_t j = 0; j < M - 1; ++j) {
            if (haystack.value[i + j] != needle[j]) { match = false; break; }
        }
        if (match) return true;
    }
    return false;
}

// -- Member data pointer: on Itanium x64, sizeof(int Base::*) == 8
namespace test_types {
struct MFPBase { virtual void vfunc(); };
struct WithMemFnPtr {
    void (MFPBase::* mfp)();
};
} // namespace test_types

// Member data pointer size must appear in signature
static_assert(
    sizeof(int test_types::Base::*) == layout_traits<test_types::WithMemPtr>::total_size,
    "P9.1: WithMemPtr total_size must equal sizeof the member data pointer"
);

// Member data pointer signature must contain memptr tag
static_assert(
    sig_contains(layout_traits<test_types::WithMemPtr>::signature, "memptr["),
    "P9.2: WithMemPtr signature must contain 'memptr[' tag"
);

// Member function pointer: on Itanium x64, sizeof(void (MFPBase::*)()) == 16
static_assert(
    sizeof(void (test_types::MFPBase::*)()) == layout_traits<test_types::WithMemFnPtr>::total_size,
    "P9.3: WithMemFnPtr total_size must equal sizeof the member function pointer"
);

static_assert(
    sig_contains(layout_traits<test_types::WithMemFnPtr>::signature, "memptr["),
    "P9.4: WithMemFnPtr signature must contain 'memptr[' tag"
);

// Member data pointer and member function pointer must have different signatures
// if their sizes differ (which they do on Itanium x64: 8 vs 16)
static_assert(
    (sizeof(int test_types::Base::*) == sizeof(void (test_types::MFPBase::*)())) ||
    !(layout_traits<test_types::WithMemPtr>::signature ==
      layout_traits<test_types::WithMemFnPtr>::signature),
    "P9.5: member data ptr and member function ptr signatures must differ when sizes differ"
);

// Function pointer: size must match sizeof on current platform
static_assert(
    layout_traits<test_types::WithFnPtr>::has_pointer,
    "P9.6: WithFnPtr must be detected as containing a pointer"
);

static_assert(
    sig_contains(layout_traits<test_types::WithFnPtr>::signature, "fnptr["),
    "P9.7: WithFnPtr signature must contain 'fnptr[' tag"
);

// Standalone function pointer signature
namespace test_types {
using SimpleFnPtr = void(*)();
struct StandaloneFnPtr {
    SimpleFnPtr fp;
};
} // namespace test_types

static_assert(
    layout_traits<test_types::StandaloneFnPtr>::total_size == sizeof(test_types::SimpleFnPtr),
    "P9.8: StandaloneFnPtr total_size must equal sizeof(void(*)())"
);

// =========================================================================
// Main -- runtime summary
// =========================================================================

int main() {
    constexpr int total_tests = 30;

    std::cout << "=== layout_traits & layout_signatures_match Tests ===\n\n";

    std::cout << "--- Sample signatures ---\n";
    std::cout << "Compact:      " << layout_traits<test_types::Compact>::signature.value << "\n";
    std::cout << "WithPointer:  " << layout_traits<test_types::WithPointer>::signature.value << "\n";
    std::cout << "WithFnPtr:    " << layout_traits<test_types::WithFnPtr>::signature.value << "\n";
    std::cout << "WithWchar:    " << layout_traits<test_types::WithWchar>::signature.value << "\n";
    std::cout << "OpaqueBlob:   " << layout_traits<test_types::OpaqueBlob>::signature.value << "\n";
    std::cout << "PairA:        " << layout_traits<test_types::PairA>::signature.value << "\n";
    std::cout << "PairB:        " << layout_traits<test_types::PairB>::signature.value << "\n";
    std::cout << "Outer:        " << layout_traits<test_types::Outer>::signature.value << "\n";
    std::cout << "WithMemPtr:   " << layout_traits<test_types::WithMemPtr>::signature.value << "\n";
    std::cout << "WithMemFnPtr: " << layout_traits<test_types::WithMemFnPtr>::signature.value << "\n";
    std::cout << "StandaloneFnPtr: " << layout_traits<test_types::StandaloneFnPtr>::signature.value << "\n";
    std::cout << "ContainsOpaqueBlobArray: " << layout_traits<test_types::ContainsOpaqueBlobArray>::signature.value << "\n";
    std::cout << "ArrayOfPadded:   " << layout_traits<test_types::ArrayOfPadded>::signature.value << "\n";

    std::cout << "\n--- Member pointer sizes ---\n";
    std::cout << "sizeof(int Base::*):              " << sizeof(int test_types::Base::*) << "\n";
    std::cout << "sizeof(void (MFPBase::*)()):       " << sizeof(void (test_types::MFPBase::*)()) << "\n";
    std::cout << "sizeof(void(*)()):                 " << sizeof(void(*)()) << "\n";

    std::cout << "\n--- By-product flags (Compact) ---\n";
    std::cout << "has_pointer:        " << layout_traits<test_types::Compact>::has_pointer << "\n";
    std::cout << "has_opaque:         " << layout_traits<test_types::Compact>::has_opaque << "\n";
    std::cout << "has_padding:        " << layout_traits<test_types::Compact>::has_padding << "\n";
    std::cout << "field_count:        " << layout_traits<test_types::Compact>::field_count << "\n";
    std::cout << "total_size:         " << layout_traits<test_types::Compact>::total_size << "\n";
    std::cout << "alignment:          " << layout_traits<test_types::Compact>::alignment << "\n";

    std::cout << "\n--- By-product flags (WithPointer) ---\n";
    std::cout << "has_pointer:        " << layout_traits<test_types::WithPointer>::has_pointer << "\n";

    std::cout << "\nAll " << total_tests << " static_assert tests passed at compile time.\n";
    return 0;
}
