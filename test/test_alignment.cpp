// Boost.TypeLayout
//
// Alignment Completeness Test Suite
// Verifies that alignment information in signatures is complete and accurate
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/typelayout.hpp>
#include <cstdint>
#include <string_view>

using namespace boost::typelayout;

// Helper to convert CompileString to string_view for searching
template<typename T>
consteval std::string_view to_view(const T& cs) {
    return std::string_view{cs.c_str(), cs.length()};
}

// ============================================================================
// Test 1: Basic Type Alignment
// ============================================================================

static_assert([]() constexpr {
    constexpr auto sig = get_layout_signature<int8_t>();
    return to_view(sig).find(",a:1]") != std::string_view::npos;
}(), "int8_t should have alignment 1");

static_assert([]() constexpr {
    constexpr auto sig = get_layout_signature<int16_t>();
    return to_view(sig).find(",a:2]") != std::string_view::npos;
}(), "int16_t should have alignment 2");

static_assert([]() constexpr {
    constexpr auto sig = get_layout_signature<int32_t>();
    return to_view(sig).find(",a:4]") != std::string_view::npos;
}(), "int32_t should have alignment 4");

static_assert([]() constexpr {
    constexpr auto sig = get_layout_signature<int64_t>();
    return to_view(sig).find(",a:8]") != std::string_view::npos;
}(), "int64_t should have alignment 8");

static_assert([]() constexpr {
    constexpr auto sig = get_layout_signature<double>();
    return to_view(sig).find(",a:8]") != std::string_view::npos;
}(), "double should have alignment 8");

// ============================================================================
// Test 2: Struct Alignment (natural alignment)
// ============================================================================

struct NaturalAlign {
    int8_t a;    // offset 0
    int32_t b;   // offset 4 (aligned to 4)
};

static_assert(alignof(NaturalAlign) == 4, "NaturalAlign should have alignment 4");
static_assert(sizeof(NaturalAlign) == 8, "NaturalAlign should have size 8");

static_assert([]() constexpr {
    constexpr auto sig = get_layout_signature<NaturalAlign>();
    // struct[s:8,a:4]
    return to_view(sig).find("struct[s:8,a:4]") != std::string_view::npos;
}(), "NaturalAlign struct should have s:8,a:4");

// ============================================================================
// Test 3: alignas Specified Alignment
// ============================================================================

struct alignas(16) Aligned16 {
    int32_t x;
    int32_t y;
};

static_assert(alignof(Aligned16) == 16, "Aligned16 should have alignment 16");
static_assert(sizeof(Aligned16) == 16, "Aligned16 should have size 16");

static_assert([]() constexpr {
    constexpr auto sig = get_layout_signature<Aligned16>();
    // struct[s:16,a:16]
    return to_view(sig).find("struct[s:16,a:16]") != std::string_view::npos;
}(), "alignas(16) struct should have a:16 in signature");

// ============================================================================
// Test 4: Over-aligned Types (cache line)
// ============================================================================

struct alignas(64) CacheLineAligned {
    int32_t data;
};

static_assert(alignof(CacheLineAligned) == 64, "CacheLineAligned should have alignment 64");
static_assert(sizeof(CacheLineAligned) == 64, "CacheLineAligned should have size 64");

static_assert([]() constexpr {
    constexpr auto sig = get_layout_signature<CacheLineAligned>();
    // struct[s:64,a:64]
    return to_view(sig).find("struct[s:64,a:64]") != std::string_view::npos;
}(), "alignas(64) struct should have a:64 in signature");

// ============================================================================
// Test 5: Padding Derivation from Signature
// ============================================================================

struct WithPadding {
    int8_t a;     // offset 0, size 1
    // padding: 3 bytes
    int32_t b;    // offset 4, size 4
    int8_t c;     // offset 8, size 1
    // tail padding: 3 bytes
};

static_assert(sizeof(WithPadding) == 12, "WithPadding should have size 12");
static_assert(alignof(WithPadding) == 4, "WithPadding should have alignment 4");

static_assert([]() constexpr {
    constexpr auto sig = get_layout_signature<WithPadding>();
    // Should contain offsets that allow padding derivation:
    // @0[a]:... @4[b]:... @8[c]:...
    auto view = to_view(sig);
    bool has_a_at_0 = view.find("@0[a]") != std::string_view::npos;
    bool has_b_at_4 = view.find("@4[b]") != std::string_view::npos;
    bool has_c_at_8 = view.find("@8[c]") != std::string_view::npos;
    bool has_size_12 = view.find("[s:12,") != std::string_view::npos;
    return has_a_at_0 && has_b_at_4 && has_c_at_8 && has_size_12;
}(), "WithPadding signature should contain all offsets for padding derivation");

// ============================================================================
// Test 6: Array Alignment
// ============================================================================

static_assert([]() constexpr {
    constexpr auto sig = get_layout_signature<int32_t[4]>();
    // array[s:16,a:4]
    return to_view(sig).find("array[s:16,a:4]") != std::string_view::npos;
}(), "int32_t[4] should have array alignment 4");

static_assert([]() constexpr {
    constexpr auto sig = get_layout_signature<double[2]>();
    // array[s:16,a:8]
    return to_view(sig).find("array[s:16,a:8]") != std::string_view::npos;
}(), "double[2] should have array alignment 8");

// ============================================================================
// Test 7: Union Alignment (maximum member alignment)
// ============================================================================

union TestUnion {
    int8_t a;
    int32_t b;
    double c;
};

