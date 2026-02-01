// Boost.TypeLayout
//
// Utility Layer Demo - Serialization Safety Checking
//
// This example demonstrates the UTILITY layer functionality:
// - Serialization safety checking
// - Platform set configuration
// - Serializable, ZeroCopyTransmittable concepts
// - Blocker reason diagnostics
//
// This builds upon the core layout signature engine.
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#include <boost/typelayout/typelayout_util.hpp>  // Utility layer (includes core)
#include <iostream>
#include <cstdint>
#include <cstring>

using namespace boost::typelayout;

// =========================================================================
// Example Types - Serializable
// =========================================================================

struct SafePacket {
    uint32_t magic;
    uint32_t sequence;
    int32_t data[8];
};

struct Coordinate {
    int32_t x;
    int32_t y;
    int32_t z;
};

// =========================================================================
// Example Types - NOT Serializable
// =========================================================================

struct HasPointer {
    int32_t value;
    int32_t* ptr;  // Pointers are not serializable
};

struct HasReference {
    int32_t& ref;  // References are not serializable
};

class Polymorphic {
public:
    virtual ~Polymorphic() = default;  // Virtual functions = not serializable
    int32_t data;
};

struct PlatformDependent {
    wchar_t value;  // 'wchar_t' has different sizes on Windows (2 bytes) vs Linux (4 bytes)!
};

struct HasBitField {
    uint32_t flags : 4;  // Bit-fields have implementation-defined layout
    uint32_t id : 12;
    uint32_t reserved : 16;
};

// =========================================================================
// Serialization-Safe Functions
// =========================================================================

// Only accept types that are safe for network transmission
template <typename T>
    requires Serializable<T>
void network_send(const T& data) {
    std::cout << "   Sending " << sizeof(T) << " bytes over network\n";
    // In real code: send(socket, &data, sizeof(T), 0);
}

// Zero-copy transmittable check
template <typename Src, typename Dst>
    requires ZeroCopyTransmittable<Src, Dst>
void zero_copy_transfer(const Src& src, Dst& dst) {
    std::memcpy(&dst, &src, sizeof(Src));
    std::cout << "   Zero-copy transfer complete\n";
}

// Shared memory safe operation
template <typename T>
    requires SharedMemorySafe<T>
T* create_shared_buffer(const char* name) {
    std::cout << "   Created shared memory buffer for type (size=" << sizeof(T) << ")\n";
    // In real code: shm_open, mmap, etc.
    return nullptr;
}

// =========================================================================
// Main Demo
// =========================================================================

int main() {
    std::cout << "=== Boost.TypeLayout Utility Demo ===\n\n";
    
    // 1. Platform Configuration
    std::cout << "1. Platform Configuration\n";
    constexpr auto current = PlatformSet::current();
    std::cout << "   Current platform: ";
    if (current.bit_width == BitWidth::Bits64) {
        std::cout << "64-bit ";
    } else {
        std::cout << "32-bit ";
    }
    if (current.endianness == Endianness::Little) {
        std::cout << "little-endian\n\n";
    } else {
        std::cout << "big-endian\n\n";
    }
    
    // 2. Serialization Status Checking
    std::cout << "2. Serialization Status\n";
    constexpr auto safe_status = serialization_status<SafePacket>();
    constexpr auto ptr_status = serialization_status<HasPointer>();
    constexpr auto poly_status = serialization_status<Polymorphic>();
    constexpr auto long_status = serialization_status<PlatformDependent>();
    constexpr auto bits_status = serialization_status<HasBitField>();
    
    std::cout << "   SafePacket:        " << safe_status.c_str() << "\n";
    std::cout << "   HasPointer:        " << ptr_status.c_str() << "\n";
    std::cout << "   Polymorphic:       " << poly_status.c_str() << "\n";
    std::cout << "   PlatformDependent: " << long_status.c_str() << "\n";
    std::cout << "   HasBitField:       " << bits_status.c_str() << "\n\n";
    
    // 3. Blocker Reason Diagnostics
    std::cout << "3. Serialization Blockers\n";
    std::cout << "   HasPointer blocked by: " << blocker_reason<HasPointer>() << "\n";
    std::cout << "   Polymorphic blocked by: " << blocker_reason<Polymorphic>() << "\n";
    std::cout << "   PlatformDependent blocked by: " << blocker_reason<PlatformDependent>() << "\n";
    std::cout << "   HasBitField blocked by: " << blocker_reason<HasBitField>() << "\n\n";
    
    // 4. Bit-field Detection
    std::cout << "4. Bit-field Detection\n";
    std::cout << "   SafePacket has bit-fields: " << (has_bitfields<SafePacket>() ? "yes" : "no") << "\n";
    std::cout << "   HasBitField has bit-fields: " << (has_bitfields<HasBitField>() ? "yes" : "no") << "\n\n";
    
    // 5. Serialization-Safe Functions
    std::cout << "5. Serialization-Safe Functions\n";
    
    SafePacket packet{0x12345678, 1, {1, 2, 3, 4, 5, 6, 7, 8}};
    network_send(packet);  // OK: SafePacket is Serializable
    
    // network_send(HasPointer{});  // COMPILE ERROR: HasPointer is not Serializable
    
    // 6. Zero-Copy Transmittable
    std::cout << "\n6. Zero-Copy Transfer\n";
    Coordinate src{100, 200, 300};
    Coordinate dst{0, 0, 0};
    zero_copy_transfer(src, dst);
    std::cout << "   Received: (" << dst.x << ", " << dst.y << ", " << dst.z << ")\n";
    
    // 7. Shared Memory Safe
    std::cout << "\n7. Shared Memory Safe Types\n";
    create_shared_buffer<SafePacket>("safe_packet_buffer");
    create_shared_buffer<Coordinate>("coordinate_buffer");
    
    // 8. Cross-Platform Serialization Check
    std::cout << "\n8. Cross-Platform Checks\n";
    
    constexpr auto target_64le = PlatformSet::bits64_le();
    constexpr auto target_32le = PlatformSet::bits32_le();
    
    constexpr bool safe_on_64 = is_serializable_v<SafePacket, target_64le>;
    constexpr bool safe_on_32 = is_serializable_v<SafePacket, target_32le>;
    
    std::cout << "   SafePacket serializable on 64-bit LE: " << (safe_on_64 ? "yes" : "no") << "\n";
    std::cout << "   SafePacket serializable on 32-bit LE: " << (safe_on_32 ? "yes" : "no") << "\n";
    
    // 9. Compile-Time Assertions
    std::cout << "\n9. Compile-Time Assertions\n";
    
    static_assert(Serializable<SafePacket>, "SafePacket must be serializable");
    static_assert(Serializable<Coordinate>, "Coordinate must be serializable");
    static_assert(!Serializable<HasPointer>, "HasPointer should not be serializable");
    static_assert(!Serializable<Polymorphic>, "Polymorphic should not be serializable");
    static_assert(!Serializable<PlatformDependent>, "PlatformDependent should not be serializable");
    
    // Network safe concept (64-bit LE)
    static_assert(NetworkSafe<SafePacket>, "SafePacket must be network safe");
    
    // Portable layout (works on both 32-bit and 64-bit)
    // Note: This may fail on 32-bit builds if platform doesn't match
    // static_assert(PortableLayout<SafePacket>, "SafePacket should have portable layout");
    
    std::cout << "   All compile-time assertions passed!\n";
    
    std::cout << "\n=== Utility Demo Complete ===\n";
    return 0;
}
