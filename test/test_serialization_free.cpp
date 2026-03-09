// Serialization-free trait tests (signature-based, no hashing).
//
// Verifies:
//   1. is_local_serialization_free -- compile-time single-endpoint judgement
//   2. serialization_free_assert   -- compile-time assertion helper
//   3. TYPELAYOUT_REGISTER_OPAQUE  -- opaque type registration with Tag
//   4. SignatureRegistry           -- runtime signature-based comparison
//   5. Exact signature matching    -- no hash collision risk
//   6. Diagnostic output           -- self-documenting signatures
//   7. Composite types with opaque fields
//
// All compile-time assertions use static_assert.  Runtime assertions
// use the TEST macro defined below.
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#include <boost/typelayout/typelayout.hpp>
#include <boost/typelayout/tools/serialization_free.hpp>
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

// =========================================================================
// Test types
// =========================================================================

namespace sf_test {

// -- Plain POD type: should be serialization-free
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

// -- Type with pointer: NOT serialization-free
struct WithPtr {
    int32_t id;
    void* data;
};

// -- Non-trivially-copyable: NOT serialization-free
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

}} // namespace boost::typelayout

// =========================================================================
// Part 1: is_local_serialization_free -- compile-time checks
// =========================================================================

// Plain POD: trivially copyable + no pointers = serialization-free
static_assert(
    is_local_serialization_free_v<sf_test::Vec3>,
    "P1.1: Vec3 is locally serialization-free"
);

// Fundamental types: serialization-free
static_assert(
    is_local_serialization_free_v<int32_t>,
    "P1.2: int32_t is locally serialization-free"
);

static_assert(
    is_local_serialization_free_v<double>,
    "P1.3: double is locally serialization-free"
);

// Type with pointer: NOT serialization-free
static_assert(
    !is_local_serialization_free_v<sf_test::WithPtr>,
    "P1.4: WithPtr contains a pointer, not serialization-free"
);

// Non-trivially-copyable: NOT serialization-free
static_assert(
    !is_local_serialization_free_v<sf_test::NonTrivial>,
    "P1.5: NonTrivial is not trivially copyable, not serialization-free"
);

// Opaque type (pointer-free): serialization-free
static_assert(
    is_local_serialization_free_v<sf_test::AesKey256>,
    "P1.6: AesKey256 is opaque but pointer-free, serialization-free"
);

// Opaque type (NOT pointer-free): NOT serialization-free
static_assert(
    !is_local_serialization_free_v<sf_test::LibHandle>,
    "P1.7: LibHandle is opaque with pointers, not serialization-free"
);

