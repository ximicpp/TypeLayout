// Boost.TypeLayout - Unit Tests using Boost.Test
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// Use Boost.Test in header-only mode
#define BOOST_TEST_MODULE TypeLayout Core Tests
#include <boost/test/included/unit_test.hpp>

#include <boost/typelayout/typelayout.hpp>
#include <cstdint>
#include <optional>
#include <variant>
#include <tuple>

using namespace boost::typelayout;

// =========================================================================
// Test Types
// =========================================================================

struct EmptyStruct {};

struct SimplePOD {
    int32_t x;
    int32_t y;
};

class SimpleClass {
public:
    SimpleClass() : x_(0), y_(0) {}
    [[maybe_unused]] int32_t sum() const { return x_ + y_; }
private:
    int32_t x_;
    int32_t y_;
};

struct NestedStruct {
    SimplePOD inner;
    int32_t z;
};

class Base {
public:
    Base() = default;
protected:
    int32_t base_val_;
};

class DerivedSingle : public Base {
public:
    DerivedSingle() : derived_val_(0) {}
    [[maybe_unused]] int32_t get() const { return derived_val_; }
private:
    int32_t derived_val_;
};

struct BitFieldStruct {
    uint32_t a : 4;
    uint32_t b : 8;
    uint32_t c : 20;
};

struct alignas(16) AlignedStruct {
    int32_t x;
    int32_t y;
};

union SimpleUnion {
    int32_t i;
    float f;
    char c[4];
};

enum class ScopedEnum : uint16_t { X, Y, Z };

// =========================================================================
// Fundamental Types Tests
// =========================================================================

BOOST_AUTO_TEST_SUITE(fundamental_types)

BOOST_AUTO_TEST_CASE(integer_types)
{
    // Compile-time signature generation
    constexpr auto sig_i8 = get_layout_signature<int8_t>();
    constexpr auto sig_u8 = get_layout_signature<uint8_t>();
    constexpr auto sig_i16 = get_layout_signature<int16_t>();
    constexpr auto sig_u16 = get_layout_signature<uint16_t>();
    constexpr auto sig_i32 = get_layout_signature<int32_t>();
    constexpr auto sig_u32 = get_layout_signature<uint32_t>();
    constexpr auto sig_i64 = get_layout_signature<int64_t>();
    constexpr auto sig_u64 = get_layout_signature<uint64_t>();
    
    // Verify all signatures are non-empty
    BOOST_TEST(sig_i8.size > 0);
    BOOST_TEST(sig_u8.size > 0);
    BOOST_TEST(sig_i16.size > 0);
    BOOST_TEST(sig_u16.size > 0);
    BOOST_TEST(sig_i32.size > 0);
    BOOST_TEST(sig_u32.size > 0);
    BOOST_TEST(sig_i64.size > 0);
    BOOST_TEST(sig_u64.size > 0);
    
    // Different sizes should produce different signatures
    BOOST_TEST(sig_i8 != sig_i16);
    BOOST_TEST(sig_i16 != sig_i32);
    BOOST_TEST(sig_i32 != sig_i64);
    
    // Signed vs unsigned should differ
    BOOST_TEST(sig_i8 != sig_u8);
    BOOST_TEST(sig_i16 != sig_u16);
    BOOST_TEST(sig_i32 != sig_u32);
    BOOST_TEST(sig_i64 != sig_u64);
}

BOOST_AUTO_TEST_CASE(floating_point_types)
{
    constexpr auto sig_float = get_layout_signature<float>();
    constexpr auto sig_double = get_layout_signature<double>();
    
    BOOST_TEST(sig_float.size > 0);
    BOOST_TEST(sig_double.size > 0);
    BOOST_TEST(sig_float != sig_double);
}

BOOST_AUTO_TEST_CASE(character_types)
{
    constexpr auto sig_char = get_layout_signature<char>();
    constexpr auto sig_wchar = get_layout_signature<wchar_t>();
    constexpr auto sig_char8 = get_layout_signature<char8_t>();
    constexpr auto sig_char16 = get_layout_signature<char16_t>();
    constexpr auto sig_char32 = get_layout_signature<char32_t>();
    
    // All character types should produce valid signatures
    BOOST_TEST(sig_char.size > 0);
    BOOST_TEST(sig_wchar.size > 0);
    BOOST_TEST(sig_char8.size > 0);
    BOOST_TEST(sig_char16.size > 0);
    BOOST_TEST(sig_char32.size > 0);
    
    // Different character types should differ
    BOOST_TEST(sig_char8 != sig_char16);
    BOOST_TEST(sig_char16 != sig_char32);
}

