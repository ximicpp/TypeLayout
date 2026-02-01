// Boost.TypeLayout
//
// Test Serialization Signature - Layer 2 serialization compatibility tests
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/typelayout/detail/serialization_traits.hpp>
#include <boost/typelayout/detail/serialization_signature.hpp>
#include <type_traits>
#include <cstdint>
#include <string>
#include <cstring>

// Use the current platform for testing
using namespace boost::typelayout;
constexpr auto CurrentPlatform = PlatformSet::current();

// =============================================================================
// Test Types
// =============================================================================

// Simple POD struct - should be serializable
struct SimpleData {
    int32_t id;
    float value;
    char name[16];
};

// Struct with pointer - NOT serializable
struct WithPointer {
    int32_t id;
    void* ptr;
};

// Struct with reference - NOT serializable (also not trivially copyable)
struct WithReference {
    int& ref;
};

// Polymorphic type - NOT serializable (not trivially copyable due to vptr)
struct PolymorphicType {
    virtual ~PolymorphicType() = default;
    int data;
};

// Non-trivially-copyable type for separate testing
struct NonTrivial {
    NonTrivial() = default;
    NonTrivial(const NonTrivial&) {}  // Non-trivial copy
    int data;
};

// Nested struct with pointer - NOT serializable
struct NestedWithPointer {
    int32_t header;
    WithPointer nested;
};

// Struct with platform-dependent size - NOT serializable for cross-platform
struct WithLong {
    int32_t a;
    long b;  // Different sizes on Windows vs Linux
};

// Nested POD struct - should be serializable
struct NestedPOD {
    int32_t header;
    SimpleData data;
};

// Struct with std::string - NOT serializable (not trivially copyable)
struct WithString {
    int32_t id;
    std::string name;
};

// =============================================================================
// Basic Serialization Trait Tests
// =============================================================================

// Test: Fundamental types should be serializable
static_assert(is_serializable_v<int32_t, CurrentPlatform>, "int32_t should be serializable");
static_assert(is_serializable_v<float, CurrentPlatform>, "float should be serializable");
static_assert(is_serializable_v<double, CurrentPlatform>, "double should be serializable");
static_assert(is_serializable_v<char, CurrentPlatform>, "char should be serializable");
static_assert(is_serializable_v<uint64_t, CurrentPlatform>, "uint64_t should be serializable");

// Test: Pointer types should NOT be serializable  
static_assert(!is_serializable_v<int*, CurrentPlatform>, "int* should NOT be serializable");
static_assert(!is_serializable_v<void*, CurrentPlatform>, "void* should NOT be serializable");
static_assert(serialization_blocker_v<void*, CurrentPlatform> == SerializationBlocker::HasPointer,
              "void* blocker should be HasPointer");

// Test: Simple POD should be serializable
static_assert(is_serializable_v<SimpleData, CurrentPlatform>, "SimpleData should be serializable");

// Test: Struct with pointer should NOT be serializable
static_assert(!is_serializable_v<WithPointer, CurrentPlatform>, "WithPointer should NOT be serializable");
static_assert(serialization_blocker_v<WithPointer, CurrentPlatform> == SerializationBlocker::HasPointer,
              "WithPointer blocker should be HasPointer");

// Test: Polymorphic types should NOT be serializable
// Note: Polymorphic types are first detected as NotTriviallyCopyable because of vptr
static_assert(!is_serializable_v<PolymorphicType, CurrentPlatform>, "PolymorphicType should NOT be serializable");
static_assert(serialization_blocker_v<PolymorphicType, CurrentPlatform> == SerializationBlocker::NotTriviallyCopyable,
              "PolymorphicType blocker should be NotTriviallyCopyable (due to vptr)");

// Test: Non-trivially-copyable types should NOT be serializable  
static_assert(!is_serializable_v<NonTrivial, CurrentPlatform>, "NonTrivial should NOT be serializable");
static_assert(serialization_blocker_v<NonTrivial, CurrentPlatform> == SerializationBlocker::NotTriviallyCopyable,
              "NonTrivial blocker should be NotTriviallyCopyable");

// Test: Nested POD should be serializable
static_assert(is_serializable_v<NestedPOD, CurrentPlatform>, "NestedPOD should be serializable");

// Test: Nested struct with pointer should NOT be serializable
static_assert(!is_serializable_v<NestedWithPointer, CurrentPlatform>, "NestedWithPointer should NOT be serializable");

// Test: std::string is NOT serializable (not trivially copyable)
static_assert(!is_serializable_v<WithString, CurrentPlatform>, "WithString should NOT be serializable");

// =============================================================================
// Platform-Dependent Type Tests
// =============================================================================

// Create a strict platform set that rejects long
constexpr auto StrictPlatform = PlatformSet::x64_le();

// Test: long should fail with strict platform set
static_assert(is_platform_dependent_size_v<long>, "long has platform-dependent size");
static_assert(is_platform_dependent_size_v<unsigned long>, "unsigned long has platform-dependent size");

// Note: We can't directly test StrictPlatform rejection since it may not match
// current platform. Instead, test the basic trait.
static_assert(basic_serialization_check<long, StrictPlatform>::value == SerializationBlocker::HasPlatformDependentSize ||
              basic_serialization_check<long, StrictPlatform>::value == SerializationBlocker::PlatformMismatch,
              "long should fail with strict platform");

// =============================================================================
// Serialization Signature String Tests
// =============================================================================

// Test that we can generate signature strings
constexpr auto simple_sig = serialization_signature<SimpleData, CurrentPlatform>();
constexpr auto ptr_sig = serialization_signature<WithPointer, CurrentPlatform>();

// Helper to check if a substring exists in c_str
consteval bool contains_substring(const char* str, const char* substr) {
    if (!str || !substr) return false;
    const char* p = str;
    while (*p) {
        const char* s1 = p;
        const char* s2 = substr;
        while (*s1 && *s2 && *s1 == *s2) {
            s1++;
            s2++;
        }
        if (*s2 == '\0') return true;
        p++;
    }
    return false;
}

// The simple data should have "[NN-le]serial" format
static_assert(contains_substring(simple_sig.c_str(), "serial"),
              "SimpleData signature should contain 'serial'");
static_assert(simple_sig.c_str()[0] == '[', "Signature should start with platform prefix");

// Pointer type should have "!serial:ptr" 
static_assert(contains_substring(ptr_sig.c_str(), "!serial:ptr"),
              "WithPointer signature should contain '!serial:ptr'");

// =============================================================================
// Compatibility Check Tests
// =============================================================================

// Identical types should be compatible if serializable
static_assert(check_serialization_compatible<SimpleData, SimpleData, CurrentPlatform>(),
              "Identical serializable types should be compatible");

// Non-serializable types should fail compatibility
static_assert(!check_serialization_compatible<WithPointer, WithPointer, CurrentPlatform>(),
              "Non-serializable types should not be compatible");

// =============================================================================
// Main - Run all tests
// =============================================================================

int main() {
    // All tests are compile-time static_asserts
    // If we get here, all tests passed!
    return 0;
}