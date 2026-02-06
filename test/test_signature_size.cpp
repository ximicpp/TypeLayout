// Boost.TypeLayout
//
// Signature Size Analysis - Measure signature string lengths for large structs
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#include <boost/typelayout.hpp>
#include <cstdint>
#include <iostream>
#include <iomanip>

using namespace boost::typelayout;

// ============================================================================
// Test Structs: Various Sizes
// ============================================================================

// 20 members
struct S20 {
    int32_t m00, m01, m02, m03, m04, m05, m06, m07, m08, m09;
    int32_t m10, m11, m12, m13, m14, m15, m16, m17, m18, m19;
};

// 40 members
struct S40 {
    int32_t m00, m01, m02, m03, m04, m05, m06, m07, m08, m09;
    int32_t m10, m11, m12, m13, m14, m15, m16, m17, m18, m19;
    int32_t m20, m21, m22, m23, m24, m25, m26, m27, m28, m29;
    int32_t m30, m31, m32, m33, m34, m35, m36, m37, m38, m39;
};

// 60 members
struct S60 {
    int32_t m00, m01, m02, m03, m04, m05, m06, m07, m08, m09;
    int32_t m10, m11, m12, m13, m14, m15, m16, m17, m18, m19;
    int32_t m20, m21, m22, m23, m24, m25, m26, m27, m28, m29;
    int32_t m30, m31, m32, m33, m34, m35, m36, m37, m38, m39;
    int32_t m40, m41, m42, m43, m44, m45, m46, m47, m48, m49;
    int32_t m50, m51, m52, m53, m54, m55, m56, m57, m58, m59;
};

// 80 members
struct S80 {
    int32_t m00, m01, m02, m03, m04, m05, m06, m07, m08, m09;
    int32_t m10, m11, m12, m13, m14, m15, m16, m17, m18, m19;
    int32_t m20, m21, m22, m23, m24, m25, m26, m27, m28, m29;
    int32_t m30, m31, m32, m33, m34, m35, m36, m37, m38, m39;
    int32_t m40, m41, m42, m43, m44, m45, m46, m47, m48, m49;
    int32_t m50, m51, m52, m53, m54, m55, m56, m57, m58, m59;
    int32_t m60, m61, m62, m63, m64, m65, m66, m67, m68, m69;
    int32_t m70, m71, m72, m73, m74, m75, m76, m77, m78, m79;
};

// 100 members
struct S100 {
    int32_t m00, m01, m02, m03, m04, m05, m06, m07, m08, m09;
    int32_t m10, m11, m12, m13, m14, m15, m16, m17, m18, m19;
    int32_t m20, m21, m22, m23, m24, m25, m26, m27, m28, m29;
    int32_t m30, m31, m32, m33, m34, m35, m36, m37, m38, m39;
    int32_t m40, m41, m42, m43, m44, m45, m46, m47, m48, m49;
    int32_t m50, m51, m52, m53, m54, m55, m56, m57, m58, m59;
    int32_t m60, m61, m62, m63, m64, m65, m66, m67, m68, m69;
    int32_t m70, m71, m72, m73, m74, m75, m76, m77, m78, m79;
    int32_t m80, m81, m82, m83, m84, m85, m86, m87, m88, m89;
    int32_t m90, m91, m92, m93, m94, m95, m96, m97, m98, m99;
};

// ============================================================================
// Main - Output signature sizes
// ============================================================================

int main() {
    std::cout << "=== TypeLayout Signature Size Analysis ===\n";
    std::cout << "(Structural mode - no field names)\n\n";
    
    std::cout << std::left << std::setw(12) << "Members"
              << std::setw(12) << "Struct Size"
              << std::setw(16) << "Signature Len"
              << "Chars/Member\n";
    std::cout << std::string(52, '-') << "\n";
    
    // S20
    {
        constexpr auto sig = get_layout_signature<S20>();
        std::cout << std::setw(12) << 20
                  << std::setw(12) << sizeof(S20)
                  << std::setw(16) << sig.length()
                  << std::fixed << std::setprecision(1) 
                  << (double)sig.length() / 20 << "\n";
    }
    
    // S40
    {
        constexpr auto sig = get_layout_signature<S40>();
        std::cout << std::setw(12) << 40
                  << std::setw(12) << sizeof(S40)
                  << std::setw(16) << sig.length()
                  << std::fixed << std::setprecision(1) 
                  << (double)sig.length() / 40 << "\n";
    }
    
    // S60
    {
        constexpr auto sig = get_layout_signature<S60>();
        std::cout << std::setw(12) << 60
                  << std::setw(12) << sizeof(S60)
                  << std::setw(16) << sig.length()
                  << std::fixed << std::setprecision(1) 
                  << (double)sig.length() / 60 << "\n";
    }
    
    // S80
    {
        constexpr auto sig = get_layout_signature<S80>();
        std::cout << std::setw(12) << 80
                  << std::setw(12) << sizeof(S80)
                  << std::setw(16) << sig.length()
                  << std::fixed << std::setprecision(1) 
                  << (double)sig.length() / 80 << "\n";
    }
    
    // S100
    {
        constexpr auto sig = get_layout_signature<S100>();
        std::cout << std::setw(12) << 100
                  << std::setw(12) << sizeof(S100)
                  << std::setw(16) << sig.length()
                  << std::fixed << std::setprecision(1) 
                  << (double)sig.length() / 100 << "\n";
    }
    
    std::cout << "\n=== Detailed Signatures ===\n\n";
    
    std::cout << "S20 signature:\n";
    std::cout << get_layout_signature<S20>().c_str() << "\n\n";
    
    std::cout << "S40 signature (first 200 chars):\n";
    {
        constexpr auto sig = get_layout_signature<S40>();
        std::string s(sig.c_str(), std::min(sig.length(), size_t{200}));
        std::cout << s << "...\n\n";
    }
    
    return 0;
}
