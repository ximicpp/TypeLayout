// Test for is_portable() fix - verifies pointers and references are excluded
#include <boost/typelayout.hpp>
#include <iostream>

using namespace boost::typelayout;

// Test types
struct GoodType { int32_t x; float y; };
struct BadPointer { int* ptr; };
struct NestedBadPointer { GoodType good; int* bad; };

int main() {
    std::cout << "=== Portability Check Fix Verification ===\n\n";
    
    std::cout << "is_portable<int>() = " << is_portable<int>() << " (expected: 1)\n";
    std::cout << "is_portable<int*>() = " << is_portable<int*>() << " (expected: 0)\n";
    std::cout << "is_portable<void*>() = " << is_portable<void*>() << " (expected: 0)\n";
    std::cout << "is_portable<nullptr_t>() = " << is_portable<std::nullptr_t>() << " (expected: 0)\n";
    std::cout << "is_portable<GoodType>() = " << is_portable<GoodType>() << " (expected: 1)\n";
    std::cout << "is_portable<BadPointer>() = " << is_portable<BadPointer>() << " (expected: 0)\n";
    std::cout << "is_portable<NestedBadPointer>() = " << is_portable<NestedBadPointer>() << " (expected: 0)\n";
    
    // Static assertions - these will fail to compile if is_portable is wrong
    static_assert(is_portable<int>() == true, "int should be portable");
    static_assert(is_portable<int*>() == false, "int* should NOT be portable");
    static_assert(is_portable<void*>() == false, "void* should NOT be portable");
    static_assert(is_portable<std::nullptr_t>() == false, "nullptr_t should NOT be portable");
    static_assert(is_portable<GoodType>() == true, "GoodType should be portable");
    static_assert(is_portable<BadPointer>() == false, "BadPointer should NOT be portable");
    static_assert(is_portable<NestedBadPointer>() == false, "NestedBadPointer should NOT be portable");
    
    std::cout << "\n✅ All static_assert passed!\n";
    return 0;
}
