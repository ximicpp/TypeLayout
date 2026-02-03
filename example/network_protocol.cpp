// Boost.TypeLayout - Network Protocol Verification Example
//
// This example demonstrates how to use TypeLayout for runtime verification
// of network protocol messages between sender and receiver.
//
// Key concepts:
// - Embed layout hash in packet header
// - Verify hash at runtime before processing
// - Handle version mismatches gracefully

#include <boost/typelayout/typelayout.hpp>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

using namespace boost::typelayout;

// =============================================================================
// Protocol Definition (shared between sender and receiver)
// =============================================================================

// Message payload class - demonstrates TypeLayout works with encapsulated classes
// NOT just POD structs! Private members are fully reflected.
class PlayerPosition {
public:
    PlayerPosition() = default;
    PlayerPosition(uint64_t id, float px, float py, float pz, uint32_t ts)
        : player_id_(id), x_(px), y_(py), z_(pz), timestamp_(ts) {}
    
    // Accessors
    uint64_t getPlayerId() const { return player_id_; }
    float getX() const { return x_; }
    float getY() const { return y_; }
    float getZ() const { return z_; }
    uint32_t getTimestamp() const { return timestamp_; }
    
    // Mutators
    void setPosition(float px, float py, float pz) { x_ = px; y_ = py; z_ = pz; }
    void setTimestamp(uint32_t ts) { timestamp_ = ts; }
    
private:
    uint64_t player_id_;   // TypeLayout reflects this!
    float x_;              // TypeLayout reflects this!
    float y_;              // TypeLayout reflects this!
    float z_;              // TypeLayout reflects this!
    uint32_t timestamp_;   // TypeLayout reflects this!
};

// Verify the class is supported
static_assert(LayoutSupported<PlayerPosition>, "PlayerPosition class is supported");

// Packet header class with encapsulation
class PacketHeader {
public:
    PacketHeader() = default;
    
    void setMagic(uint32_t m) { magic_ = m; }
    void setVersion(uint32_t v) { version_ = v; }
    void setPayloadHash(uint64_t h) { payload_hash_ = h; }
    void setPayloadSize(uint32_t s) { payload_size_ = s; }
    
    uint32_t getMagic() const { return magic_; }
    uint32_t getVersion() const { return version_; }
    uint64_t getPayloadHash() const { return payload_hash_; }
    uint32_t getPayloadSize() const { return payload_size_; }
    
private:
    uint32_t magic_;           // Protocol magic number
    uint32_t version_;         // Protocol version
    uint64_t payload_hash_;    // Layout hash of payload type
    uint32_t payload_size_;    // Size of payload in bytes
};

static_assert(LayoutSupported<PacketHeader>, "PacketHeader class is supported");

// Complete packet (composition of classes)
class PositionPacket {
public:
    PacketHeader& header() { return header_; }
    const PacketHeader& header() const { return header_; }
    PlayerPosition& payload() { return payload_; }
    const PlayerPosition& payload() const { return payload_; }
    
private:
    PacketHeader header_;
    PlayerPosition payload_;
};

static_assert(LayoutSupported<PositionPacket>, "PositionPacket class is supported");

constexpr uint32_t PROTOCOL_MAGIC = 0x54594C59;  // "TYLY"
constexpr uint32_t PROTOCOL_VERSION = 1;

// =============================================================================
// Sender Side
// =============================================================================

std::vector<uint8_t> create_position_packet(const PlayerPosition& pos) {
    PositionPacket packet;
    
    // Fill header with layout verification info (using class accessors)
    packet.header().setMagic(PROTOCOL_MAGIC);
    packet.header().setVersion(PROTOCOL_VERSION);
    packet.header().setPayloadHash(get_layout_hash<PlayerPosition>());  // Embed hash!
    packet.header().setPayloadSize(sizeof(PlayerPosition));
    
    // Fill payload
    packet.payload() = pos;
    
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
    
    // Check magic (using class accessor)
    if (header->getMagic() != PROTOCOL_MAGIC) {
        return VerifyResult::INVALID_MAGIC;
    }
    
    // Check version
    if (header->getVersion() != PROTOCOL_VERSION) {
        return VerifyResult::VERSION_MISMATCH;
    }
    
    // Check layout hash - THE KEY RUNTIME VERIFICATION!
    if (header->getPayloadHash() != get_layout_hash<PlayerPosition>()) {
        return VerifyResult::LAYOUT_MISMATCH;
    }
    
    // Check size
    if (header->getPayloadSize() != sizeof(PlayerPosition)) {
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
    out_pos = packet->payload();
    return true;
}

// =============================================================================
// Demo
// =============================================================================

int main() {
    std::cout << "=== TypeLayout Network Protocol Example ===\n";
    std::cout << "(Using encapsulated classes - NOT just POD structs!)\n\n";
    
    // Show layout info
    std::cout << "PlayerPosition class layout:\n";
    std::cout << "  Size: " << sizeof(PlayerPosition) << " bytes\n";
    std::cout << "  Hash: 0x" << std::hex << get_layout_hash<PlayerPosition>() << std::dec << "\n";
    std::cout << "  Signature: " << get_layout_signature<PlayerPosition>() << "\n\n";
    
    // Sender creates a packet
    std::cout << "--- Sender ---\n";
    PlayerPosition send_pos{12345, 100.5f, 200.0f, 50.25f, 1000};
    auto packet_data = create_position_packet(send_pos);
    std::cout << "Created packet: " << packet_data.size() << " bytes\n";
    std::cout << "Embedded hash: 0x" << std::hex 
              << reinterpret_cast<const PacketHeader*>(packet_data.data())->getPayloadHash() 
              << std::dec << "\n\n";
    
    // Receiver processes the packet
    std::cout << "--- Receiver ---\n";
    PlayerPosition recv_pos{};
    if (process_position_packet(packet_data.data(), packet_data.size(), recv_pos)) {
        std::cout << "Packet verified and processed successfully!\n";
        std::cout << "Received: player_id=" << recv_pos.getPlayerId() 
                  << ", pos=(" << recv_pos.getX() << ", " << recv_pos.getY() << ", " << recv_pos.getZ() << ")"
                  << ", timestamp=" << recv_pos.getTimestamp() << "\n";
    }
    
    // Simulate a tampered packet (wrong hash)
    std::cout << "\n--- Tampered Packet Test ---\n";
    auto bad_packet = packet_data;
    // Tamper with the payload_hash field (offset 8 in PacketHeader: magic(4) + version(4))
    uint64_t bad_hash = 0xDEADBEEF;
    std::memcpy(bad_packet.data() + 8, &bad_hash, sizeof(bad_hash));
    PlayerPosition bad_pos{};
    if (!process_position_packet(bad_packet.data(), bad_packet.size(), bad_pos)) {
        std::cout << "Tampered packet correctly rejected!\n";
    }
    
    return 0;
}
