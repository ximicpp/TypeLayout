// Transfer safety tests (signature-based, no hashing).
//
// Verifies:
//   1. is_byte_copy_safe           -- compile-time admission (core)
//   2. TYPELAYOUT_REGISTER_OPAQUE  -- opaque type registration with Tag
//   3. is_transfer_safe<T>(sig)    -- runtime cross-endpoint verification
//   4. Exact signature matching    -- no hash collision risk
//   5. Composite types with opaque fields

#include <boost/typelayout/typelayout.hpp>
#include <boost/typelayout/layout_traits.hpp>
#include <boost/typelayout/tools/transfer.hpp>
#include "test_util.hpp"
#include <iostream>
#include <cstdint>

// Runtime test helper macro.
// Prints PASS/FAIL for a boolean condition.
#define TEST(cond, label, passed, failed)                               \
    do {                                                                 \
        if ((cond)) {                                                    \
            std::cout << "  [PASS] " << (label) << "\n";                \
            ++(passed);                                                  \
        } else {                                                         \
            std::cout << "  [FAIL] " << (label) << "\n";                \
            ++(failed);                                                  \
        }                                                                \
    } while (0)

using namespace boost::typelayout;
using boost::typelayout::detail::layout_traits;

// =========================================================================
// Test types
// =========================================================================

namespace sf_test {

// -- Plain POD type: should be byte-copy safe
struct Vec3 {
    float x, y, z;
};

// -- Another layout-compatible POD (same field types/offsets)
struct Vec3Alt {
    float a, b, c;
};

// -- POD with different layout
struct Vec4 {
    float x, y, z, w;
};

// -- Type with pointer: NOT byte-copy safe
struct WithPtr {
    int32_t id;
    void* data;
};

// -- Non-trivially-copyable: NOT byte-copy safe
struct NonTrivial {
    int32_t x;
    NonTrivial(const NonTrivial&) {}  // user-defined copy ctor
    NonTrivial() = default;
};

// -- Opaque type: 32-byte key block, no pointers
struct AesKey256 {
    unsigned char data[32];
};

// -- Opaque type: 64-byte sensor raw data, no pointers
struct SensorRaw {
    char raw[64];
};

// -- Opaque type: another 64-byte type (different tag, same size)
struct ActuatorRaw {
    char raw[64];
};

// -- Opaque type: FFI handle, contains pointers
struct LibHandle {
    void* impl;
    int flags;
};

// -- POD struct for Sensor
struct Sensor {
    char     id;
    int32_t  value;
    double   timestamp;
};

// -- Simple packet for registry test
struct Packet {
    uint32_t seq;
    float x;
    float y;
    float z;
};

// -- Relocatable opaque type (non-trivially-copyable, but byte-copy safe)
struct FakeOffsetStr {
    char data_[32];
    FakeOffsetStr() {}
    ~FakeOffsetStr() {}
};

// -- Struct containing relocatable opaque member
struct MessageWithOffsetStr {
    int32_t       id;
    FakeOffsetStr name;
};

} // namespace sf_test

// =========================================================================
// Register opaque types using the new Tag-based macro
// =========================================================================

namespace boost { namespace typelayout {

// AesKey256: 32-byte key block, no pointers
TYPELAYOUT_REGISTER_OPAQUE(sf_test::AesKey256, "AesKey256", false)

// SensorRaw: 64-byte sensor data, no pointers
TYPELAYOUT_REGISTER_OPAQUE(sf_test::SensorRaw, "SensorRaw", false)

// ActuatorRaw: 64-byte actuator data, no pointers (same size, different tag)
TYPELAYOUT_REGISTER_OPAQUE(sf_test::ActuatorRaw, "ActuatorRaw", false)

// LibHandle: FFI handle, HAS pointers
TYPELAYOUT_REGISTER_OPAQUE(sf_test::LibHandle, "LibHandle", true)

// FakeOffsetStr: relocatable opaque (non-trivially-copyable, byte-copy safe)
TYPELAYOUT_OPAQUE_TYPE_RELOCATABLE(sf_test::FakeOffsetStr, "fofs")

}} // namespace boost::typelayout

// =========================================================================
// Part 1: is_byte_copy_safe -- compile-time admission checks
// =========================================================================

// Plain POD: trivially copyable + no pointers = byte-copy safe
static_assert(
    is_byte_copy_safe_v<sf_test::Vec3>,
    "P1.1: Vec3 is byte-copy safe"
);

// Fundamental types: byte-copy safe
static_assert(
    is_byte_copy_safe_v<int32_t>,
    "P1.2: int32_t is byte-copy safe"
);

static_assert(
    is_byte_copy_safe_v<double>,
    "P1.3: double is byte-copy safe"
);

// Type with pointer: NOT byte-copy safe
static_assert(
    !is_byte_copy_safe_v<sf_test::WithPtr>,
    "P1.4: WithPtr contains a pointer, not byte-copy safe"
);

