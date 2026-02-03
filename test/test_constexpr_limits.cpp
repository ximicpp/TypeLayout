// Test file for constexpr step limit analysis
// This file intentionally tests the boundary conditions

#include <boost/typelayout/typelayout.hpp>
#include <iostream>
#include <cstdint>
#include <variant>

using namespace boost::typelayout;

// 100 members structure - known to exceed default constexpr step limit
struct Large100 {
    int32_t a01, a02, a03, a04, a05, a06, a07, a08, a09, a10;
    int32_t a11, a12, a13, a14, a15, a16, a17, a18, a19, a20;
    int32_t a21, a22, a23, a24, a25, a26, a27, a28, a29, a30;
    int32_t a31, a32, a33, a34, a35, a36, a37, a38, a39, a40;
    int32_t a41, a42, a43, a44, a45, a46, a47, a48, a49, a50;
    int32_t b01, b02, b03, b04, b05, b06, b07, b08, b09, b10;
    int32_t b11, b12, b13, b14, b15, b16, b17, b18, b19, b20;
    int32_t b21, b22, b23, b24, b25, b26, b27, b28, b29, b30;
    int32_t b31, b32, b33, b34, b35, b36, b37, b38, b39, b40;
    int32_t b41, b42, b43, b44, b45, b46, b47, b48, b49, b50;
};

// std::variant with 10 alternatives - known to exceed default constexpr step limit
using Variant10 = std::variant<int8_t, int16_t, int32_t, int64_t, float, double, char, bool, short, long>;

int main() {
    std::cout << "Testing constexpr step limits...\n" << std::endl;
    
    // Test Large100
    std::cout << "=== Large100 (100 int32_t members) ===" << std::endl;
    constexpr auto sig100 = get_layout_signature<Large100>();
    std::cout << "[PASS] Large100 signature generated" << std::endl;
    std::cout << "sizeof=" << sizeof(Large100) << std::endl;
    // Don't print full signature - too long
    std::cout << "sig length=" << sig100.size << " chars" << std::endl;
    
    // Test Variant10
    std::cout << "\n=== Variant10 (10 type alternatives) ===" << std::endl;
    constexpr auto sigV10 = get_layout_signature<Variant10>();
    std::cout << "[PASS] Variant10 signature generated" << std::endl;
    std::cout << "sizeof=" << sizeof(Variant10) << std::endl;
    std::cout << "sig length=" << sigV10.size << " chars" << std::endl;
    
    std::cout << "\n[SUCCESS] All constexpr limit tests passed with increased step limit!" << std::endl;
    return 0;
}
