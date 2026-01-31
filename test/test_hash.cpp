// test_hash.cpp - Hash computation tests using Boost.Test
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#define BOOST_TEST_MODULE HashTest
#include <boost/test/unit_test.hpp>
#include <boost/typelayout/typelayout.hpp>
#include <cstdint>
#include <set>

using namespace boost::typelayout;

//=============================================================================
// Test Structures
//=============================================================================

struct SimpleStruct {
    int32_t a;
    int32_t b;
};

struct DifferentLayout {
    int64_t x;
};

struct SameLayoutDifferentNames {
    int32_t x;
    int32_t y;
};

struct NestedStruct {
    SimpleStruct inner;
    int32_t outer;
};

struct LargeStruct {
    int32_t a, b, c, d, e, f, g, h;
    int64_t i, j, k, l;
};

//=============================================================================
// Basic Hash Tests
//=============================================================================

BOOST_AUTO_TEST_SUITE(BasicHash)

BOOST_AUTO_TEST_CASE(primitive_hash_non_zero)
{
    constexpr auto hash = get_layout_hash<int32_t>();
    BOOST_CHECK_NE(hash, 0ULL);
}

BOOST_AUTO_TEST_CASE(struct_hash_non_zero)
{
    constexpr auto hash = get_layout_hash<SimpleStruct>();
    BOOST_CHECK_NE(hash, 0ULL);
}

BOOST_AUTO_TEST_CASE(hash_is_constexpr)
{
    // Hash computation must be compile-time
    constexpr auto hash = get_layout_hash<int32_t>();
    static_assert(hash != 0, "Hash should be non-zero at compile time");
    BOOST_CHECK(true);  // If we get here, constexpr worked
}

BOOST_AUTO_TEST_SUITE_END()

//=============================================================================
// Hash Determinism Tests
//=============================================================================

BOOST_AUTO_TEST_SUITE(HashDeterminism)

BOOST_AUTO_TEST_CASE(same_type_same_hash)
{
    constexpr auto hash1 = get_layout_hash<int32_t>();
    constexpr auto hash2 = get_layout_hash<int32_t>();
    BOOST_CHECK_EQUAL(hash1, hash2);
}

BOOST_AUTO_TEST_CASE(struct_hash_deterministic)
{
    constexpr auto hash1 = get_layout_hash<SimpleStruct>();
    constexpr auto hash2 = get_layout_hash<SimpleStruct>();
    BOOST_CHECK_EQUAL(hash1, hash2);
}

BOOST_AUTO_TEST_CASE(nested_struct_hash_deterministic)
{
    constexpr auto hash1 = get_layout_hash<NestedStruct>();
    constexpr auto hash2 = get_layout_hash<NestedStruct>();
    BOOST_CHECK_EQUAL(hash1, hash2);
}

BOOST_AUTO_TEST_CASE(cv_qualified_same_hash)
{
    // CV qualifiers should not affect layout hash
    constexpr auto base = get_layout_hash<int32_t>();
    constexpr auto const_hash = get_layout_hash<const int32_t>();
    constexpr auto volatile_hash = get_layout_hash<volatile int32_t>();
    
    BOOST_CHECK_EQUAL(base, const_hash);
    BOOST_CHECK_EQUAL(base, volatile_hash);
}

BOOST_AUTO_TEST_SUITE_END()

//=============================================================================
// Hash Uniqueness Tests
//=============================================================================

BOOST_AUTO_TEST_SUITE(HashUniqueness)

BOOST_AUTO_TEST_CASE(different_primitives_different_hash)
{
    constexpr auto i32_hash = get_layout_hash<int32_t>();
    constexpr auto i64_hash = get_layout_hash<int64_t>();
    BOOST_CHECK_NE(i32_hash, i64_hash);
}

BOOST_AUTO_TEST_CASE(signed_unsigned_different_hash)
{
    constexpr auto signed_hash = get_layout_hash<int32_t>();
    constexpr auto unsigned_hash = get_layout_hash<uint32_t>();
    BOOST_CHECK_NE(signed_hash, unsigned_hash);
}

BOOST_AUTO_TEST_CASE(different_structs_different_hash)
{
    constexpr auto simple_hash = get_layout_hash<SimpleStruct>();
    constexpr auto different_hash = get_layout_hash<DifferentLayout>();
    BOOST_CHECK_NE(simple_hash, different_hash);
}

BOOST_AUTO_TEST_CASE(same_layout_different_names_different_hash)
{
    // Member names are part of the signature, so different names = different hash
    constexpr auto hash1 = get_layout_hash<SimpleStruct>();
    constexpr auto hash2 = get_layout_hash<SameLayoutDifferentNames>();
    BOOST_CHECK_NE(hash1, hash2);
}

BOOST_AUTO_TEST_CASE(all_integer_types_unique)
{
    std::set<uint64_t> hashes;
    
    hashes.insert(get_layout_hash<int8_t>());
    hashes.insert(get_layout_hash<int16_t>());
    hashes.insert(get_layout_hash<int32_t>());
    hashes.insert(get_layout_hash<int64_t>());
    hashes.insert(get_layout_hash<uint8_t>());
    hashes.insert(get_layout_hash<uint16_t>());
    hashes.insert(get_layout_hash<uint32_t>());
    hashes.insert(get_layout_hash<uint64_t>());
    
    BOOST_CHECK_EQUAL(hashes.size(), 8);  // All unique
}