// Non-trivially-copyable but all members safe: IS byte-copy safe
// (is_byte_copy_safe recurses into members; int32_t is safe)
static_assert(
    is_byte_copy_safe_v<sf_test::NonTrivial>,
    "P1.5: NonTrivial has safe members, byte-copy safe via recursion"
);

// Opaque type (pointer-free): byte-copy safe
static_assert(
    is_byte_copy_safe_v<sf_test::AesKey256>,
    "P1.6: AesKey256 is opaque and pointer-free, byte-copy safe"
);

// Opaque type (NOT pointer-free): NOT byte-copy safe
static_assert(
    !is_byte_copy_safe_v<sf_test::LibHandle>,
    "P1.7: LibHandle is opaque with pointers, not byte-copy safe"
);

// Sensor (plain POD): byte-copy safe
static_assert(
    is_byte_copy_safe_v<sf_test::Sensor>,
    "P1.8: Sensor is byte-copy safe"
);

// =========================================================================
// Part 2: layout_traits fields (has_opaque) + Tool-level predicates
// =========================================================================

// Opaque detection (has_opaque is a Core by-product)
static_assert(
    layout_traits<sf_test::AesKey256>::has_opaque,
    "P2.1: AesKey256 has_opaque == true"
);

static_assert(
    !layout_traits<sf_test::Vec3>::has_opaque,
    "P2.2: Vec3 has_opaque == false"
);

static_assert(
    !layout_traits<int32_t>::has_opaque,
    "P2.3: int32_t has_opaque == false"
);

// has_pointer for REGISTER_OPAQUE respects HasPointer assertion
static_assert(
    !layout_traits<sf_test::AesKey256>::has_pointer,
    "P2.4: AesKey256 has_pointer == false (user asserted no pointers)"
);

static_assert(
    layout_traits<sf_test::LibHandle>::has_pointer,
    "P2.5: LibHandle has_pointer == true (user asserted has pointers)"
);

// is_byte_copy_safe (core predicate)
static_assert(
    is_byte_copy_safe_v<sf_test::Vec3>,
    "P2.6: Vec3 is_byte_copy_safe == true"
);

static_assert(
    !is_byte_copy_safe_v<sf_test::WithPtr>,
    "P2.7: WithPtr is_byte_copy_safe == false"
);

// =========================================================================
// Part 3: Signature uniqueness (exact string identity)
// =========================================================================

// Same-size opaque types with different tags produce different signatures
static_assert(
    !(layout_traits<sf_test::SensorRaw>::signature ==
      layout_traits<sf_test::ActuatorRaw>::signature),
    "P3.1: same-size opaque types with different Tags have different signatures"
);

// Layout-compatible types have the same signature
static_assert(
    layout_traits<sf_test::Vec3>::signature ==
    layout_traits<sf_test::Vec3Alt>::signature,
    "P3.2: layout-compatible types have same signature"
);

// Different layouts have different signatures
static_assert(
    !(layout_traits<sf_test::Vec3>::signature ==
      layout_traits<sf_test::Vec4>::signature),
    "P3.3: different layouts have different signatures"
);

// =========================================================================
// Part 4: Opaque signature format O(Tag|size|align)
// =========================================================================

// Verify opaque signature contains the Tag
static_assert(
    layout_traits<sf_test::AesKey256>::signature.contains(
        FixedString{"O(AesKey256|"}),
    "P4.1: AesKey256 signature contains O(AesKey256|..."
);

static_assert(
    layout_traits<sf_test::SensorRaw>::signature.contains(
        FixedString{"O(SensorRaw|"}),
    "P4.2: SensorRaw signature contains O(SensorRaw|..."
);

// =========================================================================
// Part 5: is_byte_copy_safe for opaque + POD types (compile-time)
// =========================================================================

static_assert(
    is_byte_copy_safe_v<sf_test::Vec3>,
    "P5.1: Vec3 is byte-copy safe"
);

static_assert(
    is_byte_copy_safe_v<int32_t>,
    "P5.2: int32_t is byte-copy safe"
);

static_assert(
    is_byte_copy_safe_v<sf_test::AesKey256>,
    "P5.3: AesKey256 is byte-copy safe"
);

// =========================================================================
// Main -- runtime tests (is_transfer_safe)
// =========================================================================

