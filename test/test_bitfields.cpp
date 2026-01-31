// test_bitfields.cpp - Bitfield layout signature tests using Boost.Test
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#define BOOST_TEST_MODULE BitfieldsTest
#include <boost/test/unit_test.hpp>
#include <boost/typelayout/typelayout.hpp>
#include <cstdint>

using namespace boost::typelayout;

//=============================================================================
// Bitfield Structures
//=============================================================================

// Simple bitfield
struct SimpleBitfield {
    uint32_t a : 4;
    uint32_t b : 4;
    uint32_t c : 8;
    uint32_t d : 16;
};

// Bitfield with different underlying types
struct MixedBitfield {
    uint8_t  small : 4;
    uint16_t medium : 8;
    uint32_t large : 12;
};

// Bitfield with gaps (unnamed bitfields)
struct GappedBitfield {
    uint32_t a : 4;
    uint32_t   : 4;  // 4-bit gap
    uint32_t b : 8;
    uint32_t   : 0;  // force alignment to next unit
    uint32_t c : 4;
};

// Bitfield spanning storage units
struct SpanningBitfield {
    uint8_t a : 6;
    uint8_t b : 6;  // Spans into next byte
    uint8_t c : 4;
};

// Zero-width bitfield forcing alignment
struct ZeroWidthBitfield {
    uint32_t a : 8;
    uint32_t   : 0;  // Force alignment
    uint32_t b : 8;
};

// Signed and unsigned bitfields
struct SignedBitfield {
    int32_t  signed_val : 8;
    uint32_t unsigned_val : 8;
};

// Bitfield with bool (C++11+)
struct BoolBitfield {
    bool flag1 : 1;
    bool flag2 : 1;
    bool flag3 : 1;
    bool flag4 : 1;
};

// Large bitfield struct
struct LargeBitfield {
    uint64_t low  : 32;
    uint64_t high : 32;
};

// Bitfield mixed with regular members
struct MixedMembers {
    uint32_t regular;
    uint32_t bits : 16;
    uint32_t more_regular;
};

//=============================================================================
// Simple Bitfield Tests
//=============================================================================

BOOST_AUTO_TEST_SUITE(SimpleBitfields)

BOOST_AUTO_TEST_CASE(simple_bitfield_size)
{
    // All fields fit in one 32-bit unit
    BOOST_CHECK_EQUAL(sizeof(SimpleBitfield), 4);
}

