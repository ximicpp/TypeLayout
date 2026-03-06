// Test: classify<T> -- five-tier safety classification (Tool layer)
//
// Validates the classify<T> struct and classify_v<T> variable template
// against various type categories.  All checks are compile-time
// (static_assert) to verify the consteval classification logic.
//
//
// Requires P2996 (Bloomberg Clang).
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#include <boost/typelayout/tools/classify.hpp>
#include <boost/typelayout/opaque.hpp>
#include <cstdint>
#include <iostream>

using namespace boost::typelayout;

// =========================================================================
// Test types
// =========================================================================

struct TrivialPair {
    int32_t x;
    int32_t y;
};

struct WithPointer {
    int32_t value;
    int32_t* ptr;
};

struct WithFnPtr {
    void (*callback)(int);
};

struct PaddedStruct {
    int8_t  a;
    // 3 bytes padding (alignment of int32_t)
    int32_t b;
};

// -- Opaque type for recursive detection testing
struct OpaqueData {
    char raw[32];
};

// Register OpaqueData as opaque BEFORE any code that inspects it
namespace boost { namespace typelayout {
TYPELAYOUT_OPAQUE_TYPE(OpaqueData, "opaque_data", 32, 1)
}} // namespace boost::typelayout

// -- Struct containing an opaque member (recursive opaque detection)
struct ContainsOpaque {
    int32_t id;
    OpaqueData blob;
};

// -- Struct containing an array of opaque members (P0 fix: array element detection)
struct ArrayOfOpaque {
    int32_t count;
    OpaqueData arr[3];
};

// -- Struct containing an array of padded structs (P1 fix: element padding detection)
struct ArrayOfPadded {
    PaddedStruct items[2];
};

struct WithWchar {
    wchar_t ch;
};

struct Nested {
    TrivialPair pair;
    int32_t     extra;
};

struct NestedWithPointer {
    TrivialPair pair;
    int32_t*    p;
};

struct Empty {};

struct SingleField {
    uint64_t val;
};

// =========================================================================
// 1. Five-tier classify<T> tests (static_assert)
// =========================================================================

// --- TrivialSafe ---
// Fixed-width integers: no pointers, no padding, no platform deps
static_assert(classify_v<int32_t> == SafetyLevel::TrivialSafe,
    "int32_t should be TrivialSafe");
static_assert(classify_v<uint64_t> == SafetyLevel::TrivialSafe,
    "uint64_t should be TrivialSafe");
static_assert(classify_v<float> == SafetyLevel::TrivialSafe,
    "float should be TrivialSafe");
static_assert(classify_v<double> == SafetyLevel::TrivialSafe,
    "double should be TrivialSafe");
static_assert(classify_v<char> == SafetyLevel::TrivialSafe,
    "char should be TrivialSafe");
static_assert(classify_v<bool> == SafetyLevel::TrivialSafe,
    "bool should be TrivialSafe");

// Struct of fixed-width fields, no padding
static_assert(classify_v<TrivialPair> == SafetyLevel::TrivialSafe,
    "TrivialPair (two int32_t) should be TrivialSafe");
static_assert(classify_v<SingleField> == SafetyLevel::TrivialSafe,
    "SingleField (one uint64_t) should be TrivialSafe");

// Nested struct of trivial fields
static_assert(classify_v<Nested> == SafetyLevel::TrivialSafe,
    "Nested (TrivialPair + int32_t) should be TrivialSafe");

// Empty struct
static_assert(classify_v<Empty> == SafetyLevel::TrivialSafe,
    "Empty struct should be TrivialSafe");

// --- PointerRisk (pointer-containing types) ---
// All pointer-like types trigger has_pointer.  Pointers are also
// platform-variant in size (32 vs 64 bit), but the dangling-pointer
// risk from memcpy is more severe and actionable, so PointerRisk
// takes priority over PlatformVariant.

static_assert(classify_v<WithPointer> == SafetyLevel::PointerRisk,
    "WithPointer should be PointerRisk (ptr[ triggers has_pointer)");

static_assert(classify_v<NestedWithPointer> == SafetyLevel::PointerRisk,
    "NestedWithPointer should be PointerRisk (contains ptr[)");

