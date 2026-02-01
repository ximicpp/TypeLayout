// Boost.TypeLayout
//
// Killer Application Demo: Zero-Copy Network Protocol
//
// This example demonstrates TypeLayout's #1-B killer application:
// IDL-free, zero-encode/decode network transmission.
//
// Key Advantages over Protobuf/FlatBuffers/Cap'n Proto:
//   - No IDL files (.proto, .fbs, .capnp)
//   - No code generation step
//   - Zero encode/decode CPU overhead
//   - Automatic layout change detection
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#include <boost/typelayout/typelayout_all.hpp>
#include <iostream>
#include <cstring>
#include <cstdint>
#include <vector>
#include <expected>
#include <span>

using namespace boost::typelayout;

// =============================================================================
// Protocol Error Types
// =============================================================================

enum class ProtocolError {
    OK,
    LAYOUT_MISMATCH,
    SIZE_MISMATCH,
    MAGIC_MISMATCH,
    INCOMPLETE_DATA
};

const char* to_string(ProtocolError err) {
    switch (err) {
        case ProtocolError::OK: return "OK";
        case ProtocolError::LAYOUT_MISMATCH: return "LAYOUT_MISMATCH";
        case ProtocolError::SIZE_MISMATCH: return "SIZE_MISMATCH";
        case ProtocolError::MAGIC_MISMATCH: return "MAGIC_MISMATCH";
        case ProtocolError::INCOMPLETE_DATA: return "INCOMPLETE_DATA";
        default: return "UNKNOWN";
    }
}

// =============================================================================
// Network Packet Header
// =============================================================================

/// Zero-copy packet header with layout verification
struct PacketHeader {
    uint32_t magic;          // Protocol magic number
    uint32_t payload_size;   // Size of payload in bytes
    uint64_t layout_hash;    // TypeLayout hash for verification
    
    static constexpr uint32_t MAGIC = 0x544C5043;  // "TLPC" (TypeLayout Protocol)
};

static_assert(sizeof(PacketHeader) == 16, "PacketHeader should be 16 bytes");

// =============================================================================
// Example Message Types
// =============================================================================

/// Player position update (sent 60+ times per second in games)
struct PlayerPosition {
    uint32_t player_id;
    float x, y, z;
    float velocity_x, velocity_y, velocity_z;
    uint64_t timestamp;
};

/// Game event notification
struct GameEvent {
    uint32_t event_type;
    uint32_t source_id;
    uint32_t target_id;
    int32_t value;
    uint64_t timestamp;
};

/// Player stats (larger payload)
struct PlayerStats {
    uint32_t player_id;
    int32_t health;
    int32_t max_health;
    int32_t mana;
    int32_t max_mana;
    int32_t strength;
    int32_t agility;
    int32_t intelligence;
    uint32_t level;
    uint64_t experience;
    float position[3];
    char name[32];
};

// Compile-time verification - types must be serializable for zero-copy
static_assert(Serializable<PlayerPosition>,
    "PlayerPosition must be serializable for zero-copy");
static_assert(Serializable<GameEvent>,
    "GameEvent must be serializable for zero-copy");
static_assert(Serializable<PlayerStats>,
    "PlayerStats must be serializable for zero-copy");

// =============================================================================
// Simulated Network Buffer
// =============================================================================

class NetworkBuffer {
public:
    void clear() { data_.clear(); write_pos_ = 0; read_pos_ = 0; }
    
    // Write raw bytes
    void write(const void* ptr, size_t size) {
        const auto* bytes = static_cast<const char*>(ptr);
        data_.insert(data_.end(), bytes, bytes + size);
        write_pos_ += size;
    }
    
    // Read raw bytes
    bool read(void* ptr, size_t size) {
        if (read_pos_ + size > data_.size()) return false;
        std::memcpy(ptr, data_.data() + read_pos_, size);
        read_pos_ += size;
        return true;
    }
    
    // Get pointer to current read position
    const char* peek(size_t size) const {
        if (read_pos_ + size > data_.size()) return nullptr;
        return data_.data() + read_pos_;
    }
    
    void advance(size_t size) { read_pos_ += size; }
    
    size_t bytes_available() const { return data_.size() - read_pos_; }
    size_t total_size() const { return data_.size(); }
    
private:
    std::vector<char> data_;
    size_t write_pos_ = 0;
    size_t read_pos_ = 0;
};

// =============================================================================
// Zero-Copy Send/Receive Functions
// =============================================================================

/// Send a message with zero encoding overhead
/// The layout hash is computed at compile-time!
template<Serializable T>
void send_zero_copy(NetworkBuffer& net, const T& payload) {
    // Create header with compile-time computed hash
    PacketHeader header{
        .magic = PacketHeader::MAGIC,
        .payload_size = sizeof(T),
        .layout_hash = get_layout_hash<T>()  // constexpr!
    };
    
    // Write header + payload (just memcpy, no encoding!)
    net.write(&header, sizeof(header));
    net.write(&payload, sizeof(T));
}

