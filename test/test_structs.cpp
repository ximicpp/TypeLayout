// test_structs.cpp - Struct layout signature tests using Boost.Test
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#define BOOST_TEST_MODULE StructsTest
#include <boost/test/unit_test.hpp>
#include <boost/typelayout/typelayout.hpp>
#include <cstdint>

using namespace boost::typelayout;

//=============================================================================
// Test Structures
//=============================================================================

// Empty struct
struct Empty {};

// Simple POD structs
struct Point2D {
    float x;
    float y;
};

struct Point3D {
    double x;
    double y;
    double z;
};

// Struct with padding
struct WithPadding {
    char a;      // offset 0
    // 3 bytes padding
    int32_t b;   // offset 4
    char c;      // offset 8
    // 3 bytes padding
};  // size = 12

// Packed-style struct (no padding expected)
struct Packed {
    int32_t a;
    int32_t b;
    int32_t c;
};

// Nested struct
struct Outer {
    int32_t x;
    Point2D inner;
    int32_t y;
};

// Mixed types
struct MixedTypes {
    uint8_t  a;
    uint16_t b;
    uint32_t c;
    uint64_t d;
};

//=============================================================================
// Empty Struct Tests
//=============================================================================

BOOST_AUTO_TEST_SUITE(EmptyStructs)

BOOST_AUTO_TEST_CASE(empty_struct_signature)
{
    // Empty structs have size 1 in C++
    constexpr auto sig = get_layout_signature<Empty>();
    BOOST_CHECK_EQUAL(sizeof(Empty), 1);
    
    // Verify the signature starts with arch prefix
    std::string sig_str = sig.c_str();
    BOOST_CHECK(sig_str.find("[64-le]") == 0);
}

BOOST_AUTO_TEST_CASE(empty_struct_deterministic)
{
    constexpr auto sig1 = get_layout_signature<Empty>();
    constexpr auto sig2 = get_layout_signature<Empty>();
    BOOST_CHECK_EQUAL(sig1.c_str(), sig2.c_str());
}

BOOST_AUTO_TEST_SUITE_END()

//=============================================================================
// Simple Struct Tests
//=============================================================================

BOOST_AUTO_TEST_SUITE(SimpleStructs)

