// Test: compat::classify_signature -- runtime safety classification
//
// Validates that classify_signature() correctly classifies signature
// strings into SafetyLevel categories.  Also validates that
// layout_traits<T>::has_padding / has_pointer / has_opaque give
// correct results for various type categories.
//
// Requires P2996 (Bloomberg Clang).

#include <boost/typelayout/typelayout.hpp>
#include <boost/typelayout/tools/safety_level.hpp>
#include <boost/typelayout/opaque.hpp>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <string_view>

using namespace boost::typelayout;
using namespace boost::typelayout::detail;

// =========================================================================
// Test types
// =========================================================================

struct TrivialPair {
    int32_t x;
    int32_t y;
};

struct WithPointer {
    int32_t value;
    int32_t* ptr;
};

struct WithFnPtr {
    void (*callback)(int);
};

struct PaddedStruct {
    int8_t  a;
    // 3 bytes padding (alignment of int32_t)
    int32_t b;
};

// -- Opaque type for recursive detection testing
struct OpaqueData {
    char raw[32];
};

namespace boost { namespace typelayout {
TYPELAYOUT_REGISTER_OPAQUE(OpaqueData, "opaque_data", false)
}} // namespace boost::typelayout

struct ContainsOpaque {
    int32_t id;
    OpaqueData blob;
};

struct ArrayOfOpaque {
    int32_t count;
    OpaqueData arr[3];
};

struct ArrayOfPadded {
    PaddedStruct items[2];
};

struct WithWchar {
    wchar_t ch;
};

struct Nested {
    TrivialPair pair;
    int32_t     extra;
};

struct NestedWithPointer {
    TrivialPair pair;
    int32_t*    p;
};

struct Empty {};

struct SingleField {
    uint64_t val;
};

// =========================================================================
// 1. layout_traits boolean properties (static_assert)
// =========================================================================

// --- has_padding ---
static_assert(!layout_traits<int32_t>::has_padding,
    "int32_t has no padding");
static_assert(!layout_traits<TrivialPair>::has_padding,
    "TrivialPair (two int32_t) has no padding");
static_assert(layout_traits<PaddedStruct>::has_padding,
    "PaddedStruct has alignment padding");
static_assert(layout_traits<ArrayOfPadded>::has_padding,
    "ArrayOfPadded: array element has padding");
static_assert(!layout_traits<Empty>::has_padding,
    "Empty struct has no padding");
static_assert(!layout_traits<Nested>::has_padding,
    "Nested (TrivialPair + int32_t) has no padding");

// --- has_pointer ---
static_assert(!layout_traits<TrivialPair>::has_pointer,
    "TrivialPair has no pointer");
static_assert(layout_traits<WithPointer>::has_pointer,
    "WithPointer contains a pointer");
static_assert(layout_traits<WithFnPtr>::has_pointer,
    "WithFnPtr contains a function pointer");
static_assert(layout_traits<NestedWithPointer>::has_pointer,
    "NestedWithPointer contains a pointer");
static_assert(!layout_traits<int32_t>::has_pointer,
    "int32_t has no pointer");

// --- has_opaque ---
static_assert(layout_traits<ContainsOpaque>::has_opaque,
    "ContainsOpaque contains an opaque member");
static_assert(layout_traits<ArrayOfOpaque>::has_opaque,
    "ArrayOfOpaque: array element is opaque");
static_assert(!layout_traits<TrivialPair>::has_opaque,
    "TrivialPair is not opaque");

// =========================================================================
// 2. classify_signature runtime tests
// =========================================================================