static_assert(alignof(TestUnion) == 8, "TestUnion should have alignment 8 (max of members)");
static_assert(sizeof(TestUnion) == 8, "TestUnion should have size 8");

static_assert([]() constexpr {
    constexpr auto sig = get_layout_signature<TestUnion>();
    // union[s:8,a:8]
    return to_view(sig).find("union[s:8,a:8]") != std::string_view::npos;
}(), "Union should have max member alignment in signature");

// ============================================================================
// Test 8: Nested Struct Alignment
// ============================================================================

struct Inner {
    double d;  // alignof = 8
};

struct Outer {
    int8_t x;   // offset 0
    Inner inner; // offset 8 (aligned to 8)
};

static_assert(alignof(Outer) == 8, "Outer should have alignment 8");
static_assert(sizeof(Outer) == 16, "Outer should have size 16");

static_assert([]() constexpr {
    constexpr auto sig = get_layout_signature<Outer>();
    // struct[s:16,a:8]
    return to_view(sig).find("struct[s:16,a:8]") != std::string_view::npos;
}(), "Outer struct should inherit Inner's alignment");

// ============================================================================
// Test 9: Platform-Specific Pointer Alignment
// ============================================================================

static_assert([]() constexpr {
    constexpr auto sig = get_layout_signature<void*>();
    if constexpr (sizeof(void*) == 8) {
        // 64-bit: ptr[s:8,a:8]
        return to_view(sig).find("ptr[s:8,a:8]") != std::string_view::npos;
    } else {
        // 32-bit: ptr[s:4,a:4]
        return to_view(sig).find("ptr[s:4,a:4]") != std::string_view::npos;
    }
}(), "Pointer alignment should match platform");

// ============================================================================
// Test 10: Enum Alignment
// ============================================================================

enum class SmallEnum : uint8_t { A, B, C };
enum class NormalEnum : int32_t { X, Y, Z };
enum class LargeEnum : int64_t { P, Q, R };

static_assert([]() constexpr {
    constexpr auto sig = get_layout_signature<SmallEnum>();
    return to_view(sig).find(",a:1]") != std::string_view::npos;
}(), "uint8_t enum should have alignment 1");

static_assert([]() constexpr {
    constexpr auto sig = get_layout_signature<NormalEnum>();
    return to_view(sig).find(",a:4]") != std::string_view::npos;
}(), "int32_t enum should have alignment 4");

static_assert([]() constexpr {
    constexpr auto sig = get_layout_signature<LargeEnum>();
    return to_view(sig).find(",a:8]") != std::string_view::npos;
}(), "int64_t enum should have alignment 8");

// ============================================================================
// Test 11: Signature Format Consistency
// ============================================================================

// Every type signature should contain [s:N,a:M] format
template<typename T>
consteval bool has_size_align_format() {
    constexpr auto sig = get_layout_signature<T>();
    auto view = to_view(sig);
    // Find pattern [s:...,a:...]
    auto pos = view.find("[s:");
    if (pos == std::string_view::npos) return false;
    auto pos2 = view.find(",a:", pos);
    if (pos2 == std::string_view::npos) return false;
    auto pos3 = view.find("]", pos2);
    return pos3 != std::string_view::npos;
}

static_assert(has_size_align_format<int>(), "int should have [s:N,a:M] format");
static_assert(has_size_align_format<double>(), "double should have [s:N,a:M] format");
static_assert(has_size_align_format<void*>(), "void* should have [s:N,a:M] format");
static_assert(has_size_align_format<NaturalAlign>(), "struct should have [s:N,a:M] format");
static_assert(has_size_align_format<TestUnion>(), "union should have [s:N,a:M] format");
static_assert(has_size_align_format<int[4]>(), "array should have [s:N,a:M] format");

// ============================================================================
// Test 12: Alignment Values Match alignof()
// ============================================================================

template<typename T>
consteval bool alignment_matches_alignof() {
    constexpr auto sig = get_layout_signature<T>();
    constexpr size_t expected_align = alignof(T);
    
    // Build expected pattern ",a:N]"
    char pattern[16] = ",a:";
    size_t pos = 3;
    size_t n = expected_align;
    if (n == 0) {
        pattern[pos++] = '0';
    } else {
        char digits[20];
        int digit_count = 0;
        while (n > 0) {
            digits[digit_count++] = '0' + (n % 10);
            n /= 10;
        }
        for (int i = digit_count - 1; i >= 0; --i) {
            pattern[pos++] = digits[i];
        }
    }
    pattern[pos++] = ']';
    pattern[pos] = '\0';
    
    auto view = to_view(sig);
    // Simple substring search
    for (size_t i = 0; i + pos <= view.size(); ++i) {
        bool match = true;
        for (size_t j = 0; j < pos && match; ++j) {
            if (view[i + j] != pattern[j]) match = false;
        }
        if (match) return true;
    }
    return false;
}

static_assert(alignment_matches_alignof<int8_t>(), "int8_t alignment should match");
static_assert(alignment_matches_alignof<int32_t>(), "int32_t alignment should match");
static_assert(alignment_matches_alignof<int64_t>(), "int64_t alignment should match");
static_assert(alignment_matches_alignof<double>(), "double alignment should match");
static_assert(alignment_matches_alignof<Aligned16>(), "Aligned16 alignment should match");

// ============================================================================
// Main
// -----------------------------------------------------------------------------

int main() {
    // All tests are compile-time static_asserts
    // If we reach here, all alignment tests passed
    return 0;
}