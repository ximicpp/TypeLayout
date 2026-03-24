// Byte-copy safety admission tests.
//
// Tests is_byte_copy_safe_v<T> across all decision tree branches:
//   Branch 1: Opaque types (has_opaque_signature)
//   Branch 2: Trivially copyable types with no pointer
//   Branch 3: Recursive class types (!union, !polymorphic)
//   Branch 4: Otherwise false

#include <boost/typelayout/typelayout.hpp>
#include <iostream>
#include <cstdint>

using namespace boost::typelayout;

// =========================================================================
// Test types -- defined inline per testing invariant
// =========================================================================

// -- Branch 2: trivially copyable, no pointer --
struct TrivialSafe {
    int32_t x;
    float   y;
};

struct TrivialWithPtr {
    int32_t x;
    int*    ptr;
};

struct TrivialWithPadding {
    int8_t  a;
    int32_t b;
};

// -- Branch 4: polymorphic --
struct Polymorphic {
    virtual void f();
    int32_t x;
};

// -- Union types --
union PodUnion {
    int32_t a;
    float   b;
};

// -- Inheritance --
struct SafeBase {
    int32_t x;
};

struct UnsafeBase {
    int32_t x;
    void*   p;
};

struct DerivedFromSafe : SafeBase {
    int32_t y;
};

struct DerivedFromUnsafe : UnsafeBase {
    int32_t y;
};

// -- Opaque test types --

// Simulate offset_ptr-based container (non-trivially-copyable, but byte-copy safe)
struct FakeXString {
    char data_[32];
    FakeXString() {}
    ~FakeXString() {}
};

template <typename T>
struct FakeXVector {
    char storage_[24];
    FakeXVector() {}
    ~FakeXVector() {}
};

template <typename K, typename V>
struct FakeXMap {
    char storage_[48];
    FakeXMap() {}
    ~FakeXMap() {}
};

// Register opaque types in boost::typelayout namespace
namespace boost { namespace typelayout {

TYPELAYOUT_OPAQUE_TYPE_RELOCATABLE(FakeXString, "fxstr")
TYPELAYOUT_OPAQUE_CONTAINER_RELOCATABLE(FakeXVector, "fxvec")
TYPELAYOUT_OPAQUE_MAP_RELOCATABLE(FakeXMap, "fxmap")

}} // namespace boost::typelayout

// -- Struct containing opaque members (non-trivially-copyable at top level) --
struct MessageWithOpaque {
    int32_t     id;
    FakeXString name;
};

struct MessageWithOpaqueContainer {
    int32_t              id;
    FakeXVector<int32_t> items;
};

struct MessageWithOpaqueMap {
    int32_t                       id;
    FakeXMap<int32_t, TrivialSafe> entries;
};

// -- Opaque container with unsafe element --
struct UnsafeElement {
    int32_t x;
    void*   p;
};

struct MessageWithUnsafeContainer {
    int32_t                   id;
    FakeXVector<UnsafeElement> items;
};

struct MessageWithUnsafeMapValue {
    int32_t                             id;
    FakeXMap<int32_t, UnsafeElement>    entries;
};

// -- Nested struct recursion --
struct Inner {
    int32_t     a;
    FakeXString s;
};

struct Outer {
    int32_t id;
    Inner   inner;
};

// -- Opaque type with pointer (W1: REGISTER_OPAQUE with HasPointer=true) --
struct OpaqueWithPtr {
    char data_[16];
};

namespace boost { namespace typelayout {
TYPELAYOUT_REGISTER_OPAQUE(OpaqueWithPtr, "owp", true)
}} // namespace boost::typelayout

// -- Array types --
struct SafeForArray {
    int32_t x;
    int32_t y;
};

// =========================================================================
// Compile-time verification (static_assert preferred)
// =========================================================================

// 5.2: Trivially copyable safe type -> true
static_assert(is_byte_copy_safe_v<TrivialSafe>,
    "TrivialSafe should be byte-copy safe");
static_assert(is_byte_copy_safe_v<int32_t>,
    "int32_t should be byte-copy safe");
static_assert(is_byte_copy_safe_v<TrivialWithPadding>,
    "TrivialWithPadding has padding but is still byte-copy safe");

