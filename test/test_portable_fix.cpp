// Test for is_trivially_serializable() - verifies pointers and references are excluded
#include <boost/typelayout.hpp>
#include <iostream>

using namespace boost::typelayout;

// Test types
struct GoodType { int32_t x; float y; };
struct BadPointer { int* ptr; };
struct NestedBadPointer { GoodType good; int* bad; };

int main() {
    std::cout << "=== Trivial Serialization Check Verification ===\n\n";
    
    std::cout << "is_trivially_serializable<int>() = " << is_trivially_serializable<int>() << " (expected: 1)\n";
    std::cout << "is_trivially_serializable<int*>() = " << is_trivially_serializable<int*>() << " (expected: 0)\n";
    std::cout << "is_trivially_serializable<void*>() = " << is_trivially_serializable<void*>() << " (expected: 0)\n";
    std::cout << "is_trivially_serializable<nullptr_t>() = " << is_trivially_serializable<std::nullptr_t>() << " (expected: 0)\n";
    std::cout << "is_trivially_serializable<GoodType>() = " << is_trivially_serializable<GoodType>() << " (expected: 1)\n";
    std::cout << "is_trivially_serializable<BadPointer>() = " << is_trivially_serializable<BadPointer>() << " (expected: 0)\n";
    std::cout << "is_trivially_serializable<NestedBadPointer>() = " << is_trivially_serializable<NestedBadPointer>() << " (expected: 0)\n";
    
    // Static assertions - these will fail to compile if is_trivially_serializable is wrong
    static_assert(is_trivially_serializable<int>() == true, "int should be serializable");
    static_assert(is_trivially_serializable<int*>() == false, "int* should NOT be serializable");
    static_assert(is_trivially_serializable<void*>() == false, "void* should NOT be serializable");
    static_assert(is_trivially_serializable<std::nullptr_t>() == false, "nullptr_t should NOT be serializable");
    static_assert(is_trivially_serializable<GoodType>() == true, "GoodType should be serializable");
    static_assert(is_trivially_serializable<BadPointer>() == false, "BadPointer should NOT be serializable");
    static_assert(is_trivially_serializable<NestedBadPointer>() == false, "NestedBadPointer should NOT be serializable");
    
    // Test concepts
    static_assert(TriviallySerializable<GoodType>, "GoodType should satisfy TriviallySerializable");
    static_assert(!TriviallySerializable<BadPointer>, "BadPointer should not satisfy TriviallySerializable");
    
    std::cout << "\n✅ All static_assert passed!\n";
    return 0;
}