// Sensor (plain POD): serialization-free
static_assert(
    is_local_serialization_free_v<sf_test::Sensor>,
    "P1.8: Sensor is locally serialization-free"
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

// is_local_serialization_free (Tool-level predicate)
static_assert(
    is_local_serialization_free_v<sf_test::Vec3>,
    "P2.6: Vec3 is_local_serialization_free == true"
);

static_assert(
    !is_local_serialization_free_v<sf_test::WithPtr>,
    "P2.7: WithPtr is_local_serialization_free == false"
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
// Part 5: serialization_free_assert
// =========================================================================

// Must compile for safe types
static_assert(
    serialization_free_assert<sf_test::Vec3>::value,
    "P5.1: Vec3 passes serialization_free_assert"
);

static_assert(
    serialization_free_assert<int32_t>::value,
    "P5.2: int32_t passes serialization_free_assert"
);

static_assert(
    serialization_free_assert<sf_test::AesKey256>::value,
    "P5.3: AesKey256 passes serialization_free_assert"
);

// =========================================================================
// Main -- runtime tests (SignatureRegistry)
// =========================================================================

int main() {
    int passed = 0;
    int failed = 0;
    constexpr int compile_time_tests = 20;  // static_asserts above

    std::cout << "=== Serialization Free Tests (Signature-Based) ===\n\n";
    std::cout << compile_time_tests
              << " compile-time static_assert tests passed.\n\n";

    // --- Part 6: SignatureRegistry basic operations ---

    std::cout << "--- Part 6: SignatureRegistry ---\n";

    // 6.1: Same-endpoint matching (simulated: local == remote)
    {
        SignatureRegistry reg;
        reg.register_local<sf_test::Vec3>();

        // Simulate remote sending the same signature
        auto key = std::string(typeid(sf_test::Vec3).name());
        auto local_sig = std::string_view(layout_traits<sf_test::Vec3>::signature);
        reg.register_remote(key, local_sig);

        bool result = reg.is_serialization_free<sf_test::Vec3>();
        TEST(result, "P6.1: same signature -> serialization_free", passed, failed);
    }

    // 6.2: Different signature -> not serialization_free
    {
        SignatureRegistry reg;
        reg.register_local<sf_test::Vec3>();

        auto key = std::string(typeid(sf_test::Vec3).name());
        reg.register_remote(key, "[32-le]{f32@0,f32@4,f32@8|12|4}");

        bool result = reg.is_serialization_free<sf_test::Vec3>();
        TEST(!result, "P6.2: different signature -> not serialization_free", passed, failed);
    }

    // 6.3: Remote not registered -> not serialization_free
    {
        SignatureRegistry reg;
        reg.register_local<sf_test::Vec3>();
        // No register_remote call

        bool result = reg.is_serialization_free<sf_test::Vec3>();
        TEST(!result, "P6.3: remote not registered -> not serialization_free", passed, failed);
    }

    // 6.4: Opaque type with matching signature -> serialization_free
    {
        SignatureRegistry reg;
        reg.register_local<sf_test::AesKey256>();

        auto key = std::string(typeid(sf_test::AesKey256).name());
        auto local_sig = std::string_view(layout_traits<sf_test::AesKey256>::signature);
        reg.register_remote(key, local_sig);

        bool result = reg.is_serialization_free<sf_test::AesKey256>();
        TEST(result, "P6.4: opaque with matching sig -> serialization_free", passed, failed);
    }

    // 6.5: Opaque type with wrong tag in remote sig -> not serialization_free
    {
        SignatureRegistry reg;
        reg.register_local<sf_test::SensorRaw>();

        auto key = std::string(typeid(sf_test::SensorRaw).name());
        // Remote sends ActuatorRaw's signature instead
        auto wrong_sig = std::string_view(layout_traits<sf_test::ActuatorRaw>::signature);
        reg.register_remote(key, wrong_sig);

        bool result = reg.is_serialization_free<sf_test::SensorRaw>();
        TEST(!result, "P6.5: opaque tag mismatch -> not serialization_free", passed, failed);
    }

    // 6.6: Multiple types in one registry
    {
        SignatureRegistry reg;
        reg.register_local<sf_test::Vec3>();
        reg.register_local<sf_test::Packet>();
        reg.register_local<sf_test::AesKey256>();

        // Simulate: Vec3 matches, Packet does not (different platform), AesKey matches
        auto vec3_key = std::string(typeid(sf_test::Vec3).name());
        auto pkt_key = std::string(typeid(sf_test::Packet).name());
        auto aes_key = std::string(typeid(sf_test::AesKey256).name());

        reg.register_remote(vec3_key, std::string_view(layout_traits<sf_test::Vec3>::signature));
        reg.register_remote(pkt_key, "[32-le]{u32@0,f32@4,f32@8,f32@12|16|4}");
        reg.register_remote(aes_key, std::string_view(layout_traits<sf_test::AesKey256>::signature));

        TEST(reg.is_serialization_free<sf_test::Vec3>(),
             "P6.6a: Vec3 matches in multi-type registry", passed, failed);
        TEST(!reg.is_serialization_free<sf_test::Packet>(),
             "P6.6b: Packet does not match (different platform)", passed, failed);
        TEST(reg.is_serialization_free<sf_test::AesKey256>(),
             "P6.6c: AesKey256 matches in multi-type registry", passed, failed);
    }

    // --- Part 7: Diagnostic output ---

    std::cout << "\n--- Part 7: Diagnostics ---\n";

    // 7.1: Diagnose a mismatch
    {
        SignatureRegistry reg;
        reg.register_local<sf_test::Sensor>();

        auto key = std::string(typeid(sf_test::Sensor).name());
        reg.register_remote(key, "[32-le]{c8@0,i32@4,f64@8|16|4}");

        std::string diag = reg.diagnose<sf_test::Sensor>();
        TEST(diag.find("local:") != std::string::npos &&
             diag.find("remote:") != std::string::npos,
             "P7.1: diagnose() produces local and remote info", passed, failed);
        std::cout << "    Diagnostic output:\n";
        // Indent each line
        for (char c : diag) {
            if (c == '\n') std::cout << "\n    ";
            else std::cout << c;
        }
        std::cout << "\n";
    }

    // 7.2: Diagnose unregistered remote
    {
        SignatureRegistry reg;
        reg.register_local<sf_test::Vec3>();

        std::string diag = reg.diagnose<sf_test::Vec3>();
        TEST(diag.find("(not registered)") != std::string::npos,
             "P7.2: diagnose() shows (not registered) for missing remote", passed, failed);
    }

    // --- Part 8: is_transfer_safe ---

    std::cout << "\n--- Part 8: is_transfer_safe ---\n";

    // 8.1: Same endpoint, matching signature → true
    {
        constexpr auto local_sig = get_layout_signature<sf_test::Vec3>();
        bool result = is_transfer_safe<sf_test::Vec3>(std::string_view(local_sig));
        TEST(result, "P8.1: Vec3 same signature -> is_transfer_safe", passed, failed);
    }

    // 8.2: Same endpoint, different signature → false (condition 3 fails)
    {
        bool result = is_transfer_safe<sf_test::Vec3>("[32-le]record[s:12,a:4]{@0:f32[s:4,a:4]}");
        TEST(!result, "P8.2: Vec3 wrong signature -> not is_transfer_safe", passed, failed);
    }

    // 8.3: WithPtr: condition 2 (!has_pointer) fails → always false
    {
        constexpr auto local_sig = get_layout_signature<sf_test::Vec3>();
        // Even if we pass a matching-looking sig, WithPtr has a pointer → false
        bool result = is_transfer_safe<sf_test::WithPtr>(std::string_view(local_sig));
        TEST(!result, "P8.3: WithPtr (has_pointer) -> not is_transfer_safe", passed, failed);
    }

    // 8.4: Opaque type, matching signature → true
    {
        constexpr auto local_sig = get_layout_signature<sf_test::AesKey256>();
        bool result = is_transfer_safe<sf_test::AesKey256>(std::string_view(local_sig));
        TEST(result, "P8.4: AesKey256 matching signature -> is_transfer_safe", passed, failed);
    }

    // 8.5: Opaque type, wrong tag in remote signature → false
    {
        constexpr auto wrong_sig = get_layout_signature<sf_test::ActuatorRaw>();
        bool result = is_transfer_safe<sf_test::SensorRaw>(std::string_view(wrong_sig));
        TEST(!result, "P8.5: SensorRaw vs ActuatorRaw signature -> not is_transfer_safe", passed, failed);
    }

    // 8.6: LibHandle has_pointer=true → always false even with exact signature
    {
        constexpr auto local_sig = get_layout_signature<sf_test::LibHandle>();
        bool result = is_transfer_safe<sf_test::LibHandle>(std::string_view(local_sig));
        TEST(!result, "P8.6: LibHandle (has_pointer=true) -> not is_transfer_safe", passed, failed);
    }

    std::cout << "\n--- Part 9: Signature info ---\n";
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