// 5.3: Type with pointer member -> false
static_assert(!is_byte_copy_safe_v<TrivialWithPtr>,
    "TrivialWithPtr contains a pointer, not byte-copy safe");

// 5.4: Polymorphic type -> false
static_assert(!is_byte_copy_safe_v<Polymorphic>,
    "Polymorphic types have vptr, not byte-copy safe");

// 5.5: POD union -> true, non-trivial union -> false
static_assert(is_byte_copy_safe_v<PodUnion>,
    "POD union is trivially copyable, byte-copy safe");

// 5.6: Struct with registered relocatable opaque member -> true
static_assert(is_byte_copy_safe_v<FakeXString>,
    "Registered relocatable opaque type should be byte-copy safe");
static_assert(is_byte_copy_safe_v<MessageWithOpaque>,
    "Struct with opaque members should be byte-copy safe (recursive check)");

// 5.7: Opaque container with safe element -> true, unsafe element -> false
static_assert(is_byte_copy_safe_v<FakeXVector<int32_t>>,
    "Opaque container with safe element should be byte-copy safe");
static_assert(is_byte_copy_safe_v<MessageWithOpaqueContainer>,
    "Struct with opaque container (safe element) should be byte-copy safe");
static_assert(!is_byte_copy_safe_v<FakeXVector<UnsafeElement>>,
    "Opaque container with unsafe element should NOT be byte-copy safe");
static_assert(!is_byte_copy_safe_v<MessageWithUnsafeContainer>,
    "Struct with opaque container (unsafe element) should NOT be byte-copy safe");

// 5.8: Opaque map with safe key+value -> true, unsafe value -> false
static_assert(is_byte_copy_safe_v<FakeXMap<int32_t, TrivialSafe>>,
    "Opaque map with safe key+value should be byte-copy safe");
static_assert(is_byte_copy_safe_v<MessageWithOpaqueMap>,
    "Struct with opaque map (safe k+v) should be byte-copy safe");
static_assert(!is_byte_copy_safe_v<FakeXMap<int32_t, UnsafeElement>>,
    "Opaque map with unsafe value should NOT be byte-copy safe");
static_assert(!is_byte_copy_safe_v<MessageWithUnsafeMapValue>,
    "Struct with opaque map (unsafe value) should NOT be byte-copy safe");

// 5.9: Nested struct recursion
static_assert(is_byte_copy_safe_v<Inner>,
    "Inner struct with opaque member should be byte-copy safe");
static_assert(is_byte_copy_safe_v<Outer>,
    "Outer struct containing Inner should be byte-copy safe (deep recursion)");

// 5.10: Inherited base with unsafe member -> false
static_assert(is_byte_copy_safe_v<DerivedFromSafe>,
    "Derived from safe base should be byte-copy safe");
static_assert(!is_byte_copy_safe_v<DerivedFromUnsafe>,
    "Derived from unsafe base (has pointer) should NOT be byte-copy safe");

// 5.11: Array of safe elements -> true, array of unsafe elements -> false
static_assert(is_byte_copy_safe_v<SafeForArray[10]>,
    "Array of safe elements should be byte-copy safe");
static_assert(is_byte_copy_safe_v<int32_t[5]>,
    "Array of int32_t should be byte-copy safe");
static_assert(!is_byte_copy_safe_v<UnsafeElement[3]>,
    "Array of unsafe elements should NOT be byte-copy safe");

// W1: Opaque type registered with HasPointer=true -> false
static_assert(!is_byte_copy_safe_v<OpaqueWithPtr>,
    "Opaque type with HasPointer=true should NOT be byte-copy safe");

// W2: Nested container -- FakeXVector<FakeXVector<int32_t>> -> true
static_assert(is_byte_copy_safe_v<FakeXVector<FakeXVector<int32_t>>>,
    "Nested opaque container with safe inner element should be byte-copy safe");
static_assert(!is_byte_copy_safe_v<FakeXVector<FakeXVector<UnsafeElement>>>,
    "Nested opaque container with unsafe inner element should NOT be byte-copy safe");

