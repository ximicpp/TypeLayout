// Boost.TypeLayout
//
// Killer Application Demo: Cross-Process Shared Memory Verification
//
// This example demonstrates TypeLayout's #1-A killer application:
// Zero-overhead layout validation for shared memory IPC.
//
// Problem Solved:
//   When two processes share memory, they must agree on the struct layout.
//   If one process is compiled with a different struct definition:
//   - Traditional: Silent data corruption
//   - With TypeLayout: Immediate detection via hash mismatch
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#include <boost/typelayout.hpp>
#include <iostream>
#include <cstring>
#include <cstdint>

using namespace boost::typelayout;

// =============================================================================
// Shared Data Structure
// =============================================================================

/// Game state that will be shared between processes
/// Note: All types are portable (no pointers, no 'long')
struct GameState {
    float player_x;
    float player_y;
    float player_z;
    int32_t health;
    int32_t score;
    uint64_t timestamp;
};

// Compile-time verification that GameState is safe for shared memory
static_assert(SharedMemorySafe<GameState>, 
    "GameState must be safe for shared memory");
static_assert(ZeroCopyTransmittable<GameState>,
    "GameState must be zero-copy transmittable");

// =============================================================================
// Shared Memory Region Template
// =============================================================================

/// Header for shared memory region with layout verification
template<typename T>
struct SharedMemoryHeader {
    uint64_t layout_hash;      // Layout hash for verification
    uint32_t magic;            // Magic number to detect corruption
    uint32_t version;          // Application version (optional)
    
    // Data follows the header
    T data;
    
    /// Initialize header with compile-time computed hash
    void initialize() {
        layout_hash = get_layout_hash<T>();
        magic = 0xDEADBEEF;
        version = 1;
        std::memset(&data, 0, sizeof(T));
    }
    
    /// Verify that the layout matches expected
    [[nodiscard]] bool verify() const {
        if (magic != 0xDEADBEEF) {
            return false;  // Memory corruption
        }
        if (layout_hash != get_layout_hash<T>()) {
            return false;  // Layout mismatch!
        }
        return true;
    }
};

// =============================================================================
// Simulated Shared Memory (for demo purposes)
// =============================================================================

// In real code, this would be:
//   void* shm = shm_open("/game_state", ...) + mmap(...)
// For this demo, we simulate with a static buffer
alignas(64) static char g_simulated_shm[4096];

/// Simulate creating shared memory (producer process)
template<typename T>
    requires SharedMemorySafe<T>
SharedMemoryHeader<T>* create_shared_memory(const char* name) {
    std::cout << "[Producer] Creating shared memory: " << name << "\n";
    std::cout << "[Producer] Data type layout hash: 0x" 
              << std::hex << get_layout_hash<T>() << std::dec << "\n";
    
    auto* region = reinterpret_cast<SharedMemoryHeader<T>*>(g_simulated_shm);
    region->initialize();
    
    std::cout << "[Producer] Shared memory initialized successfully\n";
    return region;
}

/// Simulate attaching to shared memory (consumer process)
template<typename T>
    requires SharedMemorySafe<T>
SharedMemoryHeader<T>* attach_shared_memory(const char* name) {
    std::cout << "[Consumer] Attaching to shared memory: " << name << "\n";
    std::cout << "[Consumer] Expected layout hash: 0x" 
              << std::hex << get_layout_hash<T>() << std::dec << "\n";
    
    auto* region = reinterpret_cast<SharedMemoryHeader<T>*>(g_simulated_shm);
    
    if (!region->verify()) {
        std::cout << "[Consumer] ERROR: Layout verification failed!\n";
        std::cout << "[Consumer] Found hash: 0x" << std::hex 
                  << region->layout_hash << std::dec << "\n";
        std::cout << "[Consumer] This could mean:\n";
        std::cout << "  - Producer was compiled with different struct definition\n";
        std::cout << "  - Producer uses different compiler/platform\n";
        std::cout << "  - Memory corruption occurred\n";
        return nullptr;
    }
    
    std::cout << "[Consumer] Layout verification passed!\n";
    return region;
}

// =============================================================================
// Demo: Successful Verification
// =============================================================================

void demo_success() {
    std::cout << "\n=== Demo: Successful Shared Memory Verification ===\n\n";
    
    // Producer creates shared memory
    auto* producer_view = create_shared_memory<GameState>("/game_state");
    
    // Producer writes data
    producer_view->data.player_x = 100.0f;
    producer_view->data.player_y = 50.0f;
    producer_view->data.player_z = 0.0f;
    producer_view->data.health = 100;
    producer_view->data.score = 9999;
    producer_view->data.timestamp = 1234567890;
    
    std::cout << "[Producer] Wrote game state: pos=(" 
              << producer_view->data.player_x << "," 
              << producer_view->data.player_y << ","
              << producer_view->data.player_z << ") health=" 
              << producer_view->data.health << "\n\n";
    
    // Consumer attaches and reads
    auto* consumer_view = attach_shared_memory<GameState>("/game_state");
    
    if (consumer_view) {
        std::cout << "[Consumer] Read game state: pos=(" 
                  << consumer_view->data.player_x << "," 
                  << consumer_view->data.player_y << ","
                  << consumer_view->data.player_z << ") health=" 
                  << consumer_view->data.health << "\n";
        std::cout << "[Consumer] Success! Data transferred correctly.\n";
    }
}

// =============================================================================
// Demo: Layout Mismatch Detection
// =============================================================================

// Simulate a different version of GameState (e.g., from older code)
struct GameStateV2 {
    float player_x;
    float player_y;
    // NOTE: player_z is missing in this version!
    int32_t health;
    int32_t score;
    uint64_t timestamp;
    int32_t level;  // New field added
};

static_assert(SharedMemorySafe<GameStateV2>,
    "GameStateV2 must be safe for shared memory");

void demo_mismatch() {
    std::cout << "\n=== Demo: Layout Mismatch Detection ===\n\n";
    
    // Producer creates with GameState
    auto* producer_view = create_shared_memory<GameState>("/game_state");
    producer_view->data.player_x = 100.0f;
    producer_view->data.health = 100;
    
    std::cout << "\n";
    
    // Consumer tries to attach with DIFFERENT struct (GameStateV2)
    // This simulates a version mismatch between processes
    std::cout << "[Consumer] Attempting to attach with different struct version...\n";
    auto* consumer_view = attach_shared_memory<GameStateV2>("/game_state");
    
    if (!consumer_view) {
        std::cout << "\n[Demo] TypeLayout correctly detected the mismatch!\n";
        std::cout << "[Demo] Without TypeLayout, this would cause silent data corruption.\n";
    }
}

// =============================================================================
// Main
// =============================================================================

int main() {
    std::cout << "╔══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  Boost.TypeLayout - Killer App #1-A: Shared Memory Demo      ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════╝\n";
    
    // Show layout info
    std::cout << "\n--- Layout Information ---\n";
    std::cout << "GameState signature:\n  " << get_layout_signature<GameState>() << "\n";
    std::cout << "GameStateV2 signature:\n  " << get_layout_signature<GameStateV2>() << "\n";
    std::cout << "\nGameState hash:   0x" << std::hex << get_layout_hash<GameState>() << "\n";
    std::cout << "GameStateV2 hash: 0x" << get_layout_hash<GameStateV2>() << std::dec << "\n";
    
    // Run demos
    demo_success();
    demo_mismatch();
    
    std::cout << "\n╔══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  Key Takeaway: TypeLayout prevents silent data corruption    ║\n";
    std::cout << "║  in shared memory by automatic layout verification.          ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════╝\n";
    
    return 0;
}
