// Advanced test file for verifying all documentation examples
// Build with: clang++ -std=c++26 -freflection -stdlib=libc++ -I../../include test_advanced_example.cpp -o test_advanced

#include <boost/typelayout.hpp>
#include <iostream>
#include <cstdint>
#include <array>

using namespace boost::typelayout;

// ============================================================================
// Network Protocol Example (from network-protocol.adoc)
// ============================================================================

namespace protocol_v1 {
    struct Header {
        uint32_t magic;
        uint16_t version;
        uint16_t type;
        uint32_t payload_size;
        uint64_t timestamp;
    };
    
    struct LoginRequest {
        Header header;
        char username[32];
        char password_hash[64];
        uint32_t flags;
    };
}

// ============================================================================
// Shared Memory Example (from shared-memory.adoc)
// ============================================================================

namespace shm {
    struct SharedCounter {
        uint64_t value;
        uint32_t readers;
        uint32_t writers;
    };
    
    struct SharedBuffer {
        uint32_t size;
        uint32_t read_pos;
        uint32_t write_pos;
        uint32_t flags;
        char data[4096];
    };
}

// ============================================================================
// Serialization Example (from serialization.adoc)
// ============================================================================

namespace file_format {
    struct FileHeader {
        char magic[4];
        uint32_t version;
        uint32_t record_count;
        uint64_t layout_hash;
    };
    
    struct Record {
        uint64_t id;
        int32_t x;
        int32_t y;
        uint32_t flags;
        char name[32];
    };
}

// ============================================================================
// Bit-fields Example (from bitfields.adoc)
// ============================================================================

struct PackedFlags {
    uint32_t enabled : 1;
    uint32_t mode : 3;
    uint32_t priority : 4;
    uint32_t count : 8;
    uint32_t reserved : 16;
};

struct IPv4Header {
    uint8_t version : 4;
    uint8_t ihl : 4;
    uint8_t dscp : 6;
    uint8_t ecn : 2;
    uint16_t total_length;
    uint16_t identification;
    uint16_t flags : 3;
    uint16_t fragment_offset : 13;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t checksum;
    uint32_t src_addr;
    uint32_t dst_addr;
};

// ============================================================================
// Inheritance Example (from inheritance.adoc)
// ============================================================================

struct Entity {
    uint64_t id;
    uint32_t type;
};

struct Player : Entity {
    char name[32];
    uint32_t health;
    uint32_t mana;
};

struct NPC : Entity {
    char dialog[128];
    uint32_t behavior;
};

// ============================================================================
// Tests
// ============================================================================

int main() {
    std::cout << "=== Advanced Documentation Example Verification ===\n\n";
    
    // Network Protocol Tests
    std::cout << "--- Network Protocol ---\n";
    constexpr auto header_sig = get_layout_signature<protocol_v1::Header>();
    constexpr auto login_sig = get_layout_signature<protocol_v1::LoginRequest>();
    std::cout << "Header: " << header_sig.c_str() << "\n";
    std::cout << "LoginRequest: " << login_sig.c_str() << "\n";
    
    // Verify serializability
    static_assert(is_serializable_v<protocol_v1::Header, PlatformSet::bits64_le()>, "Header must be serializable");
    static_assert(is_serializable_v<protocol_v1::LoginRequest, PlatformSet::bits64_le()>, "LoginRequest must be serializable");
    std::cout << "  Both types are serializable: YES\n";
    
    // Shared Memory Tests
    std::cout << "\n--- Shared Memory ---\n";
    constexpr auto counter_sig = get_layout_signature<shm::SharedCounter>();
    constexpr auto buffer_sig = get_layout_signature<shm::SharedBuffer>();
    std::cout << "SharedCounter size: " << sizeof(shm::SharedCounter) << " bytes\n";
    std::cout << "SharedBuffer size: " << sizeof(shm::SharedBuffer) << " bytes\n";
    
    // Verification hashes for IPC
    constexpr auto counter_verif = get_layout_verification<shm::SharedCounter>();
    std::cout << "SharedCounter verification:\n";
    std::cout << "  FNV-1a: 0x" << std::hex << counter_verif.fnv1a << "\n";
    std::cout << "  DJB2:   0x" << counter_verif.djb2 << std::dec << "\n";
    
    // Serialization Tests
    std::cout << "\n--- Serialization ---\n";
    constexpr auto file_hash = get_layout_hash<file_format::FileHeader>();
    constexpr auto record_hash = get_layout_hash<file_format::Record>();
    std::cout << "FileHeader hash: 0x" << std::hex << file_hash << "\n";
    std::cout << "Record hash: 0x" << record_hash << std::dec << "\n";
    
    // Bit-fields Tests
    std::cout << "\n--- Bit-fields ---\n";
    constexpr auto flags_sig = get_layout_signature<PackedFlags>();
    constexpr auto ipv4_sig = get_layout_signature<IPv4Header>();
    std::cout << "PackedFlags signature:\n  " << flags_sig.c_str() << "\n";
    std::cout << "PackedFlags size: " << sizeof(PackedFlags) << " bytes\n";
    
    // Verify bit-field detection
    static_assert(has_bitfields<PackedFlags>(), "PackedFlags should have bitfields");
    static_assert(has_bitfields<IPv4Header>(), "IPv4Header should have bitfields");
    static_assert(!is_serializable_v<PackedFlags, PlatformSet::current()>, "Bitfield types are NOT serializable");
    std::cout << "  Bit-field detection: WORKING\n";
    std::cout << "  Serializability check: CORRECT (bitfields are not serializable)\n";
    
    // Inheritance Tests
    std::cout << "\n--- Inheritance ---\n";
    constexpr auto entity_sig = get_layout_signature<Entity>();
    constexpr auto player_sig = get_layout_signature<Player>();
    constexpr auto npc_sig = get_layout_signature<NPC>();
    std::cout << "Entity: " << entity_sig.c_str() << "\n";
    std::cout << "Player: " << player_sig.c_str() << "\n";
    std::cout << "NPC: " << npc_sig.c_str() << "\n";
    
    // Verify no hash collision in the protocol
    static_assert(no_hash_collision<
        protocol_v1::Header,
        protocol_v1::LoginRequest,
        shm::SharedCounter,
        shm::SharedBuffer,
        file_format::FileHeader,
        file_format::Record,
        Entity,
        Player,
        NPC
    >(), "No hash collisions in type library");
    std::cout << "\n  No hash collisions in type library: VERIFIED\n";
    
    // Concept usage
    std::cout << "\n--- Concept Verification ---\n";
    static_assert(Serializable<protocol_v1::Header>);
    static_assert(LayoutCompatible<protocol_v1::Header, protocol_v1::Header>);
    std::cout << "  Serializable<Header>: YES\n";
    std::cout << "  LayoutCompatible<Header, Header>: YES\n";
    
    std::cout << "\n=== All advanced tests passed! ===\n";
    return 0;
}