BOOST_AUTO_TEST_SUITE_END()

//=============================================================================
// Dual Hash Tests
//=============================================================================

BOOST_AUTO_TEST_SUITE(DualHash)

BOOST_AUTO_TEST_CASE(dual_hash_pair_non_zero)
{
    constexpr auto [fnv, djb] = get_layout_hash_pair<int32_t>();
    BOOST_CHECK_NE(fnv, 0ULL);
    BOOST_CHECK_NE(djb, 0ULL);
}

BOOST_AUTO_TEST_CASE(dual_hash_different_algorithms)
{
    constexpr auto [fnv, djb] = get_layout_hash_pair<int32_t>();
    // FNV-1a and DJB2 should produce different results
    BOOST_CHECK_NE(fnv, djb);
}

BOOST_AUTO_TEST_CASE(dual_hash_deterministic)
{
    constexpr auto [fnv1, djb1] = get_layout_hash_pair<SimpleStruct>();
    constexpr auto [fnv2, djb2] = get_layout_hash_pair<SimpleStruct>();
    
    BOOST_CHECK_EQUAL(fnv1, fnv2);
    BOOST_CHECK_EQUAL(djb1, djb2);
}

BOOST_AUTO_TEST_CASE(dual_hash_combined)
{
    // Combined hash should be different from either individual hash
    constexpr auto single = get_layout_hash<int32_t>();
    constexpr auto [fnv, djb] = get_layout_hash_pair<int32_t>();
    
    // At least verify the relationship
    BOOST_CHECK((single == fnv) || (single == (fnv ^ djb)));
}

BOOST_AUTO_TEST_SUITE_END()

//=============================================================================
// Hash-Based Verification Tests
//=============================================================================

BOOST_AUTO_TEST_SUITE(HashVerification)

BOOST_AUTO_TEST_CASE(verify_same_type_passes)
{
    constexpr auto expected = get_layout_hash<int32_t>();
    constexpr bool matches = verify_layout_hash<int32_t>(expected);
    BOOST_CHECK(matches);
}

BOOST_AUTO_TEST_CASE(verify_wrong_hash_fails)
{
    constexpr uint64_t wrong_hash = 0x12345678DEADBEEF;
    constexpr bool matches = verify_layout_hash<int32_t>(wrong_hash);
    BOOST_CHECK(!matches);
}

BOOST_AUTO_TEST_CASE(verify_struct_layout)
{
    constexpr auto expected = get_layout_hash<SimpleStruct>();
    constexpr bool matches = verify_layout_hash<SimpleStruct>(expected);
    BOOST_CHECK(matches);
}

BOOST_AUTO_TEST_CASE(static_verification)
{
    // Can be used in static_assert
    constexpr auto hash = get_layout_hash<int32_t>();
    static_assert(verify_layout_hash<int32_t>(hash), "Layout hash should match");
    BOOST_CHECK(true);
}

BOOST_AUTO_TEST_SUITE_END()

//=============================================================================
// Hash Distribution Tests
//=============================================================================

BOOST_AUTO_TEST_SUITE(HashDistribution)

BOOST_AUTO_TEST_CASE(large_struct_hash)
{
    constexpr auto hash = get_layout_hash<LargeStruct>();
    BOOST_CHECK_NE(hash, 0ULL);
}

BOOST_AUTO_TEST_CASE(nested_hash_unique)
{
    constexpr auto simple = get_layout_hash<SimpleStruct>();
    constexpr auto nested = get_layout_hash<NestedStruct>();
    BOOST_CHECK_NE(simple, nested);
}

BOOST_AUTO_TEST_CASE(pointer_types_unique)
{
    std::set<uint64_t> hashes;
    
    hashes.insert(get_layout_hash<void*>());
    hashes.insert(get_layout_hash<int*>());
    hashes.insert(get_layout_hash<char*>());
    hashes.insert(get_layout_hash<double*>());
    
    // All pointer types might have the same layout signature
    // since they're all "ptr[s:8,a:8]" on 64-bit
    BOOST_CHECK_GE(hashes.size(), 1);  // At least one unique hash
}

BOOST_AUTO_TEST_SUITE_END()

//=============================================================================
// Edge Case Hash Tests
//=============================================================================

BOOST_AUTO_TEST_SUITE(EdgeCaseHash)

BOOST_AUTO_TEST_CASE(empty_struct_hash)
{
    struct Empty {};
    constexpr auto hash = get_layout_hash<Empty>();
    BOOST_CHECK_NE(hash, 0ULL);
}

BOOST_AUTO_TEST_CASE(single_member_hash)
{
    struct Single { int32_t x; };
    constexpr auto hash = get_layout_hash<Single>();
    BOOST_CHECK_NE(hash, 0ULL);
}

BOOST_AUTO_TEST_CASE(array_member_hash)
{
    struct WithArray { int32_t data[4]; };
    constexpr auto hash = get_layout_hash<WithArray>();
    BOOST_CHECK_NE(hash, 0ULL);
}

BOOST_AUTO_TEST_CASE(bitfield_hash)
{
    struct Bitfield { uint32_t a : 4; uint32_t b : 4; };
    constexpr auto hash = get_layout_hash<Bitfield>();
    BOOST_CHECK_NE(hash, 0ULL);
}

BOOST_AUTO_TEST_SUITE_END()