BOOST_AUTO_TEST_CASE(simple_bitfield_signature)
{
    constexpr auto sig = get_layout_signature<SimpleBitfield>();
    std::string sig_str = sig.c_str();
    
    // Should contain bitfield information
    BOOST_CHECK(sig_str.find("a") != std::string::npos);
    BOOST_CHECK(sig_str.find("b") != std::string::npos);
    BOOST_CHECK(sig_str.find("c") != std::string::npos);
    BOOST_CHECK(sig_str.find("d") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(simple_bitfield_deterministic)
{
    constexpr auto sig1 = get_layout_signature<SimpleBitfield>();
    constexpr auto sig2 = get_layout_signature<SimpleBitfield>();
    BOOST_CHECK_EQUAL(sig1.c_str(), sig2.c_str());
}

BOOST_AUTO_TEST_SUITE_END()

//=============================================================================
// Mixed Type Bitfield Tests
//=============================================================================

BOOST_AUTO_TEST_SUITE(MixedBitfields)

BOOST_AUTO_TEST_CASE(mixed_bitfield_layout)
{
    constexpr auto sig = get_layout_signature<MixedBitfield>();
    std::string sig_str = sig.c_str();
    
    BOOST_CHECK(sig_str.find("small") != std::string::npos);
    BOOST_CHECK(sig_str.find("medium") != std::string::npos);
    BOOST_CHECK(sig_str.find("large") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(mixed_bitfield_platform_dependent)
{
    // Mixed underlying types for bitfields have ABI-dependent packing
    BOOST_CHECK(is_platform_dependent_v<MixedBitfield>);
}

BOOST_AUTO_TEST_SUITE_END()

//=============================================================================
// Gapped Bitfield Tests
//=============================================================================

BOOST_AUTO_TEST_SUITE(GappedBitfields)

BOOST_AUTO_TEST_CASE(gapped_bitfield_size)
{
    // The zero-width bitfield forces alignment, likely 8 bytes total
    BOOST_CHECK_GE(sizeof(GappedBitfield), 8);
}

BOOST_AUTO_TEST_CASE(gapped_bitfield_signature)
{
    constexpr auto sig = get_layout_signature<GappedBitfield>();
    std::string sig_str = sig.c_str();
    
    // Named fields should be in signature
    BOOST_CHECK(sig_str.find("a") != std::string::npos);
    BOOST_CHECK(sig_str.find("b") != std::string::npos);
    BOOST_CHECK(sig_str.find("c") != std::string::npos);
}

BOOST_AUTO_TEST_SUITE_END()

//=============================================================================
// Zero-Width Bitfield Tests
//=============================================================================

BOOST_AUTO_TEST_SUITE(ZeroWidthBitfields)

BOOST_AUTO_TEST_CASE(zero_width_forces_alignment)
{
    // Zero-width bitfield forces next field to new storage unit
    // Size should be 2 * sizeof(uint32_t) = 8
    BOOST_CHECK_EQUAL(sizeof(ZeroWidthBitfield), 8);
}

BOOST_AUTO_TEST_CASE(zero_width_signature)
{
    constexpr auto sig = get_layout_signature<ZeroWidthBitfield>();
    std::string sig_str = sig.c_str();
    
    BOOST_CHECK(sig_str.find("a") != std::string::npos);
    BOOST_CHECK(sig_str.find("b") != std::string::npos);
}

BOOST_AUTO_TEST_SUITE_END()

//=============================================================================
// Signed Bitfield Tests
//=============================================================================

BOOST_AUTO_TEST_SUITE(SignedBitfields)

BOOST_AUTO_TEST_CASE(signed_bitfield_size)
{
    BOOST_CHECK_EQUAL(sizeof(SignedBitfield), 4);
}

BOOST_AUTO_TEST_CASE(signed_bitfield_signature)
{
    constexpr auto sig = get_layout_signature<SignedBitfield>();
    std::string sig_str = sig.c_str();
    
    BOOST_CHECK(sig_str.find("signed_val") != std::string::npos);
    BOOST_CHECK(sig_str.find("unsigned_val") != std::string::npos);
}

BOOST_AUTO_TEST_SUITE_END()

//=============================================================================
// Bool Bitfield Tests
//=============================================================================

BOOST_AUTO_TEST_SUITE(BoolBitfields)

BOOST_AUTO_TEST_CASE(bool_bitfield_compact)
{
    // 4 bool bits should fit in 1 byte
    BOOST_CHECK_LE(sizeof(BoolBitfield), 4);  // May have padding
}

BOOST_AUTO_TEST_CASE(bool_bitfield_signature)
{
    constexpr auto sig = get_layout_signature<BoolBitfield>();
    std::string sig_str = sig.c_str();
    
    BOOST_CHECK(sig_str.find("flag1") != std::string::npos);
    BOOST_CHECK(sig_str.find("flag4") != std::string::npos);
}

BOOST_AUTO_TEST_SUITE_END()

//=============================================================================
// Large Bitfield Tests
//=============================================================================

BOOST_AUTO_TEST_SUITE(LargeBitfields)

BOOST_AUTO_TEST_CASE(large_bitfield_size)
{
    // Two 32-bit fields in one 64-bit storage
    BOOST_CHECK_EQUAL(sizeof(LargeBitfield), 8);
}

BOOST_AUTO_TEST_CASE(large_bitfield_signature)
{
    constexpr auto sig = get_layout_signature<LargeBitfield>();
    std::string sig_str = sig.c_str();
    
    BOOST_CHECK(sig_str.find("low") != std::string::npos);
    BOOST_CHECK(sig_str.find("high") != std::string::npos);
}

BOOST_AUTO_TEST_SUITE_END()

//=============================================================================
// Mixed Members Tests
//=============================================================================

BOOST_AUTO_TEST_SUITE(MixedMembersWithBitfields)

BOOST_AUTO_TEST_CASE(mixed_members_layout)
{
    constexpr auto sig = get_layout_signature<MixedMembers>();
    std::string sig_str = sig.c_str();
    
    BOOST_CHECK(sig_str.find("regular") != std::string::npos);
    BOOST_CHECK(sig_str.find("bits") != std::string::npos);
    BOOST_CHECK(sig_str.find("more_regular") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(mixed_members_size)
{
    // regular(4) + bits in own unit(4) + more_regular(4) = 12
    BOOST_CHECK_GE(sizeof(MixedMembers), 12);
}

BOOST_AUTO_TEST_SUITE_END()

//=============================================================================
// Platform Dependency Tests
//=============================================================================

BOOST_AUTO_TEST_SUITE(BitfieldPlatformDependency)

BOOST_AUTO_TEST_CASE(spanning_bitfield_platform_dependent)
{
    // Bitfields that span storage units have ABI-dependent layout
    BOOST_CHECK(is_platform_dependent_v<SpanningBitfield>);
}

BOOST_AUTO_TEST_CASE(gapped_bitfield_platform_dependent)
{
    // Unnamed bitfields and zero-width fields are ABI-dependent
    BOOST_CHECK(is_platform_dependent_v<GappedBitfield>);
}

BOOST_AUTO_TEST_CASE(simple_single_unit_may_be_portable)
{
    // Simple bitfields within a single storage unit may be more portable
    // but still depend on bit ordering within the unit
    constexpr auto sig = get_layout_signature<SimpleBitfield>();
    // Just verify we can get a signature
    BOOST_CHECK(sig.size() > 0);
}

BOOST_AUTO_TEST_SUITE_END()

//=============================================================================
// Bitfield Comparison Tests
//=============================================================================

BOOST_AUTO_TEST_SUITE(BitfieldComparison)

BOOST_AUTO_TEST_CASE(different_bitfield_structs_differ)
{
    constexpr auto sig1 = get_layout_signature<SimpleBitfield>();
    constexpr auto sig2 = get_layout_signature<SignedBitfield>();
    
    BOOST_CHECK(std::string(sig1.c_str()) != std::string(sig2.c_str()));
}

BOOST_AUTO_TEST_CASE(same_bitfield_struct_consistent)
{
    constexpr auto sig1 = get_layout_signature<LargeBitfield>();
    constexpr auto sig2 = get_layout_signature<LargeBitfield>();
    
    BOOST_CHECK_EQUAL(sig1.c_str(), sig2.c_str());
}

BOOST_AUTO_TEST_SUITE_END()
