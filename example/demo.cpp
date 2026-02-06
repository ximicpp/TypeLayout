// Boost.TypeLayout - Demo Examples (v2.0 Two-Layer Signature System)
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// This demo showcases the two-layer signature system:
//   Layer 1 (Layout):     Pure byte layout — flattened, no names
//   Layer 2 (Definition): Full type definition — with names, inheritance tree
//
// Mathematical relationship: Layout = project(Definition)

#include <iostream>
#include <cstdint>
#include <boost/typelayout/typelayout.hpp>

using namespace boost::typelayout;

// ============================================================================
// Part 1: Basic struct (POD-like)
// ============================================================================

struct Point { int32_t x, y; };
struct Player {
    uint64_t id;
    char name[32];
    Point pos;
    float health;
};

// Bind to Layout signature (compilation fails if layout changes)
TYPELAYOUT_BIND_LAYOUT(Point, "[64-le]record[s:8,a:4]{@0:i32[s:4,a:4],@4:i32[s:4,a:4]}");

// ============================================================================
// Part 2: Classes with private members and constructors (Non-POD)
// ============================================================================

class Entity {
public:
    Entity(uint64_t id) : id_(id), active_(true) {}
    uint64_t getId() const { return id_; }
    bool isActive() const { return active_; }
    
private:
    uint64_t id_;      // Private members are fully reflected!
    bool active_;
};

// Non-POD class with private members - TypeLayout reflects ALL fields
static_assert(LayoutSupported<Entity>, "Entity class is supported");

// ============================================================================
// Part 3: Class Inheritance — Two-Layer Difference
// ============================================================================

class Base {
public:
    int32_t baseValue;
};

class Derived : public Base {
public:
    int32_t derivedValue;
};

// A flat struct with identical byte layout to Derived
struct Flat {
    int32_t a;
    int32_t b;
};

// KEY INSIGHT: Layout matches (same bytes), but Definition differs (inheritance vs flat)
static_assert(layout_signatures_match<Derived, Flat>(),
    "Derived and Flat have identical byte layout");
static_assert(!definition_signatures_match<Derived, Flat>(),
    "Derived and Flat have different type definitions");

// Multiple inheritance
class Mixin {
public:
    float mixinData;
};

class MultiDerived : public Base, public Mixin {
public:
    int32_t ownData;
};

static_assert(LayoutSupported<MultiDerived>, "Multiple inheritance is supported");

// ============================================================================
// Part 4: Polymorphic Classes (with virtual functions)
// ============================================================================

class IShape {
public:
    virtual ~IShape() = default;
    virtual double area() const = 0;
    
protected:
    int32_t id_;
};

class Circle : public IShape {
public:
    double area() const override { return 3.14159 * radius_ * radius_; }
    
private:
    double radius_;
};

// Polymorphic classes include vtable pointer in layout
// Definition signature will contain "polymorphic" marker
static_assert(LayoutSupported<IShape>, "Abstract polymorphic class is supported");
static_assert(LayoutSupported<Circle>, "Polymorphic derived class is supported");

// ============================================================================
// Part 5: Mixed Access Specifiers
// ============================================================================

class MixedAccess {
public:
    int32_t pub1;
    int32_t pub2;
    
protected:
    int32_t prot1;
    
private:
    int32_t priv1;
    int32_t priv2;
};

// ALL members (public, protected, private) are reflected
static_assert(LayoutSupported<MixedAccess>, "Mixed access class is supported");

// ============================================================================
// Part 6: Layout compatibility check (Two-Layer)
// ============================================================================

struct Vec2 { int32_t x, y; };

// Layout layer: same byte layout
static_assert(layout_signatures_match<Point, Vec2>(),
    "Point and Vec2 must have same byte layout");

// Definition layer: different field names → different definition
static_assert(!definition_signatures_match<Point, Vec2>(),
    "Point and Vec2 have different field names in Definition layer");

// ============================================================================
// Part 7: Compile-time hash values
// ============================================================================

constexpr uint64_t POINT_LAYOUT_HASH = get_layout_hash<Point>();
static_assert(layout_hashes_match<Point, Vec2>(),
    "Point and Vec2 must have same layout hash");

// Dual-hash verification (FNV-1a + DJB2, ~2^128 collision resistance)
constexpr auto ENTITY_VERIFICATION = get_layout_verification<Entity>();
static_assert(layout_verifications_match<Point, Vec2>(),
    "Point and Vec2 must have same layout verification");