/// Receive and verify a message with zero decoding overhead
template<Serializable T>
std::expected<const T*, ProtocolError> recv_zero_copy(NetworkBuffer& net) {
    // Check we have enough data for header
    if (net.bytes_available() < sizeof(PacketHeader)) {
        return std::unexpected(ProtocolError::INCOMPLETE_DATA);
    }
    
    // Read header
    PacketHeader header;
    if (!net.read(&header, sizeof(header))) {
        return std::unexpected(ProtocolError::INCOMPLETE_DATA);
    }
    
    // Verify magic
    if (header.magic != PacketHeader::MAGIC) {
        return std::unexpected(ProtocolError::MAGIC_MISMATCH);
    }
    
    // Verify size
    if (header.payload_size != sizeof(T)) {
        return std::unexpected(ProtocolError::SIZE_MISMATCH);
    }
    
    // Verify layout hash - this is the key safety check!
    if (header.layout_hash != get_layout_hash<T>()) {
        return std::unexpected(ProtocolError::LAYOUT_MISMATCH);
    }
    
    // Check we have enough data for payload
    if (net.bytes_available() < sizeof(T)) {
        return std::unexpected(ProtocolError::INCOMPLETE_DATA);
    }
    
    // Zero-copy: just return pointer to the buffer (no decoding!)
    const T* payload = reinterpret_cast<const T*>(net.peek(sizeof(T)));
    net.advance(sizeof(T));
    return payload;
}

// =============================================================================
// Performance Comparison
// =============================================================================

void show_performance_comparison() {
    std::cout << "\n=== Performance Comparison (Conceptual) ===\n\n";
    std::cout << "For PlayerPosition (48 bytes):\n";
    std::cout << "┌─────────────────┬────────────┬────────────┬─────────────┐\n";
    std::cout << "│ Method          │ Encode     │ Decode     │ Total       │\n";
    std::cout << "├─────────────────┼────────────┼────────────┼─────────────┤\n";
    std::cout << "│ JSON            │ ~5-10 μs   │ ~5-10 μs   │ ~10-20 μs   │\n";
    std::cout << "│ Protobuf        │ ~200-500ns │ ~200-500ns │ ~400-1000ns │\n";
    std::cout << "│ FlatBuffers     │ ~50-100ns  │ ~20-50ns   │ ~70-150ns   │\n";
    std::cout << "│ Cap'n Proto     │ ~0ns       │ ~0ns       │ ~0ns        │\n";
    std::cout << "│ TypeLayout      │ ~0ns       │ ~0ns       │ ~0ns        │\n";
    std::cout << "└─────────────────┴────────────┴────────────┴─────────────┘\n";
    std::cout << "\nTypeLayout advantage: Zero overhead + No IDL + Auto verification\n";
}

// =============================================================================
// Demo: Successful Transmission
// =============================================================================

void demo_success() {
    std::cout << "\n=== Demo: Successful Zero-Copy Transmission ===\n\n";
    
    NetworkBuffer network;
    
    // Sender creates and sends message
    std::cout << "[Sender] Creating PlayerPosition message...\n";
    PlayerPosition pos{
        .player_id = 42,
        .x = 100.5f, .y = 50.25f, .z = 10.0f,
        .velocity_x = 5.0f, .velocity_y = 0.0f, .velocity_z = -2.5f,
        .timestamp = 1234567890
    };
    
    send_zero_copy(network, pos);
    std::cout << "[Sender] Sent " << network.total_size() << " bytes\n";
    std::cout << "[Sender] Header: 16 bytes, Payload: " << sizeof(PlayerPosition) << " bytes\n";
    std::cout << "[Sender] Layout hash: 0x" << std::hex << get_layout_hash<PlayerPosition>() 
              << std::dec << "\n\n";
    
    // Receiver receives and verifies
    std::cout << "[Receiver] Receiving message...\n";
    auto result = recv_zero_copy<PlayerPosition>(network);
    
    if (result) {
        const PlayerPosition* received = *result;
        std::cout << "[Receiver] Layout verification: PASSED\n";
        std::cout << "[Receiver] Data: player_id=" << received->player_id
                  << " pos=(" << received->x << "," << received->y << "," << received->z << ")\n";
        std::cout << "[Receiver] Zero decoding - just pointer cast!\n";
    } else {
        std::cout << "[Receiver] ERROR: " << to_string(result.error()) << "\n";
    }
}

// =============================================================================
// Demo: Version Mismatch Detection
// =============================================================================

