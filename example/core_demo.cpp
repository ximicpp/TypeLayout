// Boost.TypeLayout
//
// Core Layer Demo - Layout Signature Engine
//
// This example demonstrates the CORE functionality of TypeLayout:
// - Layout signature generation
// - Layout compatibility checking
// - LayoutCompatible and LayoutMatch concepts
//
// No serialization utilities are included - this is pure layout analysis.
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#include <boost/typelayout/typelayout.hpp>  // Core layer only
#include <iostream>
#include <cstdint>

using namespace boost::typelayout;

// =========================================================================
// Example Types
// =========================================================================

struct Point2D {
    int32_t x;
    int32_t y;
};

// Coordinate has SAME layout as Point2D (same member names too for signature match)
struct Coordinate {
    int32_t x;
    int32_t y;
};

struct Point3D {
    int32_t x;
    int32_t y;
    int32_t z;
};

struct NetworkPacket {
    uint32_t magic;
    uint32_t version;
    uint64_t timestamp;
    uint8_t payload[64];
};

// =========================================================================
// Core Concepts Demo
// =========================================================================

// Use LayoutCompatible concept for type-safe operations
template <typename T, typename U>
    requires LayoutCompatible<T, U>
void safe_memcpy_between(T& dst, const U& src) {
    static_assert(sizeof(T) == sizeof(U));
    std::memcpy(&dst, &src, sizeof(T));
}

// Use LayoutMatch concept for expected signature verification
template <typename T>
    requires LayoutMatch<T, "[64-le]struct[s:8,a:4]{@0[x]:i32[s:4,a:4],@4[y]:i32[s:4,a:4]}">
void process_point(const T& p) {
    std::cout << "Processing point with verified layout\n";
}

// =========================================================================
// Main Demo
// =========================================================================

int main() {
    std::cout << "=== Boost.TypeLayout Core Demo ===\n\n";
    
    // 1. Layout Signature Generation
    std::cout << "1. Layout Signature Generation\n";
    std::cout << "   Point2D: " << get_layout_signature<Point2D>().c_str() << "\n";
    std::cout << "   Point3D: " << get_layout_signature<Point3D>().c_str() << "\n";
    std::cout << "   NetworkPacket: " << get_layout_signature<NetworkPacket>().c_str() << "\n\n";
    
    // 2. Layout Hash Generation
    std::cout << "2. Layout Hash Generation\n";
    constexpr auto hash_2d = get_layout_hash<Point2D>();
    constexpr auto hash_coord = get_layout_hash<Coordinate>();
    constexpr auto hash_3d = get_layout_hash<Point3D>();
    
    std::cout << "   Point2D hash:    0x" << std::hex << hash_2d << "\n";
    std::cout << "   Coordinate hash: 0x" << hash_coord << "\n";
    std::cout << "   Point3D hash:    0x" << hash_3d << "\n\n" << std::dec;
    
    // 3. Layout Compatibility Checking
    std::cout << "3. Layout Compatibility Checking\n";
    
    // Point2D and Coordinate have identical layouts (different names, same structure)
    constexpr bool point_coord_match = signatures_match<Point2D, Coordinate>();
    std::cout << "   Point2D vs Coordinate: " << (point_coord_match ? "COMPATIBLE" : "INCOMPATIBLE") << "\n";
    
    // Point2D and Point3D have different layouts
    constexpr bool point2d_3d_match = signatures_match<Point2D, Point3D>();
    std::cout << "   Point2D vs Point3D:    " << (point2d_3d_match ? "COMPATIBLE" : "INCOMPATIBLE") << "\n\n";
    
    // 4. LayoutVerification (Dual-Hash)
    std::cout << "4. Layout Verification (Dual-Hash)\n";
    constexpr auto verification = get_layout_verification<NetworkPacket>();
    std::cout << "   NetworkPacket:\n";
    std::cout << "     FNV-1a hash: 0x" << std::hex << verification.fnv1a << "\n";
    std::cout << "     DJB2 hash:   0x" << verification.djb2 << "\n";
    std::cout << "     Length:      " << std::dec << verification.length << " chars\n\n";
    
    // 5. Concept-Constrained Functions
    std::cout << "5. Concept-Constrained Functions\n";
    
    Point2D p1{10, 20};
    Coordinate c1{0, 0};
    
    // This works because Point2D and Coordinate are LayoutCompatible
    safe_memcpy_between(c1, p1);
    std::cout << "   Copied Point2D to Coordinate: (" << c1.x << ", " << c1.y << ")\n";
    
    // This works because Point2D matches the expected signature
    process_point(p1);
    
    // 6. Compile-Time Assertions
    std::cout << "\n6. Compile-Time Assertions\n";
    
    // Static assertions for layout verification
    static_assert(LayoutCompatible<Point2D, Coordinate>, 
                  "Point2D and Coordinate must have compatible layouts");
    
    static_assert(!LayoutCompatible<Point2D, Point3D>, 
                  "Point2D and Point3D should have different layouts");
    
    static_assert(hashes_match<Point2D, Coordinate>(),
                  "Point2D and Coordinate should have matching hashes");
    
    std::cout << "   All compile-time assertions passed!\n";
    
    std::cout << "\n=== Core Demo Complete ===\n";
    return 0;
}
