// Test: classify<T> -- five-tier safety classification (Tool layer)
//
// Validates the classify<T> struct and classify_v<T> variable template
// against various type categories.  All checks are compile-time
// (static_assert) to verify the consteval classification logic.
//
// Also validates backward compatibility: classify_safety<T>() must map
// correctly to the legacy three-tier compat::SafetyLevel.
//
// Requires P2996 (Bloomberg Clang).
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#include <boost/typelayout/tools/classify.hpp>
#include <boost/typelayout/tools/classify_safety.hpp>
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
    // 3 bytes padding
    int32_t b;
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

// --- PlatformVariant (pointer-containing types) ---
// All pointer-like types trigger both has_pointer AND is_platform_variant,
// because pointer size varies by bitness (32-bit vs 64-bit).
// Since PlatformVariant has higher priority than PointerRisk in the
// classification logic, these all resolve to PlatformVariant.

static_assert(classify_v<WithPointer> == SafetyLevel::PlatformVariant,
    "WithPointer should be PlatformVariant (ptr[ triggers is_platform_variant)");

static_assert(classify_v<NestedWithPointer> == SafetyLevel::PlatformVariant,
    "NestedWithPointer should be PlatformVariant (contains ptr[)");

static_assert(classify_v<WithFnPtr> == SafetyLevel::PlatformVariant,
    "WithFnPtr should be PlatformVariant (fnptr[ triggers is_platform_variant)");

// --- PlatformVariant ---
static_assert(classify_v<WithWchar> == SafetyLevel::PlatformVariant,
    "WithWchar should be PlatformVariant (wchar_t varies across platforms)");

static_assert(classify_v<long double> == SafetyLevel::PlatformVariant,
    "long double should be PlatformVariant (f80 varies across platforms)");

// Raw pointer type itself: ptr[ in signature -> is_platform_variant
static_assert(classify_v<int32_t*> == SafetyLevel::PlatformVariant,
    "int32_t* should be PlatformVariant (pointer size is platform-dependent)");

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
// 3. Backward compatibility: classify_safety<T>() (three-tier)
// =========================================================================

// Note: compat::SafetyLevel (3-tier) and boost::typelayout::SafetyLevel (5-tier)
// are distinct enums in different namespaces.
// We use fully-qualified names below to avoid ambiguity.

// Safe mapping
static_assert(compat::classify_safety<int32_t>() == compat::SafetyLevel::Safe,
    "int32_t should map to legacy Safe");
static_assert(compat::classify_safety<TrivialPair>() == compat::SafetyLevel::Safe,
    "TrivialPair should map to legacy Safe");

// Risk mapping (platform-variant -> Risk)
static_assert(compat::classify_safety<long double>() == compat::SafetyLevel::Risk,
    "long double should map to legacy Risk");
static_assert(compat::classify_safety<WithWchar>() == compat::SafetyLevel::Risk,
    "WithWchar should map to legacy Risk");

// Risk mapping (pointer-containing -> PlatformVariant -> Risk)
// All pointer-like types are PlatformVariant (fnptr[, ptr[, etc. all trigger
// is_platform_variant), which maps to legacy Risk.
static_assert(compat::classify_safety<WithFnPtr>() == compat::SafetyLevel::Risk,
    "WithFnPtr should map to legacy Risk (PlatformVariant due to fnptr[)");
static_assert(compat::classify_safety<WithPointer>() == compat::SafetyLevel::Risk,
    "WithPointer should map to legacy Risk (PlatformVariant due to ptr[)");

// =========================================================================
// 4. safety_level_name
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

    std::cout << "\nAll static_assert tests passed at compile time.\n";
    return 0;
}