BOOST_AUTO_TEST_CASE(point2d_layout)
{
    BOOST_CHECK_EQUAL(sizeof(Point2D), 8);    // 2 floats
    BOOST_CHECK_EQUAL(alignof(Point2D), 4);   // alignment of float
    
    constexpr auto sig = get_layout_signature<Point2D>();
    std::string sig_str = sig.c_str();
    
    // Signature should contain field information
    BOOST_CHECK(sig_str.find("x") != std::string::npos);
    BOOST_CHECK(sig_str.find("y") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(point3d_layout)
{
    BOOST_CHECK_EQUAL(sizeof(Point3D), 24);   // 3 doubles
    BOOST_CHECK_EQUAL(alignof(Point3D), 8);   // alignment of double
    
    constexpr auto sig = get_layout_signature<Point3D>();
    std::string sig_str = sig.c_str();
    
    BOOST_CHECK(sig_str.find("x") != std::string::npos);
    BOOST_CHECK(sig_str.find("y") != std::string::npos);
    BOOST_CHECK(sig_str.find("z") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(packed_no_padding)
{
    BOOST_CHECK_EQUAL(sizeof(Packed), 12);    // 3 int32
    BOOST_CHECK_EQUAL(alignof(Packed), 4);
    
    constexpr auto sig = get_layout_signature<Packed>();
    std::string sig_str = sig.c_str();
    
    // No padding expected in signature
    BOOST_CHECK(sig_str.find("pad") == std::string::npos || 
                sig_str.find("pad:0") != std::string::npos);
}

BOOST_AUTO_TEST_SUITE_END()

//=============================================================================
// Padding Tests
//=============================================================================

BOOST_AUTO_TEST_SUITE(PaddingDetection)

BOOST_AUTO_TEST_CASE(with_padding_layout)
{
    // Expected layout: char(1) + pad(3) + int(4) + char(1) + pad(3) = 12
    BOOST_CHECK_EQUAL(sizeof(WithPadding), 12);
    
    constexpr auto sig = get_layout_signature<WithPadding>();
    std::string sig_str = sig.c_str();
    
    // Signature should reflect padding
    BOOST_CHECK(sig_str.find("a") != std::string::npos);
    BOOST_CHECK(sig_str.find("b") != std::string::npos);
    BOOST_CHECK(sig_str.find("c") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(mixed_types_alignment)
{
    // u8(1) + pad(1) + u16(2) + u32(4) + u64(8) = 16
    BOOST_CHECK_EQUAL(sizeof(MixedTypes), 16);
    BOOST_CHECK_EQUAL(alignof(MixedTypes), 8);
    
    constexpr auto sig = get_layout_signature<MixedTypes>();
    std::string sig_str = sig.c_str();
    
    BOOST_CHECK(sig_str.find("a") != std::string::npos);
    BOOST_CHECK(sig_str.find("d") != std::string::npos);
}

BOOST_AUTO_TEST_SUITE_END()

//=============================================================================
// Nested Struct Tests
//=============================================================================

BOOST_AUTO_TEST_SUITE(NestedStructs)

BOOST_AUTO_TEST_CASE(outer_contains_inner)
{
    constexpr auto sig = get_layout_signature<Outer>();
    std::string sig_str = sig.c_str();
    
    // Should contain member names
    BOOST_CHECK(sig_str.find("x") != std::string::npos);
    BOOST_CHECK(sig_str.find("inner") != std::string::npos);
    BOOST_CHECK(sig_str.find("y") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(nested_struct_size)
{
    // int32(4) + Point2D(8) + int32(4) = 16
    BOOST_CHECK_EQUAL(sizeof(Outer), 16);
    BOOST_CHECK_EQUAL(alignof(Outer), 4);
}

BOOST_AUTO_TEST_SUITE_END()

//=============================================================================
// Determinism Tests
//=============================================================================

BOOST_AUTO_TEST_SUITE(Determinism)

BOOST_AUTO_TEST_CASE(same_type_same_signature)
{
    constexpr auto sig1 = get_layout_signature<Point2D>();
    constexpr auto sig2 = get_layout_signature<Point2D>();
    BOOST_CHECK_EQUAL(sig1.c_str(), sig2.c_str());
}

BOOST_AUTO_TEST_CASE(different_types_different_signatures)
{
    constexpr auto sig2d = get_layout_signature<Point2D>();
    constexpr auto sig3d = get_layout_signature<Point3D>();
    BOOST_CHECK(std::string(sig2d.c_str()) != std::string(sig3d.c_str()));
}

BOOST_AUTO_TEST_SUITE_END()

//=============================================================================
// Array Members Tests
//=============================================================================

struct WithArray {
    int32_t data[4];
};

struct WithMultiDimArray {
    int32_t matrix[2][3];
};

BOOST_AUTO_TEST_SUITE(ArrayMembers)

BOOST_AUTO_TEST_CASE(simple_array_member)
{
    BOOST_CHECK_EQUAL(sizeof(WithArray), 16);  // 4 * 4 bytes
    
    constexpr auto sig = get_layout_signature<WithArray>();
    std::string sig_str = sig.c_str();
    
    BOOST_CHECK(sig_str.find("data") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(multidim_array_member)
{
    BOOST_CHECK_EQUAL(sizeof(WithMultiDimArray), 24);  // 2 * 3 * 4 bytes
    
    constexpr auto sig = get_layout_signature<WithMultiDimArray>();
    std::string sig_str = sig.c_str();
    
    BOOST_CHECK(sig_str.find("matrix") != std::string::npos);
}

BOOST_AUTO_TEST_SUITE_END()

//=============================================================================
// Pointer Members Tests
//=============================================================================

struct WithPointers {
    int32_t* ptr;
    const char* str;
    void* data;
};

BOOST_AUTO_TEST_SUITE(PointerMembers)

BOOST_AUTO_TEST_CASE(pointer_members_layout)
{
    BOOST_CHECK_EQUAL(sizeof(WithPointers), 24);  // 3 pointers on 64-bit
    
    constexpr auto sig = get_layout_signature<WithPointers>();
    std::string sig_str = sig.c_str();
    
    BOOST_CHECK(sig_str.find("ptr") != std::string::npos);
    BOOST_CHECK(sig_str.find("str") != std::string::npos);
    BOOST_CHECK(sig_str.find("data") != std::string::npos);
}

BOOST_AUTO_TEST_SUITE_END()
