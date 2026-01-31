// test_primitives.cpp - Primitive type signature tests using Boost.Test
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#define BOOST_TEST_MODULE PrimitivesTest
#include <boost/test/unit_test.hpp>
#include <boost/typelayout/typelayout.hpp>
#include <string>

using namespace boost::typelayout;

//=============================================================================
// Integer Types
//=============================================================================

BOOST_AUTO_TEST_SUITE(IntegerTypes)

BOOST_AUTO_TEST_CASE(int8_signature)
{
    constexpr auto sig = get_layout_signature<int8_t>();
    BOOST_CHECK_EQUAL(sig.c_str(), "[64-le]i8[s:1,a:1]");
    BOOST_CHECK_EQUAL(sizeof(int8_t), 1);
    BOOST_CHECK_EQUAL(alignof(int8_t), 1);
}

BOOST_AUTO_TEST_CASE(uint8_signature)
{
    constexpr auto sig = get_layout_signature<uint8_t>();
    BOOST_CHECK_EQUAL(sig.c_str(), "[64-le]u8[s:1,a:1]");
}

BOOST_AUTO_TEST_CASE(int16_signature)
{
    constexpr auto sig = get_layout_signature<int16_t>();
    BOOST_CHECK_EQUAL(sig.c_str(), "[64-le]i16[s:2,a:2]");
    BOOST_CHECK_EQUAL(sizeof(int16_t), 2);
}

BOOST_AUTO_TEST_CASE(uint16_signature)
{
    constexpr auto sig = get_layout_signature<uint16_t>();
    BOOST_CHECK_EQUAL(sig.c_str(), "[64-le]u16[s:2,a:2]");
}

BOOST_AUTO_TEST_CASE(int32_signature)
{
    constexpr auto sig = get_layout_signature<int32_t>();
    BOOST_CHECK_EQUAL(sig.c_str(), "[64-le]i32[s:4,a:4]");
    BOOST_CHECK_EQUAL(sizeof(int32_t), 4);
}

BOOST_AUTO_TEST_CASE(uint32_signature)
{
    constexpr auto sig = get_layout_signature<uint32_t>();
    BOOST_CHECK_EQUAL(sig.c_str(), "[64-le]u32[s:4,a:4]");
}

BOOST_AUTO_TEST_CASE(int64_signature)
{
    constexpr auto sig = get_layout_signature<int64_t>();
    BOOST_CHECK_EQUAL(sig.c_str(), "[64-le]i64[s:8,a:8]");
    BOOST_CHECK_EQUAL(sizeof(int64_t), 8);
}

BOOST_AUTO_TEST_CASE(uint64_signature)
{
    constexpr auto sig = get_layout_signature<uint64_t>();
    BOOST_CHECK_EQUAL(sig.c_str(), "[64-le]u64[s:8,a:8]");
}

BOOST_AUTO_TEST_SUITE_END()

//=============================================================================
// Floating Point Types
//=============================================================================

BOOST_AUTO_TEST_SUITE(FloatingPointTypes)

BOOST_AUTO_TEST_CASE(float_signature)
{
    constexpr auto sig = get_layout_signature<float>();
    BOOST_CHECK_EQUAL(sig.c_str(), "[64-le]f32[s:4,a:4]");
    BOOST_CHECK_EQUAL(sizeof(float), 4);
}

BOOST_AUTO_TEST_CASE(double_signature)
{
    constexpr auto sig = get_layout_signature<double>();
    BOOST_CHECK_EQUAL(sig.c_str(), "[64-le]f64[s:8,a:8]");
    BOOST_CHECK_EQUAL(sizeof(double), 8);
}

BOOST_AUTO_TEST_CASE(long_double_platform_dependent)
{
    // long double size varies by platform
    BOOST_CHECK_GE(sizeof(long double), 8);
    BOOST_CHECK(is_platform_dependent_v<long double>);
}

BOOST_AUTO_TEST_SUITE_END()

//=============================================================================
// Character Types
//=============================================================================

BOOST_AUTO_TEST_SUITE(CharacterTypes)

BOOST_AUTO_TEST_CASE(char_signature)
{
    constexpr auto sig = get_layout_signature<char>();
    BOOST_CHECK_EQUAL(sig.c_str(), "[64-le]char[s:1,a:1]");
}

BOOST_AUTO_TEST_CASE(char8_signature)
{
    constexpr auto sig = get_layout_signature<char8_t>();
    BOOST_CHECK_EQUAL(sig.c_str(), "[64-le]char8[s:1,a:1]");
}

BOOST_AUTO_TEST_CASE(char16_signature)
{
    constexpr auto sig = get_layout_signature<char16_t>();
    BOOST_CHECK_EQUAL(sig.c_str(), "[64-le]char16[s:2,a:2]");
}

BOOST_AUTO_TEST_CASE(char32_signature)
{
    constexpr auto sig = get_layout_signature<char32_t>();
    BOOST_CHECK_EQUAL(sig.c_str(), "[64-le]char32[s:4,a:4]");
}