// Type library collision detection (compile-time guarantee)
static_assert(no_hash_collision<Point, Entity, Base, Derived>(), 
              "Hash collision in type library!");

// ============================================================================
// Part 8: Primitives — Both layers produce identical signatures
// ============================================================================

static_assert(get_layout_signature<int32_t>() == get_definition_signature<int32_t>(),
    "Primitives: Layout == Definition");
static_assert(get_layout_signature<double>() == get_definition_signature<double>(),
    "Primitives: Layout == Definition");

// ============================================================================
// Main - Demo Output
// ============================================================================

int main() {
    std::cout << "=== TypeLayout v2.0 — Two-Layer Signature Demo ===\n";
    std::cout << "Layer 1 (Layout):     Pure byte layout, flattened, no names\n";
    std::cout << "Layer 2 (Definition): Full type definition, tree, with names\n\n";
    
    // --- Basic struct ---
    std::cout << "--- Basic Struct (POD-like) ---\n";
    std::cout << "Point Layout:     " << get_layout_signature_cstr<Point>() << "\n";
    std::cout << "Point Definition: " << get_definition_signature_cstr<Point>() << "\n\n";
    
    // --- Non-POD class with private members ---
    std::cout << "--- Non-POD Class (private members) ---\n";
    std::cout << "Entity Layout:     " << get_layout_signature_cstr<Entity>() << "\n";
    std::cout << "Entity Definition: " << get_definition_signature_cstr<Entity>() << "\n\n";
    
    // --- Inheritance (the key two-layer difference) ---
    std::cout << "--- Inheritance: Two-Layer Difference ---\n";
    std::cout << "Derived Layout:     " << get_layout_signature_cstr<Derived>() << "\n";
    std::cout << "Derived Definition: " << get_definition_signature_cstr<Derived>() << "\n";
    std::cout << "Flat Layout:        " << get_layout_signature_cstr<Flat>() << "\n";
    std::cout << "Flat Definition:    " << get_definition_signature_cstr<Flat>() << "\n";
    std::cout << "  Derived == Flat (Layout)?      " 
              << (layout_signatures_match<Derived, Flat>() ? "YES" : "NO") << "\n";
    std::cout << "  Derived == Flat (Definition)?   " 
              << (definition_signatures_match<Derived, Flat>() ? "YES" : "NO") << "\n\n";
    
    // --- Polymorphic classes ---
    std::cout << "--- Polymorphic Classes (virtual functions) ---\n";
    std::cout << "IShape Layout:     " << get_layout_signature_cstr<IShape>() << "\n";
    std::cout << "IShape Definition: " << get_definition_signature_cstr<IShape>() << "\n";
    std::cout << "Circle Layout:     " << get_layout_signature_cstr<Circle>() << "\n";
    std::cout << "Circle Definition: " << get_definition_signature_cstr<Circle>() << "\n\n";
    
    // --- Mixed access ---
    std::cout << "--- Mixed Access Specifiers ---\n";
    std::cout << "MixedAccess Layout:     " << get_layout_signature_cstr<MixedAccess>() << "\n";
    std::cout << "MixedAccess Definition: " << get_definition_signature_cstr<MixedAccess>() << "\n\n";
    
    // --- Primitives ---
    std::cout << "--- Primitive Types (both layers identical) ---\n";
    std::cout << "int32_t: " << get_layout_signature_cstr<int32_t>() << "\n";
    std::cout << "double:  " << get_layout_signature_cstr<double>() << "\n";
    std::cout << "void*:   " << get_layout_signature_cstr<void*>() << "\n\n";
    
    // --- Layout compatibility ---
    std::cout << "--- Layout Compatibility ---\n";
    std::cout << "Point == Vec2 (Layout)?      " 
              << (layout_signatures_match<Point, Vec2>() ? "YES" : "NO") << "\n";
    std::cout << "Point == Vec2 (Definition)?  " 
              << (definition_signatures_match<Point, Vec2>() ? "YES" : "NO") << "\n";
    
    // --- Layout hashes ---
    std::cout << "\n--- Layout Hashes ---\n";
    std::cout << "Point hash:  0x" << std::hex << POINT_LAYOUT_HASH << "\n";
    std::cout << "Entity hash: 0x" << get_layout_hash<Entity>() << std::dec << "\n";
    
    std::cout << "\n=== All compile-time checks passed! ===\n";
    std::cout << "TypeLayout v2.0: Layout=project(Definition)\n";
    return 0;
}