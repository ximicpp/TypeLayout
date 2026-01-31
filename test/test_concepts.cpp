// test_concepts.cpp - Concept constraints tests using Boost.Test
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#define BOOST_TEST_MODULE ConceptsTest
#include <boost/test/unit_test.hpp>
#include <boost/typelayout/typelayout.hpp>
#include <cstdint>
#include <string>
#include <vector>
#include <optional>
#include <variant>

using namespace boost::typelayout;

//=============================================================================
// Test Types
//=============================================================================

// Standard layout type
struct StandardLayoutType {
    int32_t a;
    int32_t b;
};

// Non-standard layout (virtual functions)
struct NonStandardLayout {
    virtual ~NonStandardLayout() = default;
    int32_t data;
};

// Trivially copyable
struct TriviallyCopyable {
    int32_t x;
    double y;
};

// Non-trivially copyable
struct NonTriviallyCopyable {
    std::string str;  // Has non-trivial copy
};

// POD-like type (aggregate)
struct PODLike {
    int32_t a;
    float b;
    char c[4];
};

// Type with private members
class WithPrivate {
private:
    int32_t secret;
public:
    int32_t visible;
};

// Type with static members (should be excluded)
struct WithStatic {
    static int32_t static_member;
    int32_t instance_member;
};
int32_t WithStatic::static_member = 0;

// Union type
union UnionType {
    int32_t i;
    float f;
    char bytes[4];
};

// Enum types
enum OldEnum { A, B, C };
enum class ScopedEnum : uint16_t { X, Y, Z };

// Template instantiation
template<typename T>
struct GenericContainer {
    T value;
    size_t count;
};

//=============================================================================
// Reflectable Concept Tests
//=============================================================================

BOOST_AUTO_TEST_SUITE(ReflectableConcept)

BOOST_AUTO_TEST_CASE(primitives_are_reflectable)
{
    BOOST_CHECK(reflectable<int32_t>);
    BOOST_CHECK(reflectable<float>);
    BOOST_CHECK(reflectable<char>);
    BOOST_CHECK(reflectable<bool>);
}

BOOST_AUTO_TEST_CASE(standard_layout_reflectable)
{
    BOOST_CHECK(reflectable<StandardLayoutType>);
}

BOOST_AUTO_TEST_CASE(non_standard_layout_reflectable)
{
    // Should still be reflectable even if not standard layout
    BOOST_CHECK(reflectable<NonStandardLayout>);
}

BOOST_AUTO_TEST_CASE(pod_like_reflectable)
{
    BOOST_CHECK(reflectable<PODLike>);
}

BOOST_AUTO_TEST_CASE(with_private_reflectable)
{
    // Private members should still allow reflection
    BOOST_CHECK(reflectable<WithPrivate>);
}

BOOST_AUTO_TEST_CASE(pointers_reflectable)
{
    BOOST_CHECK(reflectable<int*>);
    BOOST_CHECK(reflectable<void*>);
    BOOST_CHECK(reflectable<const char*>);
}

BOOST_AUTO_TEST_CASE(arrays_reflectable)
{
    BOOST_CHECK(reflectable<int32_t[10]>);
    BOOST_CHECK(reflectable<char[100]>);
}

BOOST_AUTO_TEST_SUITE_END()

//=============================================================================
// Standard Layout Concept Tests
//=============================================================================

BOOST_AUTO_TEST_SUITE(StandardLayoutConcept)

BOOST_AUTO_TEST_CASE(primitives_standard_layout)
{
    BOOST_CHECK(std::is_standard_layout_v<int32_t>);
    BOOST_CHECK(std::is_standard_layout_v<double>);
}

BOOST_AUTO_TEST_CASE(simple_struct_standard_layout)
{
    BOOST_CHECK(std::is_standard_layout_v<StandardLayoutType>);
}

BOOST_AUTO_TEST_CASE(virtual_not_standard_layout)
{
    BOOST_CHECK(!std::is_standard_layout_v<NonStandardLayout>);
}

BOOST_AUTO_TEST_CASE(pod_like_standard_layout)
{
    BOOST_CHECK(std::is_standard_layout_v<PODLike>);
}

BOOST_AUTO_TEST_SUITE_END()

//=============================================================================
// Trivial Copyability Tests
//=============================================================================

BOOST_AUTO_TEST_SUITE(TrivialCopyability)

BOOST_AUTO_TEST_CASE(primitives_trivially_copyable)
{
    BOOST_CHECK(std::is_trivially_copyable_v<int32_t>);
    BOOST_CHECK(std::is_trivially_copyable_v<double>);
}

BOOST_AUTO_TEST_CASE(trivial_struct_copyable)
{
    BOOST_CHECK(std::is_trivially_copyable_v<TriviallyCopyable>);
}

BOOST_AUTO_TEST_CASE(string_not_trivially_copyable)
{
    BOOST_CHECK(!std::is_trivially_copyable_v<NonTriviallyCopyable>);
}

BOOST_AUTO_TEST_CASE(pod_trivially_copyable)
{
    BOOST_CHECK(std::is_trivially_copyable_v<PODLike>);
}

BOOST_AUTO_TEST_SUITE_END()

//=============================================================================
// Portable Layout Concept Tests
//=============================================================================

BOOST_AUTO_TEST_SUITE(PortableLayoutConcept)

BOOST_AUTO_TEST_CASE(fixed_width_portable)
{
    BOOST_CHECK(portable_layout<int32_t>);
    BOOST_CHECK(portable_layout<uint64_t>);
    BOOST_CHECK(portable_layout<float>);
    BOOST_CHECK(portable_layout<double>);
}

BOOST_AUTO_TEST_CASE(platform_types_not_portable)
{
    BOOST_CHECK(!portable_layout<long>);
    BOOST_CHECK(!portable_layout<size_t>);
    BOOST_CHECK(!portable_layout<wchar_t>);
}

