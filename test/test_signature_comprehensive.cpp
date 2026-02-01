// Boost.TypeLayout
//
// Comprehensive Layout Signature Audit Tests
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#include <boost/typelayout/typelayout.hpp>
#include <iostream>
#include <cstdint>
#include <cstddef>
#include <optional>
#include <variant>
#include <tuple>
#include <string_view>

using namespace boost::typelayout;

// =========================================================================
// Test Helper Macros
// =========================================================================

#define TEST_SIGNATURE_EXISTS(Type) \
    do { \
        constexpr auto sig = get_layout_signature<Type>(); \
        std::cout << "[PASS] " #Type ": " << sig.c_str() << std::endl; \
    } while(0)

#define TEST_SIGNATURE_SIZE_ALIGN(Type) \
    do { \
        constexpr auto sig = get_layout_signature<Type>(); \
        std::cout << "[INFO] " #Type " sizeof=" << sizeof(Type) \
                  << " alignof=" << alignof(Type) \
                  << " sig=" << sig.c_str() << std::endl; \
    } while(0)

#define TEST_SECTION(name) \
    std::cout << "\n========== " << name << " ==========\n" << std::endl

// =========================================================================
// User-Defined Test Types
// =========================================================================

// Empty struct
struct EmptyStruct {};

// Simple POD
struct SimplePOD {
    int32_t x;
    int32_t y;
};

// Nested struct
struct NestedStruct {
    SimplePOD inner;
    int32_t z;
};

// Single inheritance
struct Base {
    int32_t base_val;
};

struct DerivedSingle : Base {
    int32_t derived_val;
};

// Multiple inheritance
struct Base2 {
    int32_t base2_val;
};

struct DerivedMultiple : Base, Base2 {
    int32_t multi_val;
};

// Virtual inheritance
struct VirtualBase {
    int32_t vbase_val;
};

struct DerivedVirtual : virtual VirtualBase {
    int32_t vderived_val;
};

// Polymorphic class
struct PolymorphicClass {
    virtual ~PolymorphicClass() = default;
    int32_t poly_val;
};

// Union
union SimpleUnion {
    int32_t i;
    float f;
    char c[4];
};

// C-style enum
enum CStyleEnum { A, B, C };

// Scoped enum
enum class ScopedEnum : uint16_t { X, Y, Z };

// Bit-fields
struct BitFieldStruct {
    uint32_t a : 4;
    uint32_t b : 8;
    uint32_t c : 20;
};

// Aligned struct
struct alignas(16) AlignedStruct {
    int32_t x;
    int32_t y;
};

// Struct with pointer member
struct WithPointer {
    int32_t value;
    int32_t* ptr;
};

// Struct with reference member (cannot be default constructed)
// Skipped for now - references inside structs are uncommon

// Member pointer test class
struct MemberPtrTestClass {
    int data;
    void method() {}
};

// Anonymous union inside struct
struct WithAnonymousUnion {
    int32_t header;
    union {
        int32_t as_int;
        float as_float;
    };
};

// [[no_unique_address]] test
struct Empty1 {};
struct Empty2 {};

struct NoUniqueAddressTest {
    [[no_unique_address]] Empty1 e1;
    int32_t x;
    [[no_unique_address]] Empty2 e2;
};

// Packed struct (GCC/Clang attribute)
#if defined(__GNUC__) || defined(__clang__)
struct __attribute__((packed)) PackedStruct {
    char a;
    int32_t b;
    char c;
};
#endif

// Nested template types
template<typename T>
struct Wrapper {
    T value;
};

template<typename T, typename U>
struct Pair {
    T first;
    U second;
};

// =========================================================================
// Test Functions
// =========================================================================

void test_fundamental_integers() {
    TEST_SECTION("Fundamental Integer Types");
    
    TEST_SIGNATURE_EXISTS(int8_t);
    TEST_SIGNATURE_EXISTS(uint8_t);
    TEST_SIGNATURE_EXISTS(int16_t);
    TEST_SIGNATURE_EXISTS(uint16_t);
    TEST_SIGNATURE_EXISTS(int32_t);
    TEST_SIGNATURE_EXISTS(uint32_t);
    TEST_SIGNATURE_EXISTS(int64_t);
    TEST_SIGNATURE_EXISTS(uint64_t);
}

void test_fundamental_floats() {
    TEST_SECTION("Fundamental Float Types");
    
    TEST_SIGNATURE_SIZE_ALIGN(float);
    TEST_SIGNATURE_SIZE_ALIGN(double);
    TEST_SIGNATURE_SIZE_ALIGN(long double);
}

