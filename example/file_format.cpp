// Boost.TypeLayout - File Format Verification Example
//
// This example demonstrates how to use TypeLayout for runtime verification
// of file data formats, ensuring compatibility between writer and reader.
//
// Key concepts:
// - Embed layout hash in file header
// - Verify hash when loading file
// - Handle format version upgrades

#include <boost/typelayout.hpp>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>

using namespace boost::typelayout;

// =============================================================================
// File Format Definition
// =============================================================================

// Game save data structure
struct SaveData {
    uint32_t version;
    char player_name[32];
    uint32_t level;
    uint32_t experience;
    float health;
    float mana;
    int32_t position_x;
    int32_t position_y;
    uint64_t play_time_seconds;
};

// File header with embedded layout verification
struct FileHeader {
    char magic[4];            // "SAVE"
    uint32_t header_version;  // Header format version
    uint64_t data_hash;       // Layout hash of SaveData
    uint32_t data_size;       // Size of SaveData in bytes
    uint32_t checksum;        // Simple checksum (for demo)
};

constexpr char FILE_MAGIC[4] = {'S', 'A', 'V', 'E'};
constexpr uint32_t HEADER_VERSION = 1;

// =============================================================================
// Writer (Save Game)
// =============================================================================

bool save_game(const char* filename, const SaveData& data) {
    std::ofstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to create file: " << filename << "\n";
        return false;
    }
    
    // Create header with layout verification
    FileHeader header;
    std::memcpy(header.magic, FILE_MAGIC, 4);
    header.header_version = HEADER_VERSION;
    header.data_hash = get_layout_hash<SaveData>();  // Embed hash!
    header.data_size = sizeof(SaveData);
    header.checksum = 0;  // Simplified for demo
    
    // Write header and data
    file.write(reinterpret_cast<const char*>(&header), sizeof(header));
    file.write(reinterpret_cast<const char*>(&data), sizeof(data));
    
    std::cout << "Saved game to: " << filename << "\n";
    std::cout << "  Data hash: 0x" << std::hex << header.data_hash << std::dec << "\n";
    std::cout << "  Data size: " << header.data_size << " bytes\n";
    
    return true;
}

// =============================================================================
// Reader (Load Game)
// =============================================================================

enum class LoadResult {
    OK,
    FILE_NOT_FOUND,
    INVALID_MAGIC,
    HEADER_VERSION_MISMATCH,
    LAYOUT_MISMATCH,
    SIZE_MISMATCH,
    READ_ERROR
};

const char* load_result_str(LoadResult r) {
    switch (r) {
        case LoadResult::OK: return "OK";
        case LoadResult::FILE_NOT_FOUND: return "File not found";
        case LoadResult::INVALID_MAGIC: return "Invalid file magic";
        case LoadResult::HEADER_VERSION_MISMATCH: return "Header version mismatch";
        case LoadResult::LAYOUT_MISMATCH: return "Save data layout mismatch";
        case LoadResult::SIZE_MISMATCH: return "Data size mismatch";
        case LoadResult::READ_ERROR: return "Read error";
    }
    return "Unknown";
}

LoadResult load_game(const char* filename, SaveData& out_data) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        return LoadResult::FILE_NOT_FOUND;
    }
    
    // Read header
    FileHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (!file) {
        return LoadResult::READ_ERROR;
    }
    
    // Verify magic
    if (std::memcmp(header.magic, FILE_MAGIC, 4) != 0) {
        return LoadResult::INVALID_MAGIC;
    }
    
    // Verify header version
    if (header.header_version != HEADER_VERSION) {
        return LoadResult::HEADER_VERSION_MISMATCH;
    }
    
    // Verify layout hash - THE KEY RUNTIME VERIFICATION!
    if (header.data_hash != get_layout_hash<SaveData>()) {
        std::cerr << "Layout mismatch!\n";
        std::cerr << "  File hash:   0x" << std::hex << header.data_hash << "\n";
        std::cerr << "  Expected:    0x" << get_layout_hash<SaveData>() << std::dec << "\n";
        return LoadResult::LAYOUT_MISMATCH;
    }
    
    // Verify size
    if (header.data_size != sizeof(SaveData)) {
        return LoadResult::SIZE_MISMATCH;
    }
    
    // Safe to read the data now
    file.read(reinterpret_cast<char*>(&out_data), sizeof(SaveData));
    if (!file) {
        return LoadResult::READ_ERROR;
    }
    
    return LoadResult::OK;
}

// =============================================================================
// Demo
// =============================================================================

int main() {
    std::cout << "=== TypeLayout File Format Example ===\n\n";
    
    // Show layout info
    std::cout << "SaveData layout:\n";
    std::cout << "  Size: " << sizeof(SaveData) << " bytes\n";
    std::cout << "  Hash: 0x" << std::hex << get_layout_hash<SaveData>() << std::dec << "\n";
    std::cout << "  Signature: " << get_layout_signature<SaveData>() << "\n\n";
    
    const char* save_file = "test_save.dat";
    
    // Create and save game data
    std::cout << "--- Save Game ---\n";
    SaveData save_data{};
    save_data.version = 1;
    std::strncpy(save_data.player_name, "Hero", sizeof(save_data.player_name) - 1);
    save_data.level = 42;
    save_data.experience = 123456;
    save_data.health = 100.0f;
    save_data.mana = 75.5f;
    save_data.position_x = 1000;
    save_data.position_y = -500;
    save_data.play_time_seconds = 36000;
    
    if (!save_game(save_file, save_data)) {
        return 1;
    }
    
    // Load game data
    std::cout << "\n--- Load Game ---\n";
    SaveData loaded_data{};
    auto result = load_game(save_file, loaded_data);
    
    if (result == LoadResult::OK) {
        std::cout << "Game loaded successfully!\n";
        std::cout << "  Player: " << loaded_data.player_name << "\n";
        std::cout << "  Level: " << loaded_data.level << "\n";
        std::cout << "  Experience: " << loaded_data.experience << "\n";
        std::cout << "  Health: " << loaded_data.health << "\n";
        std::cout << "  Mana: " << loaded_data.mana << "\n";
        std::cout << "  Position: (" << loaded_data.position_x << ", " << loaded_data.position_y << ")\n";
        std::cout << "  Play time: " << loaded_data.play_time_seconds << " seconds\n";
    } else {
        std::cerr << "Failed to load game: " << load_result_str(result) << "\n";
        return 1;
    }
    
    // Clean up
    std::remove(save_file);
    
    return 0;
}
