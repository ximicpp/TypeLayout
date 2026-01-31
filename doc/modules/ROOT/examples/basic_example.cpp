// Test file for verifying documentation examples compile correctly
// Build with: clang++ -std=c++26 -freflection -I../../include test_basic_example.cpp -o test_basic

#include <boost/typelayout.hpp>
#include <iostream>
#include <cstdint>

// Example 1: Basic struct signature
struct Point {
    int32_t x;
    int32_t y;
};

// Example 2: Nested struct
struct Rectangle {
    Point top_left;
    Point bottom_right;
};

// Example 3: Portable struct with fixed-width types
struct NetworkHeader {
    uint32_t magic;
    uint16_t version;
    uint16_t flags;
    uint32_t payload_size;
};

// Example 4: Non-portable struct (uses wchar_t)
struct NonPortable {
    wchar_t wide_char;   // 2 bytes on Windows, 4 on Linux
    long value;          // 4 bytes on Windows, 8 on Linux
};

// Example 5: Struct with array
struct Buffer {
    uint32_t length;
    char data[256];
};

// Example 6: Struct with inheritance
struct Base {
    int32_t id;
};

struct Derived : Base {
    int32_t value;
};

// Example 7: Polymorphic class
class Polymorphic {
public:
    virtual ~Polymorphic() = default;
    int32_t data;
};

// Example 8: Bit-fields
struct Flags {
    uint8_t enabled : 1;
    uint8_t mode : 3;
    uint8_t reserved : 4;
};

int main() {
    using namespace boost::typelayout;
    
    std::cout << "=== Boost.TypeLayout Documentation Example Verification ===\n\n";
    
    // Test 1: Basic signature
    constexpr auto point_sig = get_layout_signature<Point>();
    std::cout << "Point signature: " << point_sig.c_str() << "\n";
    
    // Test 2: Nested struct
    constexpr auto rect_sig = get_layout_signature<Rectangle>();
    std::cout << "Rectangle signature: " << rect_sig.c_str() << "\n";
    
    // Test 3: Hash
    constexpr auto point_hash = get_layout_hash<Point>();
    std::cout << "Point hash: 0x" << std::hex << point_hash << std::dec << "\n";
    
    // Test 4: Portability check
    std::cout << "\nPortability checks:\n";
    std::cout << "  Point is portable: " << (is_portable<Point>() ? "yes" : "no") << "\n";
    std::cout << "  NetworkHeader is portable: " << (is_portable<NetworkHeader>() ? "yes" : "no") << "\n";
    std::cout << "  NonPortable is portable: " << (is_portable<NonPortable>() ? "yes" : "no") << "\n";
    
    // Test 5: Signature matching
    std::cout << "\nSignature matching:\n";
    std::cout << "  Point == Point: " << (signatures_match<Point, Point>() ? "yes" : "no") << "\n";
    std::cout << "  Point == Rectangle: " << (signatures_match<Point, Rectangle>() ? "yes" : "no") << "\n";
    
    // Test 6: Dual-hash verification
    constexpr auto verification = get_layout_verification<Point>();
    std::cout << "\nDual-hash verification for Point:\n";
    std::cout << "  FNV-1a: 0x" << std::hex << verification.fnv1a << "\n";
    std::cout << "  DJB2:   0x" << verification.djb2 << "\n";
    std::cout << "  Length: " << std::dec << verification.length << " chars\n";
    
    // Test 7: Bit-field detection
    std::cout << "\nBit-field detection:\n";
    std::cout << "  Point has bitfields: " << (has_bitfields<Point>() ? "yes" : "no") << "\n";
    std::cout << "  Flags has bitfields: " << (has_bitfields<Flags>() ? "yes" : "no") << "\n";
    
    // Test 8: Array signature
    constexpr auto buffer_sig = get_layout_signature<Buffer>();
    std::cout << "\nBuffer signature: " << buffer_sig.c_str() << "\n";
    
    // Test 9: Inheritance
    constexpr auto derived_sig = get_layout_signature<Derived>();
    std::cout << "\nDerived signature: " << derived_sig.c_str() << "\n";
    
    // Test 10: Concepts (compile-time check)
    static_assert(Portable<Point>, "Point should be portable");
    static_assert(Portable<NetworkHeader>, "NetworkHeader should be portable");
    static_assert(!Portable<NonPortable>, "NonPortable should NOT be portable");
    static_assert(LayoutCompatible<Point, Point>, "Point should be compatible with itself");
    
    std::cout << "\n=== All tests passed! ===\n";
    return 0;
}
