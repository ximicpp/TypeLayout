// Boost.TypeLayout - Demo Examples
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// This demo showcases the core layout signature features.
// TypeLayout supports ALL C++ types: struct, class, inheritance, polymorphism, etc.

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

// Bind types to "golden" layout signatures (compilation fails if layout changes)
// Note: Structural mode omits member names for semantic equivalence
TYPELAYOUT_BIND(Point, "[64-le]struct[s:8,a:4]{@0:i32[s:4,a:4],@4:i32[s:4,a:4]}");

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
// Part 3: Class Inheritance (Single, Multiple)
// ============================================================================

class Base {
public:
    int32_t baseValue;
};

class Derived : public Base {
public:
    int32_t derivedValue;
};

// Inheritance is properly reflected with base class info
static_assert(LayoutSupported<Derived>, "Derived class with inheritance is supported");

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
// Signature will contain "polymorphic" marker
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
// Part 6: Layout compatibility check
// ============================================================================

struct Vec2 { int32_t x, y; };
static_assert(signatures_match<Point, Vec2>(), "Point and Vec2 must have same layout");

// Template constraint using layout signature (Structural mode - no names)
template<typename T>
    requires LayoutMatch<T, "[64-le]struct[s:8,a:4]{@0:i32[s:4,a:4],@4:i32[s:4,a:4]}">
void send_point(const T& p) {
    std::cout << "Sending point-like data...\n";
    (void)p;
}

// ============================================================================
// Part 7: Compile-time hash values
// ============================================================================

constexpr uint64_t POINT_LAYOUT_HASH = get_layout_hash<Point>();
static_assert(hashes_match<Point, Vec2>(), "Point and Vec2 must have same layout hash");

// Dual-hash verification (FNV-1a + DJB2, ~2^128 collision resistance)
constexpr auto ENTITY_VERIFICATION = get_layout_verification<Entity>();
static_assert(verifications_match<Point, Vec2>(), "Point and Vec2 must have same verification");

// Type library collision detection (compile-time guarantee)
static_assert(no_hash_collision<Point, Entity, Base, Derived>(), 
              "Hash collision in type library!");

// Template constraint using hash
template<typename T>
    requires LayoutHashMatch<T, POINT_LAYOUT_HASH>
void process_point_data(const T& p) {
    std::cout << "Processing point data (hash validated)...\n";
    (void)p;
}

// ============================================================================
// Main - Demo Output
// ============================================================================

int main() {
    std::cout << "=== TypeLayout Demo ===\n";
    std::cout << "Supports: struct, class, inheritance, polymorphism, private members\n\n";
    
    // --- Basic struct ---
    std::cout << "--- Basic Struct (POD-like) ---\n";
    std::cout << "Point:  " << get_layout_signature_cstr<Point>() << "\n\n";
    
    // --- Non-POD class with private members ---
    std::cout << "--- Non-POD Class (private members) ---\n";
    std::cout << "Entity: " << get_layout_signature_cstr<Entity>() << "\n\n";
    
    // --- Inheritance ---
    std::cout << "--- Class Inheritance ---\n";
    std::cout << "Base:        " << get_layout_signature_cstr<Base>() << "\n";
    std::cout << "Derived:     " << get_layout_signature_cstr<Derived>() << "\n";
    std::cout << "MultiDerived:" << get_layout_signature_cstr<MultiDerived>() << "\n\n";
    
    // --- Polymorphic classes ---
    std::cout << "--- Polymorphic Classes (virtual functions) ---\n";
    std::cout << "IShape: " << get_layout_signature_cstr<IShape>() << "\n";
    std::cout << "Circle: " << get_layout_signature_cstr<Circle>() << "\n\n";
    
    // --- Mixed access ---
    std::cout << "--- Mixed Access Specifiers ---\n";
    std::cout << "MixedAccess: " << get_layout_signature_cstr<MixedAccess>() << "\n\n";
    
    // --- Primitives ---
    std::cout << "--- Primitive Types ---\n";
    std::cout << "int32_t: " << get_layout_signature_cstr<int32_t>() << "\n";
    std::cout << "double:  " << get_layout_signature_cstr<double>() << "\n";
    std::cout << "void*:   " << get_layout_signature_cstr<void*>() << "\n\n";
    
    // --- Layout compatibility ---
    std::cout << "--- Layout Compatibility ---\n";
    std::cout << "Point == Vec2: " << (signatures_match<Point, Vec2>() ? "yes" : "no") << "\n";
    
    Point p{10, 20};
    Vec2 v{30, 40};
    send_point(p);
    send_point(v);
    
    // --- Layout hashes ---
    std::cout << "\n--- Layout Hashes ---\n";
    std::cout << "Point hash:  0x" << std::hex << POINT_LAYOUT_HASH << "\n";
    std::cout << "Entity hash: 0x" << get_layout_hash<Entity>() << std::dec << "\n";
    
    process_point_data(p);
    process_point_data(v);
    
    std::cout << "\n=== All compile-time checks passed! ===\n";
    std::cout << "TypeLayout fully supports classes, inheritance, and polymorphism.\n";
    return 0;
}