BOOST_AUTO_TEST_CASE(struct_with_fixed_types_portable)
{
    BOOST_CHECK(portable_layout<StandardLayoutType>);
}

BOOST_AUTO_TEST_CASE(struct_with_pointer_not_portable)
{
    struct WithPtr { int* p; };
    BOOST_CHECK(!portable_layout<WithPtr>);
}

BOOST_AUTO_TEST_SUITE_END()

//=============================================================================
// Union Tests
//=============================================================================

BOOST_AUTO_TEST_SUITE(UnionTests)

BOOST_AUTO_TEST_CASE(union_reflectable)
{
    BOOST_CHECK(reflectable<UnionType>);
}

BOOST_AUTO_TEST_CASE(union_signature)
{
    constexpr auto sig = get_layout_signature<UnionType>();
    std::string sig_str = sig.c_str();
    
    // Size should be 4 (largest member)
    BOOST_CHECK(sig_str.find("s:4") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(union_standard_layout)
{
    BOOST_CHECK(std::is_standard_layout_v<UnionType>);
}

BOOST_AUTO_TEST_SUITE_END()

//=============================================================================
// Enum Tests
//=============================================================================

BOOST_AUTO_TEST_SUITE(EnumTests)

BOOST_AUTO_TEST_CASE(old_enum_reflectable)
{
    BOOST_CHECK(reflectable<OldEnum>);
}

BOOST_AUTO_TEST_CASE(scoped_enum_reflectable)
{
    BOOST_CHECK(reflectable<ScopedEnum>);
}

BOOST_AUTO_TEST_CASE(scoped_enum_size)
{
    BOOST_CHECK_EQUAL(sizeof(ScopedEnum), 2);  // uint16_t underlying
}

BOOST_AUTO_TEST_CASE(enum_signature)
{
    constexpr auto sig = get_layout_signature<ScopedEnum>();
    std::string sig_str = sig.c_str();
    
    // Should show size 2, align 2
    BOOST_CHECK(sig_str.find("s:2") != std::string::npos);
}

BOOST_AUTO_TEST_SUITE_END()

//=============================================================================
// Template Instantiation Tests
//=============================================================================

BOOST_AUTO_TEST_SUITE(TemplateTests)

BOOST_AUTO_TEST_CASE(generic_container_int_reflectable)
{
    using IntContainer = GenericContainer<int32_t>;
    BOOST_CHECK(reflectable<IntContainer>);
}

BOOST_AUTO_TEST_CASE(generic_container_signature)
{
    using IntContainer = GenericContainer<int32_t>;
    constexpr auto sig = get_layout_signature<IntContainer>();
    std::string sig_str = sig.c_str();
    
    BOOST_CHECK(sig_str.find("value") != std::string::npos);
    BOOST_CHECK(sig_str.find("count") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(different_instantiations_different)
{
    constexpr auto int_hash = get_layout_hash<GenericContainer<int32_t>>();
    constexpr auto double_hash = get_layout_hash<GenericContainer<double>>();
    
    BOOST_CHECK_NE(int_hash, double_hash);
}

BOOST_AUTO_TEST_SUITE_END()

//=============================================================================
// Static Member Exclusion Tests
//=============================================================================

BOOST_AUTO_TEST_SUITE(StaticMemberExclusion)

BOOST_AUTO_TEST_CASE(static_members_excluded)
{
    // Size should only include instance member
    BOOST_CHECK_EQUAL(sizeof(WithStatic), sizeof(int32_t));
}

BOOST_AUTO_TEST_CASE(static_not_in_signature)
{
    constexpr auto sig = get_layout_signature<WithStatic>();
    std::string sig_str = sig.c_str();
    
    BOOST_CHECK(sig_str.find("instance_member") != std::string::npos);
    // static_member should NOT be in signature
    BOOST_CHECK(sig_str.find("static_member") == std::string::npos);
}

BOOST_AUTO_TEST_SUITE_END()

//=============================================================================
// STL Type Tests (May not be reflectable or portable)
//=============================================================================

BOOST_AUTO_TEST_SUITE(STLTypes)

BOOST_AUTO_TEST_CASE(optional_may_be_reflectable)
{
    // std::optional has implementation-defined layout
    using OptInt = std::optional<int32_t>;
    // May or may not be reflectable depending on implementation
    BOOST_CHECK(sizeof(OptInt) >= sizeof(int32_t));
}

BOOST_AUTO_TEST_CASE(variant_may_be_reflectable)
{
    using IntFloat = std::variant<int32_t, float>;
    // May or may not be reflectable depending on implementation
    BOOST_CHECK(sizeof(IntFloat) >= 4);
}

BOOST_AUTO_TEST_CASE(vector_not_portable)
{
    // std::vector layout is implementation-defined
    struct WithVector { std::vector<int> v; };
    BOOST_CHECK(!portable_layout<WithVector>);
}

BOOST_AUTO_TEST_SUITE_END()

//=============================================================================
// Concept Composition Tests
//=============================================================================

BOOST_AUTO_TEST_SUITE(ConceptComposition)

BOOST_AUTO_TEST_CASE(portable_implies_reflectable)
{
    // If a type is portable, it should also be reflectable
    static_assert(portable_layout<int32_t> == true);
    static_assert(reflectable<int32_t> == true);
    BOOST_CHECK(true);
}

BOOST_AUTO_TEST_CASE(reflectable_not_implies_portable)
{
    // A type can be reflectable but not portable
    static_assert(reflectable<long> == true);
    static_assert(portable_layout<long> == false);
    BOOST_CHECK(true);
}

BOOST_AUTO_TEST_SUITE_END()