BOOST_AUTO_TEST_CASE(wchar_platform_dependent)
{
    // wchar_t: 2 bytes on Windows, 4 bytes on Linux
    BOOST_CHECK(sizeof(wchar_t) == 2 || sizeof(wchar_t) == 4);
    BOOST_CHECK(is_platform_dependent_v<wchar_t>);
}

BOOST_AUTO_TEST_SUITE_END()

//=============================================================================
// Boolean and Special Types
//=============================================================================

BOOST_AUTO_TEST_SUITE(SpecialTypes)

BOOST_AUTO_TEST_CASE(bool_signature)
{
    constexpr auto sig = get_layout_signature<bool>();
    BOOST_CHECK_EQUAL(sig.c_str(), "[64-le]bool[s:1,a:1]");
    BOOST_CHECK_EQUAL(sizeof(bool), 1);
}

BOOST_AUTO_TEST_CASE(nullptr_signature)
{
    constexpr auto sig = get_layout_signature<std::nullptr_t>();
    BOOST_CHECK_EQUAL(sig.c_str(), "[64-le]nullptr[s:8,a:8]");
}

BOOST_AUTO_TEST_CASE(byte_signature)
{
    constexpr auto sig = get_layout_signature<std::byte>();
    BOOST_CHECK_EQUAL(sig.c_str(), "[64-le]byte[s:1,a:1]");
}

BOOST_AUTO_TEST_SUITE_END()

//=============================================================================
// Pointer Types
//=============================================================================

BOOST_AUTO_TEST_SUITE(PointerTypes)

BOOST_AUTO_TEST_CASE(void_ptr_signature)
{
    constexpr auto sig = get_layout_signature<void*>();
    BOOST_CHECK_EQUAL(sig.c_str(), "[64-le]ptr[s:8,a:8]");
}

BOOST_AUTO_TEST_CASE(int_ptr_signature)
{
    constexpr auto sig = get_layout_signature<int*>();
    BOOST_CHECK_EQUAL(sig.c_str(), "[64-le]ptr[s:8,a:8]");
}

BOOST_AUTO_TEST_CASE(const_char_ptr_signature)
{
    constexpr auto sig = get_layout_signature<const char*>();
    BOOST_CHECK_EQUAL(sig.c_str(), "[64-le]ptr[s:8,a:8]");
}

BOOST_AUTO_TEST_SUITE_END()

//=============================================================================
// Reference Types
//=============================================================================

BOOST_AUTO_TEST_SUITE(ReferenceTypes)

BOOST_AUTO_TEST_CASE(lvalue_ref_signature)
{
    constexpr auto sig = get_layout_signature<int&>();
    BOOST_CHECK_EQUAL(sig.c_str(), "[64-le]ref[s:8,a:8]");
}

BOOST_AUTO_TEST_CASE(rvalue_ref_signature)
{
    constexpr auto sig = get_layout_signature<int&&>();
    BOOST_CHECK_EQUAL(sig.c_str(), "[64-le]rref[s:8,a:8]");
}

BOOST_AUTO_TEST_SUITE_END()

//=============================================================================
// CV-Qualified Types
//=============================================================================

BOOST_AUTO_TEST_SUITE(CVQualifiedTypes)

BOOST_AUTO_TEST_CASE(const_stripped)
{
    constexpr auto base = get_layout_signature<int32_t>();
    constexpr auto cv = get_layout_signature<const int32_t>();
    BOOST_CHECK_EQUAL(base.c_str(), cv.c_str());
}

BOOST_AUTO_TEST_CASE(volatile_stripped)
{
    constexpr auto base = get_layout_signature<int32_t>();
    constexpr auto cv = get_layout_signature<volatile int32_t>();
    BOOST_CHECK_EQUAL(base.c_str(), cv.c_str());
}

BOOST_AUTO_TEST_CASE(const_volatile_stripped)
{
    constexpr auto base = get_layout_signature<int32_t>();
    constexpr auto cv = get_layout_signature<const volatile int32_t>();
    BOOST_CHECK_EQUAL(base.c_str(), cv.c_str());
}

BOOST_AUTO_TEST_SUITE_END()

//=============================================================================
// Function Pointer Types
//=============================================================================

BOOST_AUTO_TEST_SUITE(FunctionPointerTypes)

BOOST_AUTO_TEST_CASE(void_fn_ptr)
{
    using VoidFn = void(*)();
    constexpr auto sig = get_layout_signature<VoidFn>();
    BOOST_CHECK_EQUAL(sig.c_str(), "[64-le]fnptr[s:8,a:8]");
}

BOOST_AUTO_TEST_CASE(int_fn_ptr)
{
    using IntFn = int(*)(int, int);
    constexpr auto sig = get_layout_signature<IntFn>();
    BOOST_CHECK_EQUAL(sig.c_str(), "[64-le]fnptr[s:8,a:8]");
}

BOOST_AUTO_TEST_CASE(noexcept_fn_ptr)
{
    using NoexceptFn = void(*)() noexcept;
    constexpr auto sig = get_layout_signature<NoexceptFn>();
    BOOST_CHECK_EQUAL(sig.c_str(), "[64-le]fnptr[s:8,a:8]");
}

BOOST_AUTO_TEST_SUITE_END()