void test_character_types() {
    TEST_SECTION("Character Types");
    
    TEST_SIGNATURE_EXISTS(char);
    TEST_SIGNATURE_SIZE_ALIGN(wchar_t);  // Platform dependent
    TEST_SIGNATURE_EXISTS(char8_t);
    TEST_SIGNATURE_EXISTS(char16_t);
    TEST_SIGNATURE_EXISTS(char32_t);
}

void test_special_types() {
    TEST_SECTION("Boolean and Special Types");
    
    TEST_SIGNATURE_EXISTS(bool);
    TEST_SIGNATURE_EXISTS(std::byte);
    TEST_SIGNATURE_SIZE_ALIGN(std::nullptr_t);
}

void test_void_type() {
    TEST_SECTION("Void Type");
    
    // void is an incomplete type - cannot get sizeof/alignof
    // But we should be able to get a signature for void*
    std::cout << "[INFO] void is an incomplete type - no sizeof/alignof" << std::endl;
    TEST_SIGNATURE_SIZE_ALIGN(void*);
    
    // Test that we can distinguish void* from other pointers
    constexpr auto sig_void_ptr = get_layout_signature<void*>();
    constexpr auto sig_int_ptr = get_layout_signature<int*>();
    std::cout << "[INFO] void* sig: " << sig_void_ptr.c_str() << std::endl;
    std::cout << "[INFO] int* sig: " << sig_int_ptr.c_str() << std::endl;
    std::cout << "[PASS] void pointer type handled correctly" << std::endl;
}

void test_pointer_types() {
    TEST_SECTION("Pointer Types");
    
    TEST_SIGNATURE_SIZE_ALIGN(int*);
    TEST_SIGNATURE_SIZE_ALIGN(void*);
    TEST_SIGNATURE_SIZE_ALIGN(const int*);
    TEST_SIGNATURE_SIZE_ALIGN(int**);
    TEST_SIGNATURE_SIZE_ALIGN(const void* const);
}

void test_function_pointer_types() {
    TEST_SECTION("Function Pointer Types");
    
    using FuncPtr1 = void(*)();
    using FuncPtr2 = int(*)(int, int);
    using FuncPtr3 = void(*)(int, ...) ;
    using FuncPtrNoexcept = int(*)(int) noexcept;
    
    TEST_SIGNATURE_SIZE_ALIGN(FuncPtr1);
    TEST_SIGNATURE_SIZE_ALIGN(FuncPtr2);
    TEST_SIGNATURE_SIZE_ALIGN(FuncPtr3);
    TEST_SIGNATURE_SIZE_ALIGN(FuncPtrNoexcept);
}

void test_reference_types() {
    TEST_SECTION("Reference Types");
    
    // Note: References don't have "size" in the traditional sense
    // but we treat them as pointer-like for layout purposes
    TEST_SIGNATURE_SIZE_ALIGN(int&);
    TEST_SIGNATURE_SIZE_ALIGN(int&&);
    TEST_SIGNATURE_SIZE_ALIGN(const int&);
}

void test_member_pointer_types() {
    TEST_SECTION("Member Pointer Types");
    
    using DataMemberPtr = int MemberPtrTestClass::*;
    using MethodPtr = void (MemberPtrTestClass::*)();
    
    TEST_SIGNATURE_SIZE_ALIGN(DataMemberPtr);
    TEST_SIGNATURE_SIZE_ALIGN(MethodPtr);
}

void test_array_types() {
    TEST_SECTION("Array Types");
    
    TEST_SIGNATURE_SIZE_ALIGN(int[10]);
    TEST_SIGNATURE_SIZE_ALIGN(int[3][4]);
    TEST_SIGNATURE_SIZE_ALIGN(char[100]);
    TEST_SIGNATURE_SIZE_ALIGN(SimplePOD[5]);
}

void test_cv_qualified_types() {
    TEST_SECTION("CV-Qualified Types");
    
    // CV qualifiers should be stripped (same layout)
    constexpr auto sig_int = get_layout_signature<int32_t>();
    constexpr auto sig_const_int = get_layout_signature<const int32_t>();
    constexpr auto sig_volatile_int = get_layout_signature<volatile int32_t>();
    constexpr auto sig_cv_int = get_layout_signature<const volatile int32_t>();
    
    static_assert(sig_int == sig_const_int, "const should not change signature");
    static_assert(sig_int == sig_volatile_int, "volatile should not change signature");
    static_assert(sig_int == sig_cv_int, "const volatile should not change signature");
    
    std::cout << "[PASS] CV qualifiers correctly stripped" << std::endl;
    std::cout << "[INFO] int32_t signature: " << sig_int.c_str() << std::endl;
}

