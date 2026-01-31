// test_inheritance.cpp - Inheritance layout signature tests using Boost.Test
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#define BOOST_TEST_MODULE InheritanceTest
#include <boost/test/unit_test.hpp>
#include <boost/typelayout/typelayout.hpp>
#include <cstdint>

using namespace boost::typelayout;

//=============================================================================
// Base Classes
//=============================================================================

struct Base {
    int32_t base_value;
};

struct Derived : Base {
    int32_t derived_value;
};

struct DerivedDeep : Derived {
    int32_t deep_value;
};

//=============================================================================
// Multiple Inheritance
//=============================================================================

struct BaseA {
    int32_t a;
};

struct BaseB {
    int32_t b;
};

struct MultiDerived : BaseA, BaseB {
    int32_t c;
};

//=============================================================================
// Virtual Inheritance
//=============================================================================

struct VirtualBase {
    int32_t vb;
};

struct VirtualDerived1 : virtual VirtualBase {
    int32_t vd1;
};

struct VirtualDerived2 : virtual VirtualBase {
    int32_t vd2;
};

struct VirtualDiamond : VirtualDerived1, VirtualDerived2 {
    int32_t diamond;
};

//=============================================================================
// Abstract Base Class
//=============================================================================

struct AbstractBase {
    virtual ~AbstractBase() = default;
    virtual void doSomething() = 0;
    int32_t data;
};

struct ConcreteImpl : AbstractBase {
    void doSomething() override {}
    int32_t impl_data;
};

//=============================================================================
// Empty Base Optimization (EBO)
//=============================================================================

struct EmptyBase {};

struct NonEmptyDerived : EmptyBase {
    int32_t value;
};

struct TwoEmptyBases : EmptyBase {
    int32_t x;
};

//=============================================================================
// Single Inheritance Tests
//=============================================================================

BOOST_AUTO_TEST_SUITE(SingleInheritance)