BOOST_AUTO_TEST_CASE(cv_qualifiers_stripped)
{
    // CV qualifiers should not affect layout signature
    constexpr auto sig = get_layout_signature<int32_t>();
    constexpr auto sig_const = get_layout_signature<const int32_t>();
    constexpr auto sig_volatile = get_layout_signature<volatile int32_t>();
    constexpr auto sig_cv = get_layout_signature<const volatile int32_t>();
    
    BOOST_TEST(sig == sig_const);
    BOOST_TEST(sig == sig_volatile);
    BOOST_TEST(sig == sig_cv);
}

BOOST_AUTO_TEST_SUITE_END()

// =========================================================================
// Compound Types Tests
// =========================================================================

BOOST_AUTO_TEST_SUITE(compound_types)

BOOST_AUTO_TEST_CASE(pointer_types)
{
    constexpr auto sig_int_ptr = get_layout_signature<int*>();
    constexpr auto sig_void_ptr = get_layout_signature<void*>();
    constexpr auto sig_const_ptr = get_layout_signature<const int*>();
    constexpr auto sig_ptr_ptr = get_layout_signature<int**>();
    
    // All pointer types should produce valid signatures
    BOOST_TEST(sig_int_ptr.size > 0);
    BOOST_TEST(sig_void_ptr.size > 0);
    BOOST_TEST(sig_const_ptr.size > 0);
    BOOST_TEST(sig_ptr_ptr.size > 0);
    
    // From a memory layout perspective, all pointers have identical layout
    // (same size and alignment), so their signatures should match.
    // This is by design: TypeLayout captures physical memory layout, not type semantics.
    BOOST_TEST(sig_int_ptr == sig_void_ptr);
    BOOST_TEST(sig_int_ptr == sig_ptr_ptr);
    BOOST_TEST(sig_int_ptr == sig_const_ptr);
    
    // All pointers have same size on same platform
    BOOST_TEST(sizeof(int*) == sizeof(void*));
}

BOOST_AUTO_TEST_CASE(array_types)
{
    constexpr auto sig_arr10 = get_layout_signature<int[10]>();
    constexpr auto sig_arr5 = get_layout_signature<int[5]>();
    constexpr auto sig_arr2d = get_layout_signature<int[3][4]>();
    
    // All array types should produce valid signatures
    BOOST_TEST(sig_arr10.size > 0);
    BOOST_TEST(sig_arr5.size > 0);
    BOOST_TEST(sig_arr2d.size > 0);
    
    // Different array dimensions produce different signatures
    BOOST_TEST(sig_arr10 != sig_arr5);
    BOOST_TEST(sig_arr10 != sig_arr2d);
}

BOOST_AUTO_TEST_SUITE_END()

// =========================================================================
// User-Defined Types Tests
// =========================================================================

BOOST_AUTO_TEST_SUITE(user_defined_types)

BOOST_AUTO_TEST_CASE(empty_struct)
{
    constexpr auto sig = get_layout_signature<EmptyStruct>();
    BOOST_TEST(sig.size > 0);
    BOOST_TEST(sizeof(EmptyStruct) == 1);  // Empty struct has size 1
}

BOOST_AUTO_TEST_CASE(simple_pod)
{
    constexpr auto sig = get_layout_signature<SimplePOD>();
    BOOST_TEST(sig.size > 0);
    BOOST_TEST(sizeof(SimplePOD) == 8);  // 2 x int32_t
}

BOOST_AUTO_TEST_CASE(class_with_private_members)
{
    // TypeLayout fully supports classes with private members
    constexpr auto sig = get_layout_signature<SimpleClass>();
    BOOST_TEST(sig.size > 0);
    BOOST_TEST(sizeof(SimpleClass) == sizeof(SimplePOD));  // Same layout
}

BOOST_AUTO_TEST_CASE(nested_struct)
{
    constexpr auto sig = get_layout_signature<NestedStruct>();
    BOOST_TEST(sig.size > 0);
    BOOST_TEST(sizeof(NestedStruct) == sizeof(SimplePOD) + sizeof(int32_t));
}

BOOST_AUTO_TEST_CASE(inheritance)
{
    constexpr auto sig_base = get_layout_signature<Base>();
    constexpr auto sig_derived = get_layout_signature<DerivedSingle>();
    
    BOOST_TEST(sig_base.size > 0);
    BOOST_TEST(sig_derived.size > 0);
    BOOST_TEST(sig_base != sig_derived);
    BOOST_TEST(sizeof(DerivedSingle) > sizeof(Base));
}