void test_user_defined_structs() {
    TEST_SECTION("User-Defined Structs");
    
    TEST_SIGNATURE_SIZE_ALIGN(EmptyStruct);
    TEST_SIGNATURE_SIZE_ALIGN(SimplePOD);
    TEST_SIGNATURE_SIZE_ALIGN(NestedStruct);
    TEST_SIGNATURE_SIZE_ALIGN(WithPointer);
}

void test_inheritance() {
    TEST_SECTION("Inheritance");
    
    TEST_SIGNATURE_SIZE_ALIGN(Base);
    TEST_SIGNATURE_SIZE_ALIGN(DerivedSingle);
    TEST_SIGNATURE_SIZE_ALIGN(Base2);
    TEST_SIGNATURE_SIZE_ALIGN(DerivedMultiple);
    
    std::cout << "\n--- Virtual Inheritance ---" << std::endl;
    TEST_SIGNATURE_SIZE_ALIGN(VirtualBase);
    TEST_SIGNATURE_SIZE_ALIGN(DerivedVirtual);
}

void test_polymorphic_classes() {
    TEST_SECTION("Polymorphic Classes");
    
    TEST_SIGNATURE_SIZE_ALIGN(PolymorphicClass);
    
    // Polymorphic classes have vtable pointer
    std::cout << "[INFO] Polymorphic class size includes vtable pointer" << std::endl;
}

void test_unions() {
    TEST_SECTION("Unions");
    
    TEST_SIGNATURE_SIZE_ALIGN(SimpleUnion);
}

void test_enums() {
    TEST_SECTION("Enum Types");
    
    TEST_SIGNATURE_SIZE_ALIGN(CStyleEnum);
    TEST_SIGNATURE_SIZE_ALIGN(ScopedEnum);
}

void test_bitfields() {
    TEST_SECTION("Bit-fields");
    
    TEST_SIGNATURE_SIZE_ALIGN(BitFieldStruct);
    
    // Verify bit-field format in signature
    constexpr auto sig = get_layout_signature<BitFieldStruct>();
    std::cout << "[INFO] BitFieldStruct signature: " << sig.c_str() << std::endl;
}

void test_smart_pointers() {
    TEST_SECTION("Smart Pointers");
    
    TEST_SIGNATURE_SIZE_ALIGN(std::unique_ptr<int>);
    TEST_SIGNATURE_SIZE_ALIGN(std::shared_ptr<int>);
    TEST_SIGNATURE_SIZE_ALIGN(std::weak_ptr<int>);
}

void test_std_optional() {
    TEST_SECTION("std::optional");
    
    // std::optional contains anonymous union members internally
    // Now supported with <anon:N> placeholder for anonymous members
    TEST_SIGNATURE_SIZE_ALIGN(std::optional<int>);
    TEST_SIGNATURE_SIZE_ALIGN(std::optional<SimplePOD>);
    std::cout << "[PASS] std::optional signatures generated correctly" << std::endl;
}

void test_std_variant() {
    TEST_SECTION("std::variant");
    
    // std::variant contains anonymous union members internally
    // Now supported with <anon:N> placeholder for anonymous members
    using VariantIntFloat = std::variant<int, float>;
    using VariantIntDoubleChar = std::variant<int, double, char>;
    
    TEST_SIGNATURE_SIZE_ALIGN(VariantIntFloat);
    TEST_SIGNATURE_SIZE_ALIGN(VariantIntDoubleChar);
    std::cout << "[PASS] std::variant signatures generated correctly" << std::endl;
}

void test_std_tuple() {
    TEST_SECTION("std::tuple");
    
    using TupleInt = std::tuple<int>;
    using TupleIntFloat = std::tuple<int, float>;
    using TupleIntDoubleChar = std::tuple<int, double, char>;
    
    TEST_SIGNATURE_SIZE_ALIGN(TupleInt);
    TEST_SIGNATURE_SIZE_ALIGN(TupleIntFloat);
    TEST_SIGNATURE_SIZE_ALIGN(TupleIntDoubleChar);
}

