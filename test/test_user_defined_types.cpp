// test_user_defined_types.cpp - Comprehensive User-Defined Types Analysis
// This test validates TypeLayout support for all class/struct variants
//
// Compile with: clang++ -std=c++26 -freflection -fconstexpr-steps=5000000

#include <boost/typelayout/typelayout.hpp>
#include <print>
#include <cstdint>

using namespace boost::typelayout;

#define TEST_TYPE(Type) \
    { \
        constexpr auto sig = get_layout_signature_cstr<Type>(); \
        std::println("{}: {}", #Type, sig); \
        std::println("  sizeof: {}, alignof: {}", sizeof(Type), alignof(Type)); \
    }

#define TEST_SECTION(name) std::println("\n=== {} ===", name)

// ============================================================================
// 1. Class Variants
// ============================================================================

// 1.1 Empty class
struct EmptyClass {};
class EmptyClassC {};

// 1.2 POD type
struct PODType {
    int32_t x;
    float y;
    char c;
};

// 1.3 Standard Layout type
struct StandardLayout {
    int32_t a;
    int32_t b;
    int32_t c;
};

// 1.4 Trivially Copyable type
struct TriviallyCopyable {
    int32_t val;
    TriviallyCopyable() = default;
    TriviallyCopyable(const TriviallyCopyable&) = default;
    TriviallyCopyable& operator=(const TriviallyCopyable&) = default;
};

// 1.5 Non-trivial class (custom constructor/destructor)
class NonTrivialClass {
public:
    NonTrivialClass() : value(0) {}
    ~NonTrivialClass() { /* cleanup */ }
    int32_t value;
};

// 1.6 Abstract class (pure virtual)
class AbstractClass {
public:
    virtual ~AbstractClass() = default;
    virtual void doSomething() = 0;
protected:
    int32_t data;
};

// 1.7 Final class
class FinalClass final {
    int32_t val;
};

void test_class_variants() {
    TEST_SECTION("1. Class Variants");
    
    TEST_TYPE(EmptyClass);
    TEST_TYPE(EmptyClassC);
    TEST_TYPE(PODType);
    TEST_TYPE(StandardLayout);
    TEST_TYPE(TriviallyCopyable);
    TEST_TYPE(NonTrivialClass);
    // AbstractClass cannot be instantiated, but can get signature
    TEST_TYPE(AbstractClass);
    TEST_TYPE(FinalClass);
    
    // Verify type traits
    std::println("\n  Type trait checks:");
    std::println("    PODType is_standard_layout: {}", std::is_standard_layout_v<PODType>);
    std::println("    TriviallyCopyable is_trivially_copyable: {}", std::is_trivially_copyable_v<TriviallyCopyable>);
    std::println("    NonTrivialClass is_trivially_copyable: {}", std::is_trivially_copyable_v<NonTrivialClass>);
    std::println("    AbstractClass is_abstract: {}", std::is_abstract_v<AbstractClass>);
    std::println("    FinalClass is_final: {}", std::is_final_v<FinalClass>);
}

// ============================================================================
// 2. Member Types
// ============================================================================

// 2.1 Static data members (should be excluded)
struct WithStatic {
    int32_t instance_val;
    static int32_t static_val;  // Should NOT appear in signature
    static constexpr int32_t const_static = 42;  // Should NOT appear
};
int32_t WithStatic::static_val = 0;

// 2.2 Const members
struct WithConst {
    const int32_t const_val;  // const member
    int32_t normal_val;
};

// 2.3 Mutable members
struct WithMutable {
    int32_t normal;
    mutable int32_t mutable_val;  // mutable member
};

// 2.4 Reference members
struct WithReference {
    int32_t& ref;
    const double& const_ref;
};

// 2.5 Pointer members
struct WithPointers {
    int32_t* ptr;
    const char* const_ptr;
    void* void_ptr;
};

// 2.6 Array members
struct WithArrays {
    int32_t arr1[4];
    char str[16];
    double matrix[2][3];
};

// 2.7 Bit-field members
struct WithBitfields {
    uint32_t normal;
    uint8_t bf_a : 3;
    uint8_t bf_b : 5;
    uint16_t bf_c : 10;
    uint16_t bf_d : 6;
};

void test_member_types() {
    TEST_SECTION("2. Member Types");
    
    TEST_TYPE(WithStatic);
    std::println("  Note: static members should NOT appear in signature");
    
    TEST_TYPE(WithConst);
    TEST_TYPE(WithMutable);
    TEST_TYPE(WithReference);
    TEST_TYPE(WithPointers);
    TEST_TYPE(WithArrays);
    TEST_TYPE(WithBitfields);
}

// ============================================================================
// 3. Access Control
// ============================================================================

// 3.1 All public
struct AllPublic {
    int32_t pub1;
    int32_t pub2;
};

// 3.2 Protected members
class WithProtected {
protected:
    int32_t prot_val;
public:
    int32_t pub_val;
};

// 3.3 Private members
class WithPrivate {
private:
    int32_t priv1;
    int32_t priv2;
public:
    int32_t pub1;
};

// 3.4 Mixed access levels
class MixedAccess {
public:
    int32_t pub1;
protected:
    int32_t prot1;
private:
    int32_t priv1;
public:
    int32_t pub2;
};

void test_access_control() {
    TEST_SECTION("3. Access Control");
    
    TEST_TYPE(AllPublic);
    TEST_TYPE(WithProtected);
    TEST_TYPE(WithPrivate);
    TEST_TYPE(MixedAccess);
    std::println("  Note: P2996 uses access_context::unchecked() to access all members");
}

// ============================================================================
// 4. Template Types
// ============================================================================

// 4.1 Simple template class
template<typename T>
struct SimpleTemplate {
    T value;
    int32_t count;
};

// 4.2 Template specialization
template<>
struct SimpleTemplate<bool> {
    uint8_t flag;  // Different layout for bool
    int32_t count;
};

// 4.3 Partial specialization
template<typename T>
struct SimpleTemplate<T*> {
    T* ptr;
    size_t size;
};

// 4.4 Variadic template
template<typename... Ts>
struct VariadicWrapper {
    std::tuple<Ts...> data;
};

// 4.5 CRTP pattern
template<typename Derived>
struct CRTPBase {
    int32_t base_val;
    Derived* self() { return static_cast<Derived*>(this); }
};

struct CRTPDerived : CRTPBase<CRTPDerived> {
    int32_t derived_val;
};

void test_template_types() {
    TEST_SECTION("4. Template Types");
    
    TEST_TYPE(SimpleTemplate<int32_t>);
    TEST_TYPE(SimpleTemplate<double>);
    TEST_TYPE(SimpleTemplate<bool>);  // Specialization
    TEST_TYPE(SimpleTemplate<int32_t*>);  // Partial specialization
    
    using Variadic2 = VariadicWrapper<int32_t, double>;
    using Variadic3 = VariadicWrapper<char, int32_t, float>;
    TEST_TYPE(Variadic2);
    TEST_TYPE(Variadic3);
    
    TEST_TYPE(CRTPDerived);
}

// ============================================================================
// 5. Nested Types
// ============================================================================

// 5.1 Nested struct
struct Outer1 {
    struct Inner {
        int32_t x;
        int32_t y;
    };
    Inner nested;
    int32_t outer_val;
};

// 5.2 Nested class
class OuterClass {
public:
    class InnerClass {
    public:
        double val;
    };
    InnerClass inner;
    int32_t outer_val;
};

// 5.3 Nested enum
struct WithNestedEnum {
    enum Status : uint8_t { OK, ERROR, PENDING };
    Status status;
    int32_t code;
};

// 5.4 Nested union
struct WithNestedUnion {
    union Data {
        int32_t i;
        float f;
    };
    Data data;
    uint8_t type_tag;
};

// 5.5 Anonymous nested types
struct WithAnonymous {
    struct {
        int32_t x;
        int32_t y;
    };  // Anonymous struct
    int32_t z;
};

void test_nested_types() {
    TEST_SECTION("5. Nested Types");
    
    TEST_TYPE(Outer1);
    TEST_TYPE(Outer1::Inner);
    TEST_TYPE(OuterClass);
    TEST_TYPE(OuterClass::InnerClass);
    TEST_TYPE(WithNestedEnum);
    TEST_TYPE(WithNestedUnion);
    TEST_TYPE(WithNestedUnion::Data);
    TEST_TYPE(WithAnonymous);
}

// ============================================================================
// 6. Inheritance
// ============================================================================

// 6.1 Single inheritance
struct SingleBase {
    int32_t base_val;
};
struct SingleDerived : SingleBase {
    int32_t derived_val;
};

// 6.2 Multiple inheritance
struct MixinA { int32_t a; };
struct MixinB { int32_t b; };
struct MultiInherit : MixinA, MixinB {
    int32_t own;
};

// 6.3 Virtual inheritance
struct VirtualBase {
    int64_t vb_val;
};
struct VirtualDerived1 : virtual VirtualBase {
    int32_t vd1;
};
struct VirtualDerived2 : virtual VirtualBase {
    int32_t vd2;
};

// 6.4 Diamond inheritance
struct DiamondFinal : VirtualDerived1, VirtualDerived2 {
    int32_t diamond_val;
};

// 6.5 Private/protected inheritance
struct PrivatelyInherited : private SingleBase {
    int32_t derived_val;
};
struct ProtectedInherited : protected SingleBase {
    int32_t derived_val;
};

void test_inheritance() {
    TEST_SECTION("6. Inheritance");
    
    TEST_TYPE(SingleBase);
    TEST_TYPE(SingleDerived);
    TEST_TYPE(MultiInherit);
    TEST_TYPE(VirtualDerived1);
    TEST_TYPE(DiamondFinal);
    TEST_TYPE(PrivatelyInherited);
    TEST_TYPE(ProtectedInherited);
}

// ============================================================================
// 7. Special Cases
// ============================================================================

// 7.1 EBO (Empty Base Optimization)
struct EmptyBase1 {};
struct EBOChild : EmptyBase1 {
    int32_t val;
};

// 7.2 [[no_unique_address]]
struct WithNoUniqueAddr {
    [[no_unique_address]] EmptyBase1 empty;
    int32_t val;
};

// 7.3 alignas specified
struct alignas(16) Aligned16Struct {
    int32_t x;
    int32_t y;
};

struct alignas(64) CacheLineAligned {
    int32_t data[4];
};

// 7.4 Packed struct (GCC/Clang extension)
#if defined(__GNUC__) || defined(__clang__)
struct __attribute__((packed)) PackedStruct {
    char c;
    int32_t i;
    char c2;
};
#else
struct PackedStruct {
    char c;
    int32_t i;
    char c2;
};
#endif

// 7.5 Union member
struct HasUnionMember {
    union {
        int32_t as_int;
        float as_float;
    } value;
    uint8_t type;
};

void test_special_cases() {
    TEST_SECTION("7. Special Cases");
    
    std::println("EBO test:");
    std::println("  sizeof(EmptyBase1) = {}", sizeof(EmptyBase1));
    std::println("  sizeof(EBOChild) = {} (should be 4 with EBO)", sizeof(EBOChild));
    TEST_TYPE(EBOChild);
    
    std::println("\n[[no_unique_address]] test:");
    std::println("  sizeof(WithNoUniqueAddr) = {} (should be 4 with NUA)", sizeof(WithNoUniqueAddr));
    TEST_TYPE(WithNoUniqueAddr);
    
    std::println("\naligns test:");
    std::println("  alignof(Aligned16Struct) = {} (should be 16)", alignof(Aligned16Struct));
    std::println("  alignof(CacheLineAligned) = {} (should be 64)", alignof(CacheLineAligned));
    TEST_TYPE(Aligned16Struct);
    TEST_TYPE(CacheLineAligned);
    
    std::println("\nPacked struct test:");
    std::println("  sizeof(PackedStruct) = {} (should be 6 if packed)", sizeof(PackedStruct));
    TEST_TYPE(PackedStruct);
    
    TEST_TYPE(HasUnionMember);
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::println("=======================================================");
    std::println("TypeLayout User-Defined Types Analysis");
    std::println("=======================================================");
    
    test_class_variants();
    test_member_types();
    test_access_control();
    test_template_types();
    test_nested_types();
    test_inheritance();
    test_special_cases();
    
    std::println("\n=======================================================");
    std::println("Analysis Complete!");
    std::println("=======================================================");
    
    return 0;
}