BOOST_AUTO_TEST_CASE(base_class_layout)
{
    BOOST_CHECK_EQUAL(sizeof(Base), 4);
    
    constexpr auto sig = get_layout_signature<Base>();
    std::string sig_str = sig.c_str();
    
    BOOST_CHECK(sig_str.find("base_value") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(derived_class_layout)
{
    // Base(4) + Derived(4) = 8
    BOOST_CHECK_EQUAL(sizeof(Derived), 8);
    
    constexpr auto sig = get_layout_signature<Derived>();
    std::string sig_str = sig.c_str();
    
    // Should include both base and derived members
    BOOST_CHECK(sig_str.find("base_value") != std::string::npos);
    BOOST_CHECK(sig_str.find("derived_value") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(deep_inheritance_layout)
{
    // Base(4) + Derived(4) + DerivedDeep(4) = 12
    BOOST_CHECK_EQUAL(sizeof(DerivedDeep), 12);
    
    constexpr auto sig = get_layout_signature<DerivedDeep>();
    std::string sig_str = sig.c_str();
    
    BOOST_CHECK(sig_str.find("base_value") != std::string::npos);
    BOOST_CHECK(sig_str.find("derived_value") != std::string::npos);
    BOOST_CHECK(sig_str.find("deep_value") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(derived_different_from_base)
{
    constexpr auto base_sig = get_layout_signature<Base>();
    constexpr auto derived_sig = get_layout_signature<Derived>();
    
    BOOST_CHECK(std::string(base_sig.c_str()) != std::string(derived_sig.c_str()));
}

BOOST_AUTO_TEST_SUITE_END()

//=============================================================================
// Multiple Inheritance Tests
//=============================================================================

BOOST_AUTO_TEST_SUITE(MultipleInheritance)

BOOST_AUTO_TEST_CASE(multi_derived_layout)
{
    // BaseA(4) + BaseB(4) + MultiDerived(4) = 12
    BOOST_CHECK_EQUAL(sizeof(MultiDerived), 12);
    
    constexpr auto sig = get_layout_signature<MultiDerived>();
    std::string sig_str = sig.c_str();
    
    BOOST_CHECK(sig_str.find("a") != std::string::npos);
    BOOST_CHECK(sig_str.find("b") != std::string::npos);
    BOOST_CHECK(sig_str.find("c") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(base_order_matters)
{
    // Different base order should result in different layouts
    constexpr auto base_a = get_layout_signature<BaseA>();
    constexpr auto base_b = get_layout_signature<BaseB>();
    
    BOOST_CHECK(std::string(base_a.c_str()) != std::string(base_b.c_str()));
}

BOOST_AUTO_TEST_SUITE_END()

//=============================================================================
// Virtual Inheritance Tests
//=============================================================================

BOOST_AUTO_TEST_SUITE(VirtualInheritance)

BOOST_AUTO_TEST_CASE(virtual_base_has_vptr)
{
    // Virtual inheritance adds vtable pointer overhead
    BOOST_CHECK_GT(sizeof(VirtualDerived1), sizeof(VirtualBase) + sizeof(int32_t));
}

BOOST_AUTO_TEST_CASE(virtual_diamond_resolved)
{
    // Diamond inheritance with virtual base should not duplicate VirtualBase
    constexpr auto sig = get_layout_signature<VirtualDiamond>();
    std::string sig_str = sig.c_str();
    
    BOOST_CHECK(sig_str.find("diamond") != std::string::npos);
    BOOST_CHECK(sig_str.find("vd1") != std::string::npos);
    BOOST_CHECK(sig_str.find("vd2") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(virtual_types_platform_dependent)
{
    // Virtual inheritance layout varies by ABI
    BOOST_CHECK(is_platform_dependent_v<VirtualDiamond>);
}

BOOST_AUTO_TEST_SUITE_END()

//=============================================================================
// Abstract/Polymorphic Tests
//=============================================================================

BOOST_AUTO_TEST_SUITE(Polymorphic)

BOOST_AUTO_TEST_CASE(polymorphic_has_vptr)
{
    // Polymorphic classes have vtable pointer
    // Size should be larger than just data
    BOOST_CHECK_GT(sizeof(ConcreteImpl), sizeof(int32_t) * 2);
}

BOOST_AUTO_TEST_CASE(concrete_impl_layout)
{
    constexpr auto sig = get_layout_signature<ConcreteImpl>();
    std::string sig_str = sig.c_str();
    
    BOOST_CHECK(sig_str.find("data") != std::string::npos);
    BOOST_CHECK(sig_str.find("impl_data") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(polymorphic_types_dependent)
{
    // Polymorphic class layouts depend on vtable implementation
    BOOST_CHECK(is_platform_dependent_v<ConcreteImpl>);
}

BOOST_AUTO_TEST_SUITE_END()

//=============================================================================
// Empty Base Optimization Tests
//=============================================================================

BOOST_AUTO_TEST_SUITE(EmptyBaseOptimization)

BOOST_AUTO_TEST_CASE(empty_base_size)
{
    BOOST_CHECK_EQUAL(sizeof(EmptyBase), 1);
}

BOOST_AUTO_TEST_CASE(ebo_applied)
{
    // With EBO, NonEmptyDerived should only be 4 bytes (size of int32_t)
    // Without EBO, it would be 4 + 1 + padding
    BOOST_CHECK_LE(sizeof(NonEmptyDerived), 8);
}

BOOST_AUTO_TEST_CASE(ebo_derived_has_value)
{
    constexpr auto sig = get_layout_signature<NonEmptyDerived>();
    std::string sig_str = sig.c_str();
    
    BOOST_CHECK(sig_str.find("value") != std::string::npos);
}

BOOST_AUTO_TEST_SUITE_END()

//=============================================================================
// Inheritance Signature Determinism Tests
//=============================================================================

BOOST_AUTO_TEST_SUITE(InheritanceDeterminism)

BOOST_AUTO_TEST_CASE(inherited_layout_deterministic)
{
    constexpr auto sig1 = get_layout_signature<Derived>();
    constexpr auto sig2 = get_layout_signature<Derived>();
    BOOST_CHECK_EQUAL(sig1.c_str(), sig2.c_str());
}

BOOST_AUTO_TEST_CASE(multi_inherited_deterministic)
{
    constexpr auto sig1 = get_layout_signature<MultiDerived>();
    constexpr auto sig2 = get_layout_signature<MultiDerived>();
    BOOST_CHECK_EQUAL(sig1.c_str(), sig2.c_str());
}

BOOST_AUTO_TEST_SUITE_END()