// Simulate an older version of PlayerPosition
struct PlayerPositionV1 {
    uint32_t player_id;
    float x, y;  // Missing z!
    uint64_t timestamp;
};

static_assert(Serializable<PlayerPositionV1>,
    "PlayerPositionV1 must be serializable for zero-copy");

void demo_version_mismatch() {
    std::cout << "\n=== Demo: Version Mismatch Detection ===\n\n";
    
    NetworkBuffer network;
    
    // Sender uses new version
    std::cout << "[Sender v2] Sending with PlayerPosition (new version)...\n";
    PlayerPosition pos{
        .player_id = 42,
        .x = 100.0f, .y = 50.0f, .z = 10.0f,
        .velocity_x = 0, .velocity_y = 0, .velocity_z = 0,
        .timestamp = 1234567890
    };
    send_zero_copy(network, pos);
    
    std::cout << "[Sender v2] Hash: 0x" << std::hex << get_layout_hash<PlayerPosition>() 
              << std::dec << "\n\n";
    
    // Receiver uses old version (simulating version mismatch)
    std::cout << "[Receiver v1] Receiving with PlayerPositionV1 (old version)...\n";
    std::cout << "[Receiver v1] Expected hash: 0x" << std::hex 
              << get_layout_hash<PlayerPositionV1>() << std::dec << "\n";
    
    auto result = recv_zero_copy<PlayerPositionV1>(network);
    
    if (!result) {
        std::cout << "[Receiver v1] ERROR: " << to_string(result.error()) << "\n";
        std::cout << "\n[Demo] TypeLayout correctly detected version mismatch!\n";
        std::cout << "[Demo] Without this check, data would be misinterpreted.\n";
    }
}

// =============================================================================
// Demo: Multiple Message Types
// =============================================================================

void demo_multiple_types() {
    std::cout << "\n=== Demo: Multiple Message Types in Stream ===\n\n";
    
    NetworkBuffer network;
    
    // Send different message types
    PlayerPosition pos{.player_id = 1, .x = 10, .y = 20, .z = 30};
    GameEvent event{.event_type = 1, .source_id = 1, .target_id = 2, .value = 100};
    PlayerStats stats{.player_id = 1, .health = 100, .level = 42};
    std::strcpy(stats.name, "Hero");
    
    send_zero_copy(network, pos);
    send_zero_copy(network, event);
    send_zero_copy(network, stats);
    
    std::cout << "[Sender] Sent 3 messages, total: " << network.total_size() << " bytes\n\n";
    
    // Receive in correct order
    auto r1 = recv_zero_copy<PlayerPosition>(network);
    auto r2 = recv_zero_copy<GameEvent>(network);
    auto r3 = recv_zero_copy<PlayerStats>(network);
    
    std::cout << "[Receiver] Message 1 (PlayerPosition): " 
              << (r1 ? "OK" : to_string(r1.error())) << "\n";
    std::cout << "[Receiver] Message 2 (GameEvent): " 
              << (r2 ? "OK" : to_string(r2.error())) << "\n";
    std::cout << "[Receiver] Message 3 (PlayerStats): " 
              << (r3 ? "OK" : to_string(r3.error())) << "\n";
    
    if (r3) {
        std::cout << "[Receiver] Player name: " << (*r3)->name 
                  << ", level: " << (*r3)->level << "\n";
    }
}

// =============================================================================
// Main
// =============================================================================

int main() {
    std::cout << "╔══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  Boost.TypeLayout - Killer App #1-B: Zero-Copy Protocol      ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════╝\n";
    
    // Show layout info
    std::cout << "\n--- Message Type Layouts ---\n";
    std::cout << "PlayerPosition: " << sizeof(PlayerPosition) << " bytes\n";
    std::cout << "  Hash: 0x" << std::hex << get_layout_hash<PlayerPosition>() << std::dec << "\n";
    std::cout << "GameEvent: " << sizeof(GameEvent) << " bytes\n";
    std::cout << "  Hash: 0x" << std::hex << get_layout_hash<GameEvent>() << std::dec << "\n";
    std::cout << "PlayerStats: " << sizeof(PlayerStats) << " bytes\n";
    std::cout << "  Hash: 0x" << std::hex << get_layout_hash<PlayerStats>() << std::dec << "\n";
    
    // Performance comparison
    show_performance_comparison();
    
    // Run demos
    demo_success();
    demo_version_mismatch();
    demo_multiple_types();
    
    std::cout << "\n╔══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  Key Advantages:                                             ║\n";
    std::cout << "║  • Zero encode/decode overhead (just memcpy)                 ║\n";
    std::cout << "║  • No IDL files, no code generation                          ║\n";
    std::cout << "║  • Automatic layout change detection                         ║\n";
    std::cout << "║  • Native C++ structs - no learning curve                    ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════╝\n";
    
    return 0;
}
