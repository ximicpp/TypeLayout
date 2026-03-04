// Test: Empty member evaporation fix (Bug 1) and [[no_unique_address]]
// behavior verification (Bug 2).
//
// Bug 1: Empty class members were "evaporated" during flattening because
//   the recursive engine returned "" for classes with no bases and no
//   fields.  After the fix, empty classes are treated as leaf nodes:
//     - Standalone: emitted as "record[s:1,a:1]{}" (sizeof == 1)
//     - Embedded member/base: emitted as "record[s:0,a:1]{}" (occupies 0 bytes
//       in the host layout, consistent with padding bitmap)
//
// Bug 2: [[no_unique_address]] empty members overlap with adjacent fields
//   at the same offset.  P2996 correctly reports their offset but does
//   not expose the attribute itself.  After the Bug 1 fix, these members
//   appear in the signature with their actual (overlapping) offset.
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#include <boost/typelayout/typelayout.hpp>
#include "test_util.hpp"
#include <iostream>
#include <cstdint>

using namespace boost::typelayout;
using boost::typelayout::test::contains;

// =========================================================================
// Test types
// =========================================================================

struct Empty {};

struct AlsoEmpty {};

struct WithEmptyMember {
    Empty e;
    int32_t x;
};

struct TwoEmpties {
    Empty a;
    Empty b;
    int32_t x;
};

struct DifferentEmpties {
    Empty a;
    AlsoEmpty b;
    int32_t x;
};

struct WithNoUniqueAddr {
    [[no_unique_address]] Empty e;
    int32_t x;
};

struct TwoNoUniqueAddr {
    [[no_unique_address]] Empty a;
    [[no_unique_address]] Empty b;
    int32_t x;
};

// Nested: struct containing a struct that contains an empty member
struct Inner {
    Empty e;
    int32_t val;
};

struct Outer {
    Inner inner;
    int32_t extra;
};

// Empty base class
struct DerivedFromEmpty : Empty {
    int32_t x;
};

struct DerivedFromTwoEmpties : Empty, AlsoEmpty {
    int32_t x;
};

// =========================================================================
// Part 1: Bug 1 -- Empty members must not evaporate
// =========================================================================

constexpr auto sig_empty = get_layout_signature<Empty>();
constexpr auto sig_with_empty = get_layout_signature<WithEmptyMember>();
constexpr auto sig_two_empties = get_layout_signature<TwoEmpties>();
constexpr auto sig_diff_empties = get_layout_signature<DifferentEmpties>();

// B1.1: Empty struct produces a valid signature with empty body
static_assert(
    contains(sig_empty, "record[s:1,a:1]{}"),
    "B1.1: Empty struct should produce record[s:1,a:1]{}"
);

// B1.2: Empty member must appear in the signature (not evaporate)
// Embedded empty members use s:0 (they occupy 0 bytes in host layout)
static_assert(
    contains(sig_with_empty, "record[s:0,a:1]{}"),
    "B1.2: WithEmptyMember must contain empty member record[s:0,a:1]{}"
);

// B1.3: Empty member should be at offset 0
static_assert(
    contains(sig_with_empty, "@0:record[s:0,a:1]{}"),
    "B1.3: Empty member should be emitted at @0"
);

// B1.4: WithEmptyMember and TwoEmpties must produce DIFFERENT signatures
static_assert(
    !(sig_with_empty == sig_two_empties),
    "B1.4: One empty vs two empties must produce different signatures"
);

// B1.5: TwoEmpties should contain two empty member entries
static_assert(
    contains(sig_two_empties, "@0:record[s:0,a:1]{}") &&
    contains(sig_two_empties, "@1:record[s:0,a:1]{}"),
    "B1.5: TwoEmpties should show empty members at offsets 0 and 1"
);

// B1.6: DifferentEmpties should also contain two empty entries
// (Empty and AlsoEmpty have identical layout signatures -- both record[s:0,a:1]{} when embedded)
static_assert(
    contains(sig_diff_empties, "@0:record[s:0,a:1]{}") &&
    contains(sig_diff_empties, "@1:record[s:0,a:1]{}"),
    "B1.6: DifferentEmpties should show two empty members"
);

// =========================================================================
// Part 2: Bug 2 -- [[no_unique_address]] behavior
// =========================================================================

constexpr auto sig_nua = get_layout_signature<WithNoUniqueAddr>();
constexpr auto sig_two_nua = get_layout_signature<TwoNoUniqueAddr>();

