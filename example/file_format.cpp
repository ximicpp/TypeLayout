// Boost.TypeLayout - File Format Verification Example
//
// This example demonstrates how to use TypeLayout for runtime verification
// of file data formats, ensuring compatibility between writer and reader.
//
// Key concepts:
// - Embed layout hash in file header
// - Verify hash when loading file
// - Handle format version upgrades

#include <boost/typelayout/typelayout.hpp>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>

using namespace boost::typelayout;

// =============================================================================
// File Format Definition
// =============================================================================

// Game save data class - demonstrates TypeLayout works with encapsulated classes
// NOT just POD structs! Private members are fully reflected.
class SaveData {
public:
    SaveData() = default;
    
    // Setters
    void setVersion(uint32_t v) { version_ = v; }
    void setPlayerName(const char* name) { 
        std::strncpy(player_name_, name, sizeof(player_name_) - 1); 
        player_name_[sizeof(player_name_) - 1] = '\0';
    }
    void setLevel(uint32_t l) { level_ = l; }
    void setExperience(uint32_t e) { experience_ = e; }
    void setHealth(float h) { health_ = h; }
    void setMana(float m) { mana_ = m; }
    void setPosition(int32_t x, int32_t y) { position_x_ = x; position_y_ = y; }
    void setPlayTime(uint64_t t) { play_time_seconds_ = t; }
    
    // Getters
    uint32_t getVersion() const { return version_; }
    const char* getPlayerName() const { return player_name_; }
    uint32_t getLevel() const { return level_; }
    uint32_t getExperience() const { return experience_; }
    float getHealth() const { return health_; }
    float getMana() const { return mana_; }
    int32_t getPositionX() const { return position_x_; }
    int32_t getPositionY() const { return position_y_; }
    uint64_t getPlayTime() const { return play_time_seconds_; }
    
private:
    uint32_t version_;           // TypeLayout reflects this!
    char player_name_[32];       // TypeLayout reflects this!
    uint32_t level_;             // TypeLayout reflects this!
    uint32_t experience_;        // TypeLayout reflects this!
    float health_;               // TypeLayout reflects this!
    float mana_;                 // TypeLayout reflects this!
    int32_t position_x_;         // TypeLayout reflects this!
    int32_t position_y_;         // TypeLayout reflects this!
    uint64_t play_time_seconds_; // TypeLayout reflects this!
};

// Verify class is supported
static_assert(LayoutSupported<SaveData>, "SaveData class is supported");

// File header class with encapsulation
class FileHeader {
public:
    FileHeader() = default;
    
    void setMagic(const char* m) { std::memcpy(magic_, m, 4); }
    void setHeaderVersion(uint32_t v) { header_version_ = v; }
    void setDataHash(uint64_t h) { data_hash_ = h; }
    void setDataSize(uint32_t s) { data_size_ = s; }
    void setChecksum(uint32_t c) { checksum_ = c; }
    
    const char* getMagic() const { return magic_; }
    uint32_t getHeaderVersion() const { return header_version_; }
    uint64_t getDataHash() const { return data_hash_; }
    uint32_t getDataSize() const { return data_size_; }
    uint32_t getChecksum() const { return checksum_; }
    
private:
    char magic_[4];            // "SAVE"
    uint32_t header_version_;  // Header format version
    uint64_t data_hash_;       // Layout hash of SaveData
    uint32_t data_size_;       // Size of SaveData in bytes
    uint32_t checksum_;        // Simple checksum (for demo)
};

static_assert(LayoutSupported<FileHeader>, "FileHeader class is supported");

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
    
    // Create header with layout verification (using class accessors)
    FileHeader header;
    header.setMagic(FILE_MAGIC);
    header.setHeaderVersion(HEADER_VERSION);
    header.setDataHash(get_layout_hash<SaveData>());  // Embed hash!
    header.setDataSize(sizeof(SaveData));
    header.setChecksum(0);  // Simplified for demo
    
    // Write header and data
    file.write(reinterpret_cast<const char*>(&header), sizeof(header));
    file.write(reinterpret_cast<const char*>(&data), sizeof(data));
    
    std::cout << "Saved game to: " << filename << "\n";
    std::cout << "  Data hash: 0x" << std::hex << header.getDataHash() << std::dec << "\n";
    std::cout << "  Data size: " << header.getDataSize() << " bytes\n";
    
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
    
    // Verify magic (using class accessor)
    if (std::memcmp(header.getMagic(), FILE_MAGIC, 4) != 0) {
        return LoadResult::INVALID_MAGIC;
    }
    
    // Verify header version
    if (header.getHeaderVersion() != HEADER_VERSION) {
        return LoadResult::HEADER_VERSION_MISMATCH;
    }
    
    // Verify layout hash - THE KEY RUNTIME VERIFICATION!
    if (header.getDataHash() != get_layout_hash<SaveData>()) {
        std::cerr << "Layout mismatch!\n";
        std::cerr << "  File hash:   0x" << std::hex << header.getDataHash() << "\n";
        std::cerr << "  Expected:    0x" << get_layout_hash<SaveData>() << std::dec << "\n";
        return LoadResult::LAYOUT_MISMATCH;
    }
    
    // Verify size
    if (header.getDataSize() != sizeof(SaveData)) {
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
    std::cout << "=== TypeLayout File Format Example ===\n";
    std::cout << "(Using encapsulated classes - NOT just POD structs!)\n\n";
    
    // Show layout info
    std::cout << "SaveData class layout:\n";
    std::cout << "  Size: " << sizeof(SaveData) << " bytes\n";
    std::cout << "  Hash: 0x" << std::hex << get_layout_hash<SaveData>() << std::dec << "\n";
    std::cout << "  Signature: " << get_layout_signature<SaveData>() << "\n\n";
    
    const char* save_file = "test_save.dat";
    
    // Create and save game data (using class setters)
    std::cout << "--- Save Game ---\n";
    SaveData save_data{};
    save_data.setVersion(1);
    save_data.setPlayerName("Hero");
    save_data.setLevel(42);
    save_data.setExperience(123456);
    save_data.setHealth(100.0f);
    save_data.setMana(75.5f);
    save_data.setPosition(1000, -500);
    save_data.setPlayTime(36000);
    
    if (!save_game(save_file, save_data)) {
        return 1;
    }
    
    // Load game data
    std::cout << "\n--- Load Game ---\n";
    SaveData loaded_data{};
    auto result = load_game(save_file, loaded_data);
    
    if (result == LoadResult::OK) {
        std::cout << "Game loaded successfully!\n";
        std::cout << "  Player: " << loaded_data.getPlayerName() << "\n";
        std::cout << "  Level: " << loaded_data.getLevel() << "\n";
        std::cout << "  Experience: " << loaded_data.getExperience() << "\n";
        std::cout << "  Health: " << loaded_data.getHealth() << "\n";
        std::cout << "  Mana: " << loaded_data.getMana() << "\n";
        std::cout << "  Position: (" << loaded_data.getPositionX() << ", " << loaded_data.getPositionY() << ")\n";
        std::cout << "  Play time: " << loaded_data.getPlayTime() << " seconds\n";
    } else {
        std::cerr << "Failed to load game: " << load_result_str(result) << "\n";
        return 1;
    }
    
    // Clean up
    std::remove(save_file);
    
    return 0;
}