BOOST_AUTO_TEST_CASE(bitfields)
{
    constexpr auto sig = get_layout_signature<BitFieldStruct>();
    BOOST_TEST(sig.size > 0);
    // 4 + 8 + 20 = 32 bits = 4 bytes
    BOOST_TEST(sizeof(BitFieldStruct) == 4);
}

BOOST_AUTO_TEST_CASE(aligned_struct)
{
    constexpr auto sig = get_layout_signature<AlignedStruct>();
    BOOST_TEST(sig.size > 0);
    BOOST_TEST(alignof(AlignedStruct) == 16);
}

BOOST_AUTO_TEST_CASE(union_type)
{
    constexpr auto sig = get_layout_signature<SimpleUnion>();
    BOOST_TEST(sig.size > 0);
    BOOST_TEST(sizeof(SimpleUnion) == 4);  // Size of largest member
}

BOOST_AUTO_TEST_CASE(enum_type)
{
    constexpr auto sig = get_layout_signature<ScopedEnum>();
    BOOST_TEST(sig.size > 0);
    BOOST_TEST(sizeof(ScopedEnum) == 2);  // uint16_t underlying type
}

BOOST_AUTO_TEST_SUITE_END()

// =========================================================================
// Standard Library Types Tests
// =========================================================================

BOOST_AUTO_TEST_SUITE(std_types)

BOOST_AUTO_TEST_CASE(optional)
{
    constexpr auto sig = get_layout_signature<std::optional<int>>();
    BOOST_TEST(sig.size > 0);
}

BOOST_AUTO_TEST_CASE(variant)
{
    using V = std::variant<int, float, double>;
    constexpr auto sig = get_layout_signature<V>();
    BOOST_TEST(sig.size > 0);
}

BOOST_AUTO_TEST_CASE(tuple)
{
    using T = std::tuple<int, float, double>;
    constexpr auto sig = get_layout_signature<T>();
    BOOST_TEST(sig.size > 0);
}

BOOST_AUTO_TEST_SUITE_END()

// =========================================================================
// Signature Matching Tests
// =========================================================================

BOOST_AUTO_TEST_SUITE(signature_matching)

BOOST_AUTO_TEST_CASE(same_type_matches)
{
    constexpr bool match = signatures_match<int32_t, int32_t>();
    BOOST_TEST(match == true);
}

BOOST_AUTO_TEST_CASE(different_types_dont_match)
{
    constexpr bool match = signatures_match<int32_t, int64_t>();
    BOOST_TEST(match == false);
}

BOOST_AUTO_TEST_CASE(layout_hash_consistency)
{
    constexpr auto hash1 = get_layout_hash<SimplePOD>();
    constexpr auto hash2 = get_layout_hash<SimplePOD>();
    BOOST_TEST(hash1 == hash2);
}

BOOST_AUTO_TEST_CASE(different_layouts_different_hashes)
{
    constexpr auto hash1 = get_layout_hash<SimplePOD>();
    constexpr auto hash2 = get_layout_hash<NestedStruct>();
    BOOST_TEST(hash1 != hash2);
}

BOOST_AUTO_TEST_SUITE_END()

// =========================================================================
// Compile-Time Verification Tests  
// =========================================================================

BOOST_AUTO_TEST_SUITE(compile_time_verification)

BOOST_AUTO_TEST_CASE(static_assert_abi_guard)
{
    // This is the primary use case: compile-time ABI verification
    // Define expected layout hash (would be stored in documentation/config)
    constexpr auto current_hash = get_layout_hash<SimplePOD>();
    
    // In real code, this would be:
    // static_assert(get_layout_hash<SimplePOD>() == EXPECTED_HASH, 
    //               "ABI break detected!");
    
    BOOST_TEST(current_hash != 0);  // Hash should be non-zero
}

BOOST_AUTO_TEST_CASE(layout_verification_struct)
{
    constexpr auto verification = get_layout_verification<SimplePOD>();
    
    // LayoutVerification contains dual-hash (fnv1a + djb2) and length
    BOOST_TEST(verification.fnv1a != 0);
    BOOST_TEST(verification.djb2 != 0);
    BOOST_TEST(verification.length > 0);
    
    // Dual-hash should be different (independent algorithms)
    BOOST_TEST(verification.fnv1a != verification.djb2);
}

BOOST_AUTO_TEST_SUITE_END()
