// Boost.TypeLayout - Network Protocol Verification Example
//
// This example demonstrates how to use TypeLayout for runtime verification
// of network protocol messages between sender and receiver.
//
// Key concepts:
// - Embed layout hash in packet header
// - Verify hash at runtime before processing
// - Handle version mismatches gracefully

#include <boost/typelayout.hpp>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

using namespace boost::typelayout;

// =============================================================================
// Protocol Definition (shared between sender and receiver)
// =============================================================================

// Message payload structure
struct PlayerPosition {
    uint64_t player_id;
    float x;
    float y;
    float z;
    uint32_t timestamp;
};

// Packet header with embedded layout hash
struct PacketHeader {
    uint32_t magic;           // Protocol magic number
    uint32_t version;         // Protocol version
    uint64_t payload_hash;    // Layout hash of payload type
    uint32_t payload_size;    // Size of payload in bytes
};

// Complete packet
struct PositionPacket {
    PacketHeader header;
    PlayerPosition payload;
};

constexpr uint32_t PROTOCOL_MAGIC = 0x54594C59;  // "TYLY"
constexpr uint32_t PROTOCOL_VERSION = 1;

// =============================================================================
// Sender Side
// =============================================================================

std::vector<uint8_t> create_position_packet(const PlayerPosition& pos) {
    PositionPacket packet;
    
    // Fill header with layout verification info
    packet.header.magic = PROTOCOL_MAGIC;
    packet.header.version = PROTOCOL_VERSION;
    packet.header.payload_hash = get_layout_hash<PlayerPosition>();  // Embed hash!
    packet.header.payload_size = sizeof(PlayerPosition);
    
    // Fill payload
    packet.payload = pos;
    
    // Serialize to bytes
    std::vector<uint8_t> buffer(sizeof(packet));
    std::memcpy(buffer.data(), &packet, sizeof(packet));
    
    return buffer;
}

// =============================================================================
// Receiver Side
// =============================================================================

enum class VerifyResult {
    OK,
    INVALID_MAGIC,
    VERSION_MISMATCH,
    LAYOUT_MISMATCH,
    SIZE_MISMATCH
};

const char* verify_result_str(VerifyResult r) {
    switch (r) {
        case VerifyResult::OK: return "OK";
        case VerifyResult::INVALID_MAGIC: return "Invalid magic number";
        case VerifyResult::VERSION_MISMATCH: return "Protocol version mismatch";
        case VerifyResult::LAYOUT_MISMATCH: return "Layout hash mismatch";
        case VerifyResult::SIZE_MISMATCH: return "Payload size mismatch";
    }
    return "Unknown";
}

VerifyResult verify_packet(const void* data, size_t len) {
    if (len < sizeof(PacketHeader)) {
        return VerifyResult::SIZE_MISMATCH;
    }
    
    const auto* header = static_cast<const PacketHeader*>(data);
    
    // Check magic
    if (header->magic != PROTOCOL_MAGIC) {
        return VerifyResult::INVALID_MAGIC;
    }
    
    // Check version
    if (header->version != PROTOCOL_VERSION) {
        return VerifyResult::VERSION_MISMATCH;
    }
    
    // Check layout hash - THE KEY RUNTIME VERIFICATION!
    if (header->payload_hash != get_layout_hash<PlayerPosition>()) {
        return VerifyResult::LAYOUT_MISMATCH;
    }
    
    // Check size
    if (header->payload_size != sizeof(PlayerPosition)) {
        return VerifyResult::SIZE_MISMATCH;
    }
    
    return VerifyResult::OK;
}

bool process_position_packet(const void* data, size_t len, PlayerPosition& out_pos) {
    auto result = verify_packet(data, len);
    if (result != VerifyResult::OK) {
        std::cerr << "Packet verification failed: " << verify_result_str(result) << "\n";
        return false;
    }
    
    // Safe to interpret the payload now
    const auto* packet = static_cast<const PositionPacket*>(data);
    out_pos = packet->payload;
    return true;
}

// =============================================================================
// Demo
// =============================================================================

int main() {
    std::cout << "=== TypeLayout Network Protocol Example ===\n\n";
    
    // Show layout info
    std::cout << "PlayerPosition layout:\n";
    std::cout << "  Size: " << sizeof(PlayerPosition) << " bytes\n";
    std::cout << "  Hash: 0x" << std::hex << get_layout_hash<PlayerPosition>() << std::dec << "\n";
    std::cout << "  Signature: " << get_layout_signature<PlayerPosition>() << "\n\n";
    
    // Sender creates a packet
    std::cout << "--- Sender ---\n";
    PlayerPosition send_pos{12345, 100.5f, 200.0f, 50.25f, 1000};
    auto packet_data = create_position_packet(send_pos);
    std::cout << "Created packet: " << packet_data.size() << " bytes\n";
    std::cout << "Embedded hash: 0x" << std::hex 
              << reinterpret_cast<const PacketHeader*>(packet_data.data())->payload_hash 
              << std::dec << "\n\n";
    
    // Receiver processes the packet
    std::cout << "--- Receiver ---\n";
    PlayerPosition recv_pos{};
    if (process_position_packet(packet_data.data(), packet_data.size(), recv_pos)) {
        std::cout << "Packet verified and processed successfully!\n";
        std::cout << "Received: player_id=" << recv_pos.player_id 
                  << ", pos=(" << recv_pos.x << ", " << recv_pos.y << ", " << recv_pos.z << ")"
                  << ", timestamp=" << recv_pos.timestamp << "\n";
    }
    
    // Simulate a tampered packet (wrong hash)
    std::cout << "\n--- Tampered Packet Test ---\n";
    auto bad_packet = packet_data;
    reinterpret_cast<PacketHeader*>(bad_packet.data())->payload_hash = 0xDEADBEEF;
    PlayerPosition bad_pos{};
    if (!process_position_packet(bad_packet.data(), bad_packet.size(), bad_pos)) {
        std::cout << "Tampered packet correctly rejected!\n";
    }
    
    return 0;
}