void test_classify_signature() {
    // TrivialSafe: fixed-width scalars, no padding, no pointer
    assert(classify_signature(std::string_view(
        get_layout_signature<int32_t>()))
        == SafetyLevel::TrivialSafe);

    assert(classify_signature(std::string_view(
        get_layout_signature<TrivialPair>()))
        == SafetyLevel::TrivialSafe);

    assert(classify_signature(std::string_view(
        get_layout_signature<Nested>()))
        == SafetyLevel::TrivialSafe);

    assert(classify_signature(std::string_view(
        get_layout_signature<SingleField>()))
        == SafetyLevel::TrivialSafe);

    assert(classify_signature(std::string_view(
        get_layout_signature<Empty>()))
        == SafetyLevel::TrivialSafe);

    // PaddingRisk: has alignment padding
    assert(classify_signature(std::string_view(
        get_layout_signature<PaddedStruct>()))
        == SafetyLevel::PaddingRisk);

    assert(classify_signature(std::string_view(
        get_layout_signature<ArrayOfPadded>()))
        == SafetyLevel::PaddingRisk);

    // PointerRisk: contains pointer
    assert(classify_signature(std::string_view(
        get_layout_signature<WithPointer>()))
        == SafetyLevel::PointerRisk);

    assert(classify_signature(std::string_view(
        get_layout_signature<WithFnPtr>()))
        == SafetyLevel::PointerRisk);

    assert(classify_signature(std::string_view(
        get_layout_signature<NestedWithPointer>()))
        == SafetyLevel::PointerRisk);

    assert(classify_signature(std::string_view(
        get_layout_signature<int32_t*>()))
        == SafetyLevel::PointerRisk);

    // PlatformVariant: wchar_t
    assert(classify_signature(std::string_view(
        get_layout_signature<WithWchar>()))
        == SafetyLevel::PlatformVariant);

    // PlatformVariant: long double
    assert(classify_signature(std::string_view(
        get_layout_signature<long double>()))
        == SafetyLevel::PlatformVariant);

    // Opaque
    assert(classify_signature(std::string_view(
        get_layout_signature<ContainsOpaque>()))
        == SafetyLevel::Opaque);

    assert(classify_signature(std::string_view(
        get_layout_signature<ArrayOfOpaque>()))
        == SafetyLevel::Opaque);

    std::cout << "  [PASS] classify_signature (16 runtime assertions)\n";
}

// =========================================================================
// 3. safety_level_name
// =========================================================================

void test_safety_level_name() {
    assert(safety_level_name(SafetyLevel::TrivialSafe)[0] == 'T');
    assert(safety_level_name(SafetyLevel::PaddingRisk)[0] == 'P');
    assert(safety_level_name(SafetyLevel::PointerRisk)[0] == 'P');
    assert(safety_level_name(SafetyLevel::PlatformVariant)[0] == 'P');
    assert(safety_level_name(SafetyLevel::Opaque)[0] == 'O');
    std::cout << "  [PASS] safety_level_name\n";
}

// =========================================================================
// Runtime output for human verification
// =========================================================================

template <typename T>
void print_traits(const char* name) {
    constexpr auto sig = get_layout_signature<T>();
    auto level = classify_signature(std::string_view(sig));
    std::cout << "  " << name << ": "
              << safety_level_name(level)
              << " (padding=" << layout_traits<T>::has_padding
              << ", pointer=" << layout_traits<T>::has_pointer
              << ", opaque=" << layout_traits<T>::has_opaque
              << ")\n";
}

int main() {
    std::cout << "=== test_classify ===\n";

    // Compile-time checks
    std::cout << "  [PASS] Compile-time layout_traits properties "
              << "(16 static_assert checks)\n";

    // Runtime checks
    test_classify_signature();
    test_safety_level_name();

    std::cout << "\nType details:\n";
    print_traits<int32_t>("int32_t");
    print_traits<TrivialPair>("TrivialPair");
    print_traits<PaddedStruct>("PaddedStruct");
    print_traits<WithPointer>("WithPointer");
    print_traits<WithFnPtr>("WithFnPtr");
    print_traits<WithWchar>("WithWchar");
    print_traits<long double>("long double");
    print_traits<ContainsOpaque>("ContainsOpaque");
    print_traits<ArrayOfOpaque>("ArrayOfOpaque");
    print_traits<ArrayOfPadded>("ArrayOfPadded");

    std::cout << "\nAll test_classify tests passed.\n";
    return 0;
}
