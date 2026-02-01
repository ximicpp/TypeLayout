// Boost.TypeLayout
//
// Extended Signature Tests for Additional Type Categories
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#include <boost/typelayout/signature.hpp>
#include <print>
#include <array>
#include <utility>
#include <complex>
#include <span>
#include <string_view>

using namespace boost::typelayout;

// ============================================================================
// CATEGORY 1: Standard Containers (P1)
// ============================================================================

void test_std_array() {
    std::println("=== std::array Tests ===");
    
    // std::array<int, 4>
    {
        using T = std::array<int, 4>;
        constexpr auto sig = get_layout_signature_cstr<T>();
        std::println("std::array<int, 4>: {}", sig);
        std::println("  sizeof: {}, alignof: {}", sizeof(T), alignof(T));
    }
    
    // std::array<double, 2>
    {
        using T = std::array<double, 2>;
        constexpr auto sig = get_layout_signature_cstr<T>();
        std::println("std::array<double, 2>: {}", sig);
        std::println("  sizeof: {}, alignof: {}", sizeof(T), alignof(T));
    }
    
    // Empty array
    {
        using T = std::array<int, 0>;
        constexpr auto sig = get_layout_signature_cstr<T>();
        std::println("std::array<int, 0>: {}", sig);
        std::println("  sizeof: {}, alignof: {}", sizeof(T), alignof(T));
    }
}

void test_std_pair() {
    std::println("\n=== std::pair Tests ===");
    
    // std::pair<int, double>
    {
        using T = std::pair<int, double>;
        constexpr auto sig = get_layout_signature_cstr<T>();
        std::println("std::pair<int, double>: {}", sig);
        std::println("  sizeof: {}, alignof: {}", sizeof(T), alignof(T));
    }
    
    // std::pair<char, char>
    {
        using T = std::pair<char, char>;
        constexpr auto sig = get_layout_signature_cstr<T>();
        std::println("std::pair<char, char>: {}", sig);
        std::println("  sizeof: {}, alignof: {}", sizeof(T), alignof(T));
    }
    
    // Nested pair
    {
        using T = std::pair<std::pair<int, int>, double>;
        constexpr auto sig = get_layout_signature_cstr<T>();
        std::println("std::pair<std::pair<int, int>, double>: {}", sig);
        std::println("  sizeof: {}, alignof: {}", sizeof(T), alignof(T));
    }
}

// ============================================================================
// CATEGORY 2: Boundary Cases (P2)
// ============================================================================

// Diamond inheritance test
struct DiamondBase {
    int base_val;
};

struct DiamondLeft : virtual DiamondBase {
    int left_val;
};

struct DiamondRight : virtual DiamondBase {
    int right_val;
};

struct DiamondDerived : DiamondLeft, DiamondRight {
    int derived_val;
};

void test_diamond_inheritance() {
    std::println("\n=== Diamond Inheritance Tests ===");
    
    {
        using T = DiamondDerived;
        constexpr auto sig = get_layout_signature_cstr<T>();
        std::println("DiamondDerived: {}", sig);
        std::println("  sizeof: {}, alignof: {}", sizeof(T), alignof(T));
    }
}

// Mutable member test
struct MutableTest {
    int normal_val;
    mutable int mutable_val;
    const int const_val = 42;
};

void test_mutable_members() {
    std::println("\n=== Mutable Member Tests ===");
    
    {
        using T = MutableTest;
        constexpr auto sig = get_layout_signature_cstr<T>();
        std::println("MutableTest: {}", sig);
        std::println("  sizeof: {}, alignof: {}", sizeof(T), alignof(T));
    }
}

// Cross-boundary bit-fields
struct CrossBoundaryBitfield {
    uint32_t a : 20;  // bits 0-19
    uint32_t b : 20;  // bits 20-39 (crosses 32-bit boundary)
    uint32_t c : 8;   // bits 40-47
};

