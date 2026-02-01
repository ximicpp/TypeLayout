// TypeLayout Compatibility Check - Example Configuration
//
// This is an example configuration file showing how to use
// the TypeLayout cross-platform compatibility check tool.
//
// Usage:
//   1. Copy this file to your project root as 'typelayout.config.hpp'
//   2. Replace the example types with your own types
//   3. Add a workflow file that calls the TypeLayout compat-check workflow
//
// Copyright (c) 2024-2026 TypeLayout Development Team

#pragma once

#include <cstdint>
#include <boost/typelayout/compat.hpp>

// =========================================================================
// Example: Portable Types (GOOD - will pass compatibility check)
// =========================================================================

/// A network packet with fixed-width integers
struct NetworkPacket {
    uint64_t sequence_number;
    uint32_t payload_size;
    uint8_t  flags;
    uint8_t  reserved[3];  // Explicit padding
    int32_t  data[16];
};

/// A game state structure
struct PlayerState {
    int32_t player_id;
    float   position[3];
    float   velocity[3];
    uint8_t health;
    uint8_t armor;
    uint8_t padding[2];  // Explicit padding for alignment
    int32_t score;
};

/// A sensor reading
struct SensorReading {
    uint64_t timestamp_ns;
    double   value;
    int32_t  sensor_id;
    uint16_t status;
    uint16_t reserved;
};

// =========================================================================
// Example: Non-Portable Types (BAD - will fail compatibility check)
// =========================================================================

/// WARNING: This type uses 'long' which has different sizes on LP64 vs LLP64
/// It will be detected as INCOMPATIBLE between Linux and Windows
struct BadLongType {
    long    value;      // 8 bytes on Linux, 4 bytes on Windows!
    int32_t id;
};

/// WARNING: This type uses 'wchar_t' which has different sizes
/// It will be detected as INCOMPATIBLE between platforms
struct BadWcharType {
    wchar_t name[32];   // 4 bytes per char on Linux, 2 bytes on Windows!
};

// =========================================================================
// Register Types for Compatibility Checking
// =========================================================================

// Register all types you want to check
// Comment out BadLongType and BadWcharType if you want the check to pass
TYPELAYOUT_TYPES(
    // Portable types - should pass
    NetworkPacket,
    PlayerState,
    SensorReading
    
    // Uncomment to see compatibility failures:
    // , BadLongType
    // , BadWcharType
)

// =========================================================================
// Specify Target Platforms (Optional)
// =========================================================================

// Default is linux-x64 and windows-x64
// Uncomment to customize:
// TYPELAYOUT_PLATFORMS(linux_x64, windows_x64, macos_arm64)