int main() {
    int passed = 0;
    int failed = 0;
    constexpr int compile_time_tests = 20;  // static_asserts above

    std::cout << "=== Transfer Safety Tests (Signature-Based) ===\n\n";
    std::cout << compile_time_tests
              << " compile-time static_assert tests passed.\n\n";

    // --- Part 6: is_transfer_safe ---

    std::cout << "\n--- Part 6: is_transfer_safe ---\n";

    // 6.1: Same endpoint, matching signature -> true
    {
        constexpr auto local_sig = get_layout_signature<sf_test::Vec3>();
        bool result = is_transfer_safe<sf_test::Vec3>(std::string_view(local_sig));
        TEST(result, "P6.1: Vec3 same signature -> is_transfer_safe", passed, failed);
    }

    // 6.2: Same endpoint, different signature -> false (condition 3 fails)
    {
        bool result = is_transfer_safe<sf_test::Vec3>("[32-le]record[s:12,a:4]{@0:f32[s:4,a:4]}");
        TEST(!result, "P6.2: Vec3 wrong signature -> not is_transfer_safe", passed, failed);
    }

    // 6.3: WithPtr: condition 2 (!has_pointer) fails -> always false
    {
        constexpr auto local_sig = get_layout_signature<sf_test::Vec3>();
        // Even if we pass a matching-looking sig, WithPtr has a pointer → false
        bool result = is_transfer_safe<sf_test::WithPtr>(std::string_view(local_sig));
        TEST(!result, "P6.3: WithPtr (has_pointer) -> not is_transfer_safe", passed, failed);
    }

    // 6.4: Opaque type, matching signature -> true
    {
        constexpr auto local_sig = get_layout_signature<sf_test::AesKey256>();
        bool result = is_transfer_safe<sf_test::AesKey256>(std::string_view(local_sig));
        TEST(result, "P6.4: AesKey256 matching signature -> is_transfer_safe", passed, failed);
    }

    // 6.5: Opaque type, wrong tag in remote signature -> false
    {
        constexpr auto wrong_sig = get_layout_signature<sf_test::ActuatorRaw>();
        bool result = is_transfer_safe<sf_test::SensorRaw>(std::string_view(wrong_sig));
        TEST(!result, "P6.5: SensorRaw vs ActuatorRaw signature -> not is_transfer_safe", passed, failed);
    }

    // 6.6: LibHandle has_pointer=true -> always false even with exact signature
    {
        constexpr auto local_sig = get_layout_signature<sf_test::LibHandle>();
        bool result = is_transfer_safe<sf_test::LibHandle>(std::string_view(local_sig));
        TEST(!result, "P6.6: LibHandle (has_pointer=true) -> not is_transfer_safe", passed, failed);
    }

    // 6.7: Relocatable opaque type, matching signature -> true
    {
        constexpr auto local_sig = get_layout_signature<sf_test::FakeOffsetStr>();
        bool result = is_transfer_safe<sf_test::FakeOffsetStr>(std::string_view(local_sig));
        TEST(result, "P6.7: FakeOffsetStr (relocatable opaque) matching sig -> is_transfer_safe", passed, failed);
    }

    // 6.8: Relocatable opaque type, mismatched signature -> false
    {
        bool result = is_transfer_safe<sf_test::FakeOffsetStr>("[64-le]O(fofs|64|8)");
        TEST(!result, "P6.8: FakeOffsetStr wrong sig -> not is_transfer_safe", passed, failed);
    }

    // 6.9: Composite struct with opaque members, matching signature -> true
    {
        constexpr auto local_sig = get_layout_signature<sf_test::MessageWithOffsetStr>();
        bool result = is_transfer_safe<sf_test::MessageWithOffsetStr>(std::string_view(local_sig));
        TEST(result, "P6.9: MessageWithOffsetStr (composite+opaque) matching sig -> is_transfer_safe", passed, failed);
    }

    std::cout << "\n--- Part 7: Signature info ---\n";
    std::cout << "Vec3 signature:       "
              << layout_traits<sf_test::Vec3>::signature.value << "\n";
    std::cout << "Vec3 has_opaque:      "
              << layout_traits<sf_test::Vec3>::has_opaque << "\n";
    std::cout << "AesKey256 signature:  "
              << layout_traits<sf_test::AesKey256>::signature.value << "\n";
    std::cout << "AesKey256 has_opaque: "
              << layout_traits<sf_test::AesKey256>::has_opaque << "\n";
    std::cout << "SensorRaw signature:  "
              << layout_traits<sf_test::SensorRaw>::signature.value << "\n";
    std::cout << "ActuatorRaw signature:"
              << layout_traits<sf_test::ActuatorRaw>::signature.value << "\n";
    std::cout << "LibHandle signature:  "
              << layout_traits<sf_test::LibHandle>::signature.value << "\n";
    std::cout << "LibHandle has_ptr:    "
              << layout_traits<sf_test::LibHandle>::has_pointer << "\n";
    std::cout << "Sensor signature:     "
              << layout_traits<sf_test::Sensor>::signature.value << "\n";

    // --- Summary ---
    std::cout << "\n=== Results ===\n";
    std::cout << "Compile-time: " << compile_time_tests << " passed\n";
    std::cout << "Runtime:      " << passed << " passed, " << failed << " failed\n";
    std::cout << "Total:        " << (compile_time_tests + passed) << " passed, "
              << failed << " failed\n";

    return failed > 0 ? 1 : 0;
}