void test_cross_boundary_bitfields() {
    std::println("\n=== Cross-Boundary Bit-field Tests ===");
    
    {
        using T = CrossBoundaryBitfield;
        constexpr auto sig = get_layout_signature_cstr<T>();
        std::println("CrossBoundaryBitfield: {}", sig);
        std::println("  sizeof: {}, alignof: {}", sizeof(T), alignof(T));
    }
}

// Recursive type (self-referential via pointer)
struct RecursiveNode {
    int value;
    RecursiveNode* next;
    RecursiveNode* prev;
};

void test_recursive_types() {
    std::println("\n=== Recursive Type Tests ===");
    
    {
        using T = RecursiveNode;
        constexpr auto sig = get_layout_signature_cstr<T>();
        std::println("RecursiveNode: {}", sig);
        std::println("  sizeof: {}, alignof: {}", sizeof(T), alignof(T));
    }
}

// Lambda types
void test_lambda_types() {
    std::println("\n=== Lambda Type Tests ===");
    
    // Stateless lambda
    {
        auto stateless = []() { return 42; };
        using T = decltype(stateless);
        constexpr auto sig = get_layout_signature_cstr<T>();
        std::println("Stateless lambda: {}", sig);
        std::println("  sizeof: {}, alignof: {}", sizeof(T), alignof(T));
    }
    
    // Capturing lambda
    {
        int x = 10;
        double y = 3.14;
        auto capturing = [x, y]() { return x + y; };
        using T = decltype(capturing);
        constexpr auto sig = get_layout_signature_cstr<T>();
        std::println("Capturing lambda [int, double]: {}", sig);
        std::println("  sizeof: {}, alignof: {}", sizeof(T), alignof(T));
    }
}

// ============================================================================
// CATEGORY 3: Complex and View Types
// ============================================================================

void test_complex_types() {
    std::println("\n=== std::complex Tests ===");
    
    // std::complex<float>
    {
        using T = std::complex<float>;
        constexpr auto sig = get_layout_signature_cstr<T>();
        std::println("std::complex<float>: {}", sig);
        std::println("  sizeof: {}, alignof: {}", sizeof(T), alignof(T));
    }
    
    // std::complex<double>
    {
        using T = std::complex<double>;
        constexpr auto sig = get_layout_signature_cstr<T>();
        std::println("std::complex<double>: {}", sig);
        std::println("  sizeof: {}, alignof: {}", sizeof(T), alignof(T));
    }
}

void test_string_view_span() {
    std::println("\n=== std::string_view / std::span Tests ===");
    
    // std::string_view
    {
        using T = std::string_view;
        constexpr auto sig = get_layout_signature_cstr<T>();
        std::println("std::string_view: {}", sig);
        std::println("  sizeof: {}, alignof: {}", sizeof(T), alignof(T));
    }
    
    // std::span<int>
    {
        using T = std::span<int>;
        constexpr auto sig = get_layout_signature_cstr<T>();
        std::println("std::span<int>: {}", sig);
        std::println("  sizeof: {}, alignof: {}", sizeof(T), alignof(T));
    }
    
    // std::span<int, 4> (static extent)
    {
        using T = std::span<int, 4>;
        constexpr auto sig = get_layout_signature_cstr<T>();
        std::println("std::span<int, 4>: {}", sig);
        std::println("  sizeof: {}, alignof: {}", sizeof(T), alignof(T));
    }
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::println("===================================================");
    std::println("TypeLayout Extended Signature Tests");
    std::println("===================================================\n");
    
    // P1: Standard containers
    test_std_array();
    test_std_pair();
    
    // P2: Boundary cases
    test_diamond_inheritance();
    test_mutable_members();
    test_cross_boundary_bitfields();
    test_recursive_types();
    test_lambda_types();
    
    // P3: Complex and view types
    test_complex_types();
    test_string_view_span();
    
    // NOTE: std::atomic tests temporarily disabled - requires _Atomic(T) specialization
    // This is tracked as a future enhancement
    
    std::println("\n===================================================");
    std::println("All extended tests completed!");
    std::println("===================================================");
    
    return 0;
}