// S3: REGISTER_OPAQUE generates opaque_copy_safe = true
static_assert(opaque_copy_safe<OpaqueWithPtr>::value,
    "REGISTER_OPAQUE should generate opaque_copy_safe = true");

// 5.12: Trivially copyable safe vs opaque-based safe
static_assert(std::is_trivially_copyable_v<TrivialSafe>,
    "TrivialSafe is trivially copyable");
static_assert(is_byte_copy_safe_v<TrivialSafe>,
    "TrivialSafe is byte-copy safe");

static_assert(!std::is_trivially_copyable_v<MessageWithOpaque>,
    "MessageWithOpaque is NOT trivially copyable");
static_assert(is_byte_copy_safe_v<MessageWithOpaque>,
    "MessageWithOpaque is byte-copy safe (via opaque recursion)");

// =========================================================================
// Runtime output for CI visibility
// =========================================================================

int main() {
    std::cout << "=== test_byte_copy_safe ===" << "\n";

    std::cout << "TrivialSafe:             byte_copy_safe = "
              << is_byte_copy_safe_v<TrivialSafe> << "\n";
    std::cout << "TrivialWithPtr:          byte_copy_safe = "
              << is_byte_copy_safe_v<TrivialWithPtr> << "\n";
    std::cout << "Polymorphic:             byte_copy_safe = "
              << is_byte_copy_safe_v<Polymorphic> << "\n";
    std::cout << "PodUnion:                byte_copy_safe = "
              << is_byte_copy_safe_v<PodUnion> << "\n";
    std::cout << "FakeXString:             byte_copy_safe = "
              << is_byte_copy_safe_v<FakeXString> << "\n";
    std::cout << "MessageWithOpaque:       byte_copy_safe = "
              << is_byte_copy_safe_v<MessageWithOpaque> << "\n";
    std::cout << "FakeXVector<int32_t>:    byte_copy_safe = "
              << is_byte_copy_safe_v<FakeXVector<int32_t>> << "\n";
    std::cout << "FakeXVector<Unsafe>:     byte_copy_safe = "
              << is_byte_copy_safe_v<FakeXVector<UnsafeElement>> << "\n";
    std::cout << "FakeXMap<i32,Trivial>:   byte_copy_safe = "
              << is_byte_copy_safe_v<FakeXMap<int32_t, TrivialSafe>> << "\n";
    std::cout << "FakeXMap<i32,Unsafe>:    byte_copy_safe = "
              << is_byte_copy_safe_v<FakeXMap<int32_t, UnsafeElement>> << "\n";
    std::cout << "Outer (nested):          byte_copy_safe = "
              << is_byte_copy_safe_v<Outer> << "\n";
    std::cout << "DerivedFromSafe:         byte_copy_safe = "
              << is_byte_copy_safe_v<DerivedFromSafe> << "\n";
    std::cout << "DerivedFromUnsafe:       byte_copy_safe = "
              << is_byte_copy_safe_v<DerivedFromUnsafe> << "\n";
    std::cout << "SafeForArray[10]:        byte_copy_safe = "
              << is_byte_copy_safe_v<SafeForArray[10]> << "\n";
    std::cout << "UnsafeElement[3]:        byte_copy_safe = "
              << is_byte_copy_safe_v<UnsafeElement[3]> << "\n";
    std::cout << "OpaqueWithPtr:           byte_copy_safe = "
              << is_byte_copy_safe_v<OpaqueWithPtr> << "\n";
    std::cout << "FakeXVector<FakeXVector>: byte_copy_safe = "
              << is_byte_copy_safe_v<FakeXVector<FakeXVector<int32_t>>> << "\n";

    std::cout << "\n";
    std::cout << "Admission check:" << "\n";
    std::cout << "  TrivialSafe:       trivially_copyable=" << std::is_trivially_copyable_v<TrivialSafe>
              << " byte_safe=" << is_byte_copy_safe_v<TrivialSafe> << "\n";
    std::cout << "  MessageWithOpaque: trivially_copyable=" << std::is_trivially_copyable_v<MessageWithOpaque>
              << " byte_safe=" << is_byte_copy_safe_v<MessageWithOpaque> << "\n";

    std::cout << "\nAll checks passed." << "\n";
    return 0;
}