static_assert(classify_v<WithFnPtr> == SafetyLevel::PointerRisk,
    "WithFnPtr should be PointerRisk (fnptr[ triggers has_pointer)");

// --- PaddingRisk ---
// PaddedStruct: int8_t + 3 bytes padding + int32_t = 8 bytes total.
// No pointers, no platform-variant types, but has padding.
static_assert(classify_v<PaddedStruct> == SafetyLevel::PaddingRisk,
    "PaddedStruct should be PaddingRisk (has alignment padding)");

// --- Opaque (recursive detection) ---
static_assert(classify_v<ContainsOpaque> == SafetyLevel::Opaque,
    "ContainsOpaque should be Opaque (contains an opaque member)");

// P0 fix: array of opaque elements must be detected as Opaque.
// type_has_opaque<OpaqueData[3]> must recurse into OpaqueData.
static_assert(classify_v<ArrayOfOpaque> == SafetyLevel::Opaque,
    "ArrayOfOpaque should be Opaque (array element is opaque)");

// P1 fix: array of padded structs must be detected as PaddingRisk.
// compute_has_padding must recurse into array element types.
static_assert(classify_v<ArrayOfPadded> == SafetyLevel::PaddingRisk,
    "ArrayOfPadded should be PaddingRisk (array element has padding)");

// --- PlatformVariant ---
static_assert(classify_v<WithWchar> == SafetyLevel::PlatformVariant,
    "WithWchar should be PlatformVariant (wchar_t varies across platforms)");

static_assert(classify_v<long double> == SafetyLevel::PlatformVariant,
    "long double should be PlatformVariant (f80 varies across platforms)");

// Raw pointer type itself: ptr[ in signature -> has_pointer -> PointerRisk
static_assert(classify_v<int32_t*> == SafetyLevel::PointerRisk,
    "int32_t* should be PointerRisk (pointers cause dangling refs after memcpy)");

// =========================================================================
// 2. Convenience predicates
// =========================================================================

static_assert(is_trivial_safe_v<int32_t>,
    "int32_t should pass is_trivial_safe_v");
static_assert(is_trivial_safe_v<TrivialPair>,
    "TrivialPair should pass is_trivial_safe_v");
static_assert(!is_trivial_safe_v<WithWchar>,
    "WithWchar should fail is_trivial_safe_v");

static_assert(is_memcpy_safe_v<int32_t>,
    "int32_t should pass is_memcpy_safe_v");
static_assert(!is_memcpy_safe_v<WithWchar>,
    "WithWchar should fail is_memcpy_safe_v (platform variant)");

// =========================================================================
// 3. safety_level_name
// =========================================================================

static_assert(
    safety_level_name(SafetyLevel::TrivialSafe)[0] == 'T',
    "safety_level_name(TrivialSafe) should start with 'T'");

// =========================================================================
// Runtime output for human verification
// =========================================================================

template <typename T>
void print_classify(const char* name) {
    std::cout << "  " << name << ": "
              << safety_level_name(classify_v<T>)
              << "\n";
}

int main() {
    std::cout << "=== test_classify ===\n";
    std::cout << "Five-tier classification results:\n";

    print_classify<int32_t>("int32_t");
    print_classify<uint64_t>("uint64_t");
    print_classify<float>("float");
    print_classify<double>("double");
    print_classify<TrivialPair>("TrivialPair");
    print_classify<Nested>("Nested");
    print_classify<Empty>("Empty");
    print_classify<SingleField>("SingleField");
    print_classify<WithPointer>("WithPointer");
    print_classify<WithFnPtr>("WithFnPtr");
    print_classify<WithWchar>("WithWchar");
    print_classify<NestedWithPointer>("NestedWithPointer");
    print_classify<long double>("long double");
    print_classify<int32_t*>("int32_t*");
    print_classify<PaddedStruct>("PaddedStruct");
    print_classify<ContainsOpaque>("ContainsOpaque");
    print_classify<ArrayOfOpaque>("ArrayOfOpaque");
    print_classify<ArrayOfPadded>("ArrayOfPadded");

    std::cout << "\nAll static_assert tests passed at compile time.\n";
    return 0;
}