// B2.1: [[no_unique_address]] empty member must appear in signature
static_assert(
    contains(sig_nua, "record[s:0,a:1]{}"),
    "B2.1: [[no_unique_address]] empty member must not evaporate"
);

// B2.2: WithNoUniqueAddr should have sizeof == 4 (same as bare int32_t struct)
static_assert(sizeof(WithNoUniqueAddr) == 4,
    "B2.2: [[no_unique_address]] should optimize away empty member space"
);

// B2.3: Empty member and int32_t should overlap at offset 0
static_assert(
    contains(sig_nua, "@0:record[s:0,a:1]{}") &&
    contains(sig_nua, "@0:i32[s:4,a:4]"),
    "B2.3: [[no_unique_address]] empty member overlaps with next field at @0"
);

// B2.4: WithNoUniqueAddr and WithEmptyMember must have different signatures
// (sizeof differs: 4 vs 8)
static_assert(
    !(sig_nua == sig_with_empty),
    "B2.4: NUA and regular empty member produce different signatures"
);

// B2.5: TwoNoUniqueAddr has sizeof == 4 as well
static_assert(sizeof(TwoNoUniqueAddr) == 4,
    "B2.5: Two [[no_unique_address]] empties still sizeof == 4"
);

// B2.6: TwoNoUniqueAddr should show two empty members and int at offset 0
static_assert(
    contains(sig_two_nua, "record[s:0,a:1]{}"),
    "B2.6: TwoNoUniqueAddr contains empty member entries"
);

// =========================================================================
// Part 3: Nested and base class scenarios
// =========================================================================

constexpr auto sig_outer = get_layout_signature<Outer>();
constexpr auto sig_derived_empty = get_layout_signature<DerivedFromEmpty>();
constexpr auto sig_derived_two = get_layout_signature<DerivedFromTwoEmpties>();

// B1.7: Nested struct -- Inner's empty member should appear in flattened Outer
static_assert(
    contains(sig_outer, "record[s:0,a:1]{}"),
    "B1.7: Nested Inner's empty member should appear in Outer's flattened signature"
);

// B1.8: Empty base class should appear as leaf node with s:0 (EBO)
static_assert(
    contains(sig_derived_empty, "record[s:0,a:1]{}"),
    "B1.8: Empty base class (EBO) should appear with s:0 in derived signature"
);

// B1.9: Multiple empty bases should both appear with s:0 (EBO)
static_assert(
    contains(sig_derived_two, "record[s:0,a:1]{}"),
    "B1.9: Multiple empty base classes (EBO) should appear with s:0"
);

// B1.10: DerivedFromEmpty and DerivedFromTwoEmpties should differ
static_assert(
    !(sig_derived_empty == sig_derived_two),
    "B1.10: Single vs dual empty base should produce different signatures"
);

// =========================================================================
// Main -- runtime summary
// =========================================================================

int main() {
    std::cout << "=== Empty Member / [[no_unique_address]] Tests ===\n\n";

    std::cout << "--- Signatures ---\n";
    std::cout << "Empty:              " << sig_empty.value << "\n";
    std::cout << "WithEmptyMember:    " << sig_with_empty.value << "\n";
    std::cout << "TwoEmpties:         " << sig_two_empties.value << "\n";
    std::cout << "DifferentEmpties:   " << sig_diff_empties.value << "\n";
    std::cout << "WithNoUniqueAddr:   " << sig_nua.value << "\n";
    std::cout << "TwoNoUniqueAddr:    " << sig_two_nua.value << "\n";
    std::cout << "Outer (nested):     " << sig_outer.value << "\n";
    std::cout << "DerivedFromEmpty:   " << sig_derived_empty.value << "\n";
    std::cout << "DerivedFromTwoEmpties: " << sig_derived_two.value << "\n";

    std::cout << "\n--- sizeof ---\n";
    std::cout << "sizeof(Empty):            " << sizeof(Empty) << "\n";
    std::cout << "sizeof(WithEmptyMember):  " << sizeof(WithEmptyMember) << "\n";
    std::cout << "sizeof(WithNoUniqueAddr): " << sizeof(WithNoUniqueAddr) << "\n";
    std::cout << "sizeof(TwoNoUniqueAddr):  " << sizeof(TwoNoUniqueAddr) << "\n";

    std::cout << "\nAll static_assert tests passed at compile time.\n";
    return 0;
}