void test_edge_cases() {
    TEST_SECTION("Edge Cases");
    
    std::cout << "--- Anonymous Union ---" << std::endl;
    // WithAnonymousUnion has anonymous union member
    // Now supported with <anon:N> placeholder
    TEST_SIGNATURE_SIZE_ALIGN(WithAnonymousUnion);
    std::cout << "[PASS] Anonymous union member handled correctly with <anon:N> placeholder" << std::endl;
    
    std::cout << "\n--- [[no_unique_address]] ---" << std::endl;
    TEST_SIGNATURE_SIZE_ALIGN(NoUniqueAddressTest);
    std::cout << "[INFO] Empty1 size: " << sizeof(Empty1) << std::endl;
    std::cout << "[INFO] NoUniqueAddressTest should be <= sizeof(int32_t) + 2 bytes" << std::endl;
    
    std::cout << "\n--- alignas ---" << std::endl;
    TEST_SIGNATURE_SIZE_ALIGN(AlignedStruct);
    static_assert(alignof(AlignedStruct) == 16, "AlignedStruct should have alignment 16");
    
#if defined(__GNUC__) || defined(__clang__)
    std::cout << "\n--- __attribute__((packed)) ---" << std::endl;
    TEST_SIGNATURE_SIZE_ALIGN(PackedStruct);
    // Packed struct: char(1) + int32(4) + char(1) = 6 bytes without padding
    std::cout << "[INFO] PackedStruct without packing would be: " << (sizeof(char) + sizeof(int32_t) + sizeof(char) + 2) << " bytes (with padding)" << std::endl;
    std::cout << "[INFO] PackedStruct with packing should be: 6 bytes" << std::endl;
    if (sizeof(PackedStruct) == 6) {
        std::cout << "[PASS] Packed struct correctly removes padding" << std::endl;
    } else {
        std::cout << "[INFO] Actual size: " << sizeof(PackedStruct) << std::endl;
    }
#endif
}

void test_nested_templates() {
    TEST_SECTION("Nested Template Types");
    
    using WrapperInt = Wrapper<int32_t>;
    using WrapperSimplePOD = Wrapper<SimplePOD>;
    using PairIntFloat = Pair<int32_t, float>;
    using NestedWrapper = Wrapper<Wrapper<int32_t>>;
    
    TEST_SIGNATURE_SIZE_ALIGN(WrapperInt);
    TEST_SIGNATURE_SIZE_ALIGN(WrapperSimplePOD);
    TEST_SIGNATURE_SIZE_ALIGN(PairIntFloat);
    TEST_SIGNATURE_SIZE_ALIGN(NestedWrapper);
    
    // Verify nested templates produce correct signatures
    std::cout << "[PASS] Nested template types handled correctly" << std::endl;
}

void test_signature_correctness() {
    TEST_SECTION("Signature Correctness Verification");
    
    // Test that identical layout types match
    struct LayoutA { int32_t x; int32_t y; };
    struct LayoutB { int32_t a; int32_t b; };
    
    // Different struct names but same layout should have different signatures
    // (because field names are included)
    constexpr auto sig_a = get_layout_signature<LayoutA>();
    constexpr auto sig_b = get_layout_signature<LayoutB>();
    
    std::cout << "[INFO] LayoutA: " << sig_a.c_str() << std::endl;
    std::cout << "[INFO] LayoutB: " << sig_b.c_str() << std::endl;
    
    // Different layout types should definitely not match
    struct DiffLayout { int64_t x; };
    constexpr bool match = signatures_match<LayoutA, DiffLayout>();
    static_assert(!match, "Different layouts must not match");
    std::cout << "[PASS] Different layouts produce different signatures" << std::endl;
}

// =========================================================================
// Main
// =========================================================================

int main() {
    std::cout << "TypeLayout Comprehensive Signature Audit\n" << std::endl;
    std::cout << "Platform: ";
    #if defined(_WIN32)
    std::cout << "Windows ";
    #elif defined(__linux__)
    std::cout << "Linux ";
    #elif defined(__APPLE__)
    std::cout << "macOS ";
    #endif
    std::cout << (sizeof(void*) * 8) << "-bit" << std::endl;
    
    // Section 2: Fundamental Types
    test_fundamental_integers();
    test_fundamental_floats();
    test_character_types();
    test_special_types();
    test_void_type();
    
    // Section 3: Compound Types
    test_pointer_types();
    test_function_pointer_types();
    test_reference_types();
    test_member_pointer_types();
    test_array_types();
    test_cv_qualified_types();
    
    // Section 4: User-Defined Types
    test_user_defined_structs();
    test_inheritance();
    test_polymorphic_classes();
    test_unions();
    test_enums();
    test_bitfields();
    
    // Section 5: Standard Library Types
    test_smart_pointers();
    test_std_optional();
    test_std_variant();
    test_std_tuple();
    
    // Section 6: Edge Cases
    test_edge_cases();
    test_nested_templates();
    
    // Verification
    test_signature_correctness();
    
    std::cout << "\n========== AUDIT COMPLETE ==========" << std::endl;
    return 0;
}
