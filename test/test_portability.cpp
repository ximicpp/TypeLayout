// test_portability.cpp - Portability detection tests using Boost.Test
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#define BOOST_TEST_MODULE PortabilityTest
#include <boost/test/unit_test.hpp>
#include <boost/typelayout/typelayout.hpp>
#include <cstdint>

using namespace boost::typelayout;

//=============================================================================
// Test Structures for Portability
//=============================================================================

// Fully portable struct (fixed-width types only)
struct PortableStruct {
    int32_t a;
    int64_t b;
    uint16_t c;
};

// Struct with platform-dependent types
struct NonPortableStruct {
    long value;           // Size varies: 4 bytes Windows, 8 bytes Linux
    size_t count;         // Size varies by pointer width
    wchar_t wc;           // Size varies: 2 bytes Windows, 4 bytes Linux
};

// Struct with pointers (platform-dependent size)
struct WithPointers {
    void* ptr;
    int32_t data;
};

// Struct with virtual functions
struct VirtualClass {
    virtual ~VirtualClass() = default;
    virtual void method() {}
    int32_t data;
};

// Struct with long double
struct WithLongDouble {
    long double ld;       // Size varies: 8, 12, or 16 bytes
    int32_t pad;
};

// Nested portable struct
struct NestedPortable {
    PortableStruct inner;
    int32_t outer;
};

// Nested with non-portable member
struct NestedNonPortable {
    PortableStruct portable_part;
    long platform_specific;
};

// Array of portable types
struct PortableArray {
    int32_t data[10];
};

// Array of non-portable types  
struct NonPortableArray {
    long data[10];
};

//=============================================================================
// Portable Type Tests
//=============================================================================

BOOST_AUTO_TEST_SUITE(PortableTypes)

BOOST_AUTO_TEST_CASE(int8_portable)
{
    BOOST_CHECK(!is_platform_dependent_v<int8_t>);
}

BOOST_AUTO_TEST_CASE(int16_portable)
{
    BOOST_CHECK(!is_platform_dependent_v<int16_t>);
}

BOOST_AUTO_TEST_CASE(int32_portable)
{
    BOOST_CHECK(!is_platform_dependent_v<int32_t>);
}

BOOST_AUTO_TEST_CASE(int64_portable)
{
    BOOST_CHECK(!is_platform_dependent_v<int64_t>);
}

BOOST_AUTO_TEST_CASE(uint_types_portable)
{
    BOOST_CHECK(!is_platform_dependent_v<uint8_t>);
    BOOST_CHECK(!is_platform_dependent_v<uint16_t>);
    BOOST_CHECK(!is_platform_dependent_v<uint32_t>);
    BOOST_CHECK(!is_platform_dependent_v<uint64_t>);
}

BOOST_AUTO_TEST_CASE(float_portable)
{
    BOOST_CHECK(!is_platform_dependent_v<float>);
}

BOOST_AUTO_TEST_CASE(double_portable)
{
    BOOST_CHECK(!is_platform_dependent_v<double>);
}

BOOST_AUTO_TEST_CASE(char_types_mostly_portable)
{
    BOOST_CHECK(!is_platform_dependent_v<char>);
    BOOST_CHECK(!is_platform_dependent_v<char8_t>);
    BOOST_CHECK(!is_platform_dependent_v<char16_t>);
    BOOST_CHECK(!is_platform_dependent_v<char32_t>);
}

BOOST_AUTO_TEST_CASE(bool_portable)
{
    BOOST_CHECK(!is_platform_dependent_v<bool>);
}

BOOST_AUTO_TEST_SUITE_END()

//=============================================================================
// Non-Portable Type Tests
//=============================================================================

BOOST_AUTO_TEST_SUITE(NonPortableTypes)

BOOST_AUTO_TEST_CASE(long_non_portable)
{
    // long: 4 bytes on LLP64 (Windows), 8 bytes on LP64 (Linux)
    BOOST_CHECK(is_platform_dependent_v<long>);
}

BOOST_AUTO_TEST_CASE(unsigned_long_non_portable)
{
    BOOST_CHECK(is_platform_dependent_v<unsigned long>);
}

BOOST_AUTO_TEST_CASE(size_t_non_portable)
{
    // size_t: varies with pointer width
    BOOST_CHECK(is_platform_dependent_v<size_t>);
}

BOOST_AUTO_TEST_CASE(ptrdiff_t_non_portable)
{
    BOOST_CHECK(is_platform_dependent_v<ptrdiff_t>);
}

BOOST_AUTO_TEST_CASE(wchar_non_portable)
{
    // wchar_t: 2 bytes Windows, 4 bytes Linux
    BOOST_CHECK(is_platform_dependent_v<wchar_t>);
}

BOOST_AUTO_TEST_CASE(long_double_non_portable)
{
    // long double: 8, 12, or 16 bytes depending on platform
    BOOST_CHECK(is_platform_dependent_v<long double>);
}

BOOST_AUTO_TEST_CASE(pointer_non_portable)
{
    // Pointer size varies with architecture
    BOOST_CHECK(is_platform_dependent_v<void*>);
    BOOST_CHECK(is_platform_dependent_v<int*>);
}

BOOST_AUTO_TEST_SUITE_END()

//=============================================================================
// Struct Portability Tests
//=============================================================================

BOOST_AUTO_TEST_SUITE(StructPortability)

BOOST_AUTO_TEST_CASE(portable_struct_is_portable)
{
    BOOST_CHECK(!is_platform_dependent_v<PortableStruct>);
}

