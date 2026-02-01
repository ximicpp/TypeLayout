// Boost.TypeLayout
//
// Test Serialization Status - Layer 2 serialization compatibility tests
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/typelayout/typelayout_util.hpp>  // Complete utility layer
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

// Struct with bit-fields - NOT serializable (implementation-defined layout)
struct WithBitFields {
    uint32_t header;
    uint8_t flags : 4;
    uint8_t type : 4;
};

// Nested struct with bit-fields - NOT serializable
struct NestedWithBitFields {
    int32_t id;
    WithBitFields data;
};

// =============================================================================
// Helper Functions
// =============================================================================

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
// Bit-field Tests
// =============================================================================

// Test: Struct with bit-fields should NOT be serializable (implementation-defined layout)
static_assert(!is_serializable_v<WithBitFields, CurrentPlatform>, "WithBitFields should NOT be serializable");
static_assert(serialization_blocker_v<WithBitFields, CurrentPlatform> == SerializationBlocker::HasBitField,
              "WithBitFields blocker should be HasBitField");

// Test: Nested struct with bit-fields should NOT be serializable
static_assert(!is_serializable_v<NestedWithBitFields, CurrentPlatform>, "NestedWithBitFields should NOT be serializable");
static_assert(serialization_blocker_v<NestedWithBitFields, CurrentPlatform> == SerializationBlocker::HasBitField,
              "NestedWithBitFields blocker should be HasBitField");

// Test: Bit-field status string should contain "!serial:bitfield"
constexpr auto bitfield_sig = serialization_status<WithBitFields, CurrentPlatform>();
static_assert(contains_substring(bitfield_sig.c_str(), "!serial:bitfield"),
              "WithBitFields status should contain '!serial:bitfield'");

// =============================================================================
// Platform-Dependent Type Tests
// =============================================================================

// Use platform set by bitwidth + endianness
constexpr auto Platform64LE = PlatformSet::bits64_le();

// Test: is_platform_dependent_size_v detects long
static_assert(is_platform_dependent_size_v<long>, "long has platform-dependent size");
static_assert(is_platform_dependent_size_v<unsigned long>, "unsigned long has platform-dependent size");

// Note: long is ALLOWED in serialization checks to not break int64_t on LP64 systems
// Use is_platform_dependent_size_v for explicit portability warnings

// Test: wchar_t and long double are always rejected
static_assert(is_serializable_v<wchar_t, Platform64LE> == false, "wchar_t should NOT be serializable");
static_assert(is_serializable_v<long double, Platform64LE> == false, "long double should NOT be serializable");
static_assert(serialization_blocker_v<wchar_t, Platform64LE> == SerializationBlocker::HasPlatformDependentSize,
              "wchar_t blocker should be HasPlatformDependentSize");

// =============================================================================
// Serialization Signature String Tests
// =============================================================================

// Test that we can generate status strings
constexpr auto simple_sig = serialization_status<SimpleData, CurrentPlatform>();
constexpr auto ptr_sig = serialization_status<WithPointer, CurrentPlatform>();

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