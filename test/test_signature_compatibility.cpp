// Boost.TypeLayout
//
// Test: Signature-Driven Compatibility Model
// Tests for the revised compatibility model including:
// - Virtual inheritance signature verification
// - Bit-field signature verification
// - Runtime state type rejection (std::variant, std::optional)
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#include <boost/typelayout.hpp>
#include <boost/typelayout/util/serialization_check.hpp>
#include <boost/typelayout/util/concepts.hpp>
#include <variant>
#include <optional>

using namespace boost::typelayout;

// =========================================================================
// Test Types: Virtual Inheritance
// =========================================================================

struct VirtualBase {
    int base_value;
};

struct VirtualDerived1 : virtual VirtualBase {
    int derived1_value;
};

struct VirtualDerived2 : virtual VirtualBase {
    int derived2_value;
};

// Diamond inheritance
struct VirtualDiamond : VirtualDerived1, VirtualDerived2 {
    int diamond_value;
};

// =========================================================================
// Test Types: Bit-fields
// =========================================================================

struct BitfieldFlags {
    uint32_t version : 4;
    uint32_t flags : 12;
    uint32_t length : 16;
};

struct NestedBitfield {
    BitfieldFlags header;
    int data;
};

struct MultiUnitBitfield {
    uint32_t a : 8;
    uint32_t b : 8;
    uint32_t c : 8;
    uint32_t d : 8;
    uint64_t e : 32;
    uint64_t f : 32;
};

// =========================================================================
// Test Types: Simple Serializable (for comparison)
// =========================================================================

struct SimpleSerializable {
    int32_t value;
    float data;
};

struct ArrayMember {
    int32_t values[4];
};

// =========================================================================
// Tests: Virtual Inheritance Signatures
// =========================================================================

// Virtual inheritance types should generate valid signatures
// (signatures include offset information)
static_assert(requires { get_layout_signature<VirtualBase>(); },
    "VirtualBase should generate a layout signature");

static_assert(requires { get_layout_signature<VirtualDerived1>(); },
    "VirtualDerived1 should generate a layout signature");

static_assert(requires { get_layout_signature<VirtualDiamond>(); },
    "VirtualDiamond should generate a layout signature");

// =========================================================================
// Tests: Bit-field Signatures (ALLOWED for serialization)
// =========================================================================

// Bit-fields should be detected correctly
static_assert(has_bitfields_v<BitfieldFlags>,
    "BitfieldFlags should be detected as having bit-fields");

static_assert(has_bitfields_v<NestedBitfield>,
    "NestedBitfield should be detected as having bit-fields");

static_assert(has_bitfields_v<MultiUnitBitfield>,
    "MultiUnitBitfield should be detected as having bit-fields");

static_assert(!has_bitfields_v<SimpleSerializable>,
    "SimpleSerializable should NOT be detected as having bit-fields");

// Bit-fields are NOW ALLOWED for serialization
// (signature includes bit positions, differences detected by signature comparison)
static_assert(is_serializable_v<BitfieldFlags>,
    "BitfieldFlags SHOULD be serializable (signature-driven model)");

static_assert(is_serializable_v<NestedBitfield>,
    "NestedBitfield SHOULD be serializable (signature-driven model)");

static_assert(is_serializable_v<MultiUnitBitfield>,
    "MultiUnitBitfield SHOULD be serializable (signature-driven model)");

// =========================================================================
// Tests: Runtime State Types (NOT serializable)
// =========================================================================

// std::variant should be detected and rejected
static_assert(!is_serializable_v<std::variant<int, double>>,
    "std::variant should NOT be serializable (has runtime state)");

static_assert(serialization_blocker_v<std::variant<int, double>> == SerializationBlocker::HasRuntimeState,
    "std::variant blocker should be HasRuntimeState");

// std::optional should be detected and rejected
static_assert(!is_serializable_v<std::optional<int>>,
    "std::optional should NOT be serializable (has runtime state)");

static_assert(serialization_blocker_v<std::optional<int>> == SerializationBlocker::HasRuntimeState,
    "std::optional blocker should be HasRuntimeState");

// Nested runtime state types should also be rejected
struct ContainsVariant {
    int id;
    std::variant<int, float> value;
};

struct ContainsOptional {
    int id;
    std::optional<double> value;
};

static_assert(!is_serializable_v<ContainsVariant>,
    "Struct containing std::variant should NOT be serializable");

static_assert(!is_serializable_v<ContainsOptional>,
    "Struct containing std::optional should NOT be serializable");

// =========================================================================
// Tests: Simple Types (still serializable)
// =========================================================================

static_assert(is_serializable_v<SimpleSerializable>,
    "SimpleSerializable should be serializable");

static_assert(is_serializable_v<ArrayMember>,
    "ArrayMember should be serializable");

static_assert(is_serializable_v<int32_t>,
    "int32_t should be serializable");

static_assert(is_serializable_v<double>,
    "double should be serializable");

// =========================================================================
// Tests: Non-serializable types (unchanged behavior)
// =========================================================================

struct HasPointer {
    int* ptr;
};

struct HasVirtualFunction {
    virtual void foo() {}
};

static_assert(!is_serializable_v<HasPointer>,
    "Types with pointers should NOT be serializable");

static_assert(!is_serializable_v<HasVirtualFunction>,
    "Types with virtual functions should NOT be serializable");

static_assert(serialization_blocker_v<HasPointer> == SerializationBlocker::HasPointer,
    "HasPointer blocker should be HasPointer");

static_assert(serialization_blocker_v<HasVirtualFunction> == SerializationBlocker::HasVirtualFn,
    "HasVirtualFunction blocker should be HasVirtualFn");

// =========================================================================
// Main function (all tests are compile-time)
// =========================================================================

int main() {
    // All tests are compile-time static_asserts
    // If compilation succeeds, all tests passed
    
    // Runtime verification of signature generation
    constexpr auto bitfield_sig = get_layout_signature<BitfieldFlags>();
    constexpr auto simple_sig = get_layout_signature<SimpleSerializable>();
    
    // Just verify they can be instantiated
    (void)bitfield_sig;
    (void)simple_sig;
    
    return 0;
}