BOOST_AUTO_TEST_CASE(non_portable_struct_detected)
{
    BOOST_CHECK(is_platform_dependent_v<NonPortableStruct>);
}

BOOST_AUTO_TEST_CASE(struct_with_pointers_non_portable)
{
    BOOST_CHECK(is_platform_dependent_v<WithPointers>);
}

BOOST_AUTO_TEST_CASE(virtual_class_non_portable)
{
    // Virtual functions imply vtable, which is ABI-dependent
    BOOST_CHECK(is_platform_dependent_v<VirtualClass>);
}

BOOST_AUTO_TEST_CASE(struct_with_long_double_non_portable)
{
    BOOST_CHECK(is_platform_dependent_v<WithLongDouble>);
}

BOOST_AUTO_TEST_SUITE_END()

//=============================================================================
// Nested Struct Portability Tests
//=============================================================================

BOOST_AUTO_TEST_SUITE(NestedPortability)

BOOST_AUTO_TEST_CASE(nested_portable_is_portable)
{
    // Nesting portable structs remains portable
    BOOST_CHECK(!is_platform_dependent_v<NestedPortable>);
}

BOOST_AUTO_TEST_CASE(nested_with_non_portable_member)
{
    // Any non-portable member makes the whole struct non-portable
    BOOST_CHECK(is_platform_dependent_v<NestedNonPortable>);
}

BOOST_AUTO_TEST_SUITE_END()

//=============================================================================
// Array Portability Tests
//=============================================================================

BOOST_AUTO_TEST_SUITE(ArrayPortability)

BOOST_AUTO_TEST_CASE(portable_array_is_portable)
{
    BOOST_CHECK(!is_platform_dependent_v<PortableArray>);
}

BOOST_AUTO_TEST_CASE(non_portable_array_detected)
{
    BOOST_CHECK(is_platform_dependent_v<NonPortableArray>);
}

BOOST_AUTO_TEST_CASE(array_of_int32_portable)
{
    using ArrayType = int32_t[10];
    BOOST_CHECK(!is_platform_dependent_v<ArrayType>);
}

BOOST_AUTO_TEST_CASE(array_of_pointers_non_portable)
{
    using ArrayType = void*[10];
    BOOST_CHECK(is_platform_dependent_v<ArrayType>);
}

BOOST_AUTO_TEST_SUITE_END()

//=============================================================================
// Architecture Prefix Tests
//=============================================================================

BOOST_AUTO_TEST_SUITE(ArchitecturePrefix)

BOOST_AUTO_TEST_CASE(prefix_contains_bitwidth)
{
    constexpr auto sig = get_layout_signature<int32_t>();
    std::string sig_str = sig.c_str();
    
    // Should contain 64-bit or 32-bit indicator
    BOOST_CHECK(sig_str.find("64") != std::string::npos || 
                sig_str.find("32") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(prefix_contains_endianness)
{
    constexpr auto sig = get_layout_signature<int32_t>();
    std::string sig_str = sig.c_str();
    
    // Should contain endianness indicator
    BOOST_CHECK(sig_str.find("le") != std::string::npos || 
                sig_str.find("be") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(arch_prefix_format)
{
    constexpr auto sig = get_layout_signature<int32_t>();
    std::string sig_str = sig.c_str();
    
    // On typical 64-bit little-endian systems
    BOOST_CHECK(sig_str.find("[64-le]") == 0 || 
                sig_str.find("[64-be]") == 0 ||
                sig_str.find("[32-le]") == 0 ||
                sig_str.find("[32-be]") == 0);
}

BOOST_AUTO_TEST_SUITE_END()

//=============================================================================
// Cross-Platform Comparison Tests
//=============================================================================

BOOST_AUTO_TEST_SUITE(CrossPlatformComparison)

BOOST_AUTO_TEST_CASE(portable_signature_consistent)
{
    // Same portable type should give same signature on same platform
    constexpr auto sig1 = get_layout_signature<PortableStruct>();
    constexpr auto sig2 = get_layout_signature<PortableStruct>();
    BOOST_CHECK_EQUAL(sig1.c_str(), sig2.c_str());
}

BOOST_AUTO_TEST_CASE(signature_includes_size_align)
{
    constexpr auto sig = get_layout_signature<int32_t>();
    std::string sig_str = sig.c_str();
    
    // Should include size and alignment info
    BOOST_CHECK(sig_str.find("s:4") != std::string::npos);
    BOOST_CHECK(sig_str.find("a:4") != std::string::npos);
}

BOOST_AUTO_TEST_SUITE_END()

//=============================================================================
// Type Category Portability Tests
//=============================================================================

BOOST_AUTO_TEST_SUITE(TypeCategoryPortability)

BOOST_AUTO_TEST_CASE(references_non_portable)
{
    // References are implemented as pointers, so size varies
    BOOST_CHECK(is_platform_dependent_v<int&>);
    BOOST_CHECK(is_platform_dependent_v<int&&>);
}

BOOST_AUTO_TEST_CASE(function_pointers_non_portable)
{
    using FnPtr = void(*)();
    BOOST_CHECK(is_platform_dependent_v<FnPtr>);
}

BOOST_AUTO_TEST_CASE(member_pointers_non_portable)
{
    struct Test { int member; };
    using MemPtr = int Test::*;
    BOOST_CHECK(is_platform_dependent_v<MemPtr>);
}

BOOST_AUTO_TEST_SUITE_END()
