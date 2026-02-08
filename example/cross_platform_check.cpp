// Cross-Platform Signature Export Tool (Phase 1)
//
// Compile and run this on each target platform to produce a .sig.hpp header.
// The generated header can then be used in Phase 2 for compile-time
// compatibility checking across platforms.
//
// Usage:
//   ./sig_export                           # auto-detect platform, write to stdout
//   ./sig_export sigs/                     # auto-detect platform, write to sigs/<platform>.sig.hpp
//   ./sig_export sigs/ my_custom_platform  # manual platform name
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#include <boost/typelayout/tools/sig_export.hpp>
#include <cstdint>
#include <string>
#include <filesystem>

using namespace boost::typelayout;

// =========================================================================
// Representative types for cross-platform testing
// =========================================================================

// --- Safe types (fixed-width only, expected portable) ---

struct PacketHeader {
    uint32_t magic;       // Protocol magic number
    uint16_t version;     // Protocol version
    uint16_t type;        // Message type
    uint32_t payload_len; // Payload length
    uint32_t checksum;    // CRC32
};

struct SharedMemRegion {
    uint64_t offset;      // Byte offset into shared segment
    uint64_t size;        // Region size in bytes
    uint32_t flags;       // Access flags
    uint32_t owner_pid;   // Owner process ID
};

struct FileHeader {
    char     magic[4];    // "TLAY"
    uint32_t version;
    uint64_t timestamp;
    uint32_t entry_count;
    uint32_t reserved;
};

struct SensorRecord {
    uint64_t timestamp_ns;  // Nanoseconds since epoch
    float    temperature;   // Celsius
    float    humidity;      // Percentage
    float    pressure;      // hPa
    uint32_t sensor_id;
};

struct IpcCommand {
    uint32_t  cmd_id;
    uint32_t  flags;
    int64_t   arg1;
    int64_t   arg2;
    char      payload[64];
};

// --- Unsafe types (platform-dependent members) ---

struct UnsafeStruct {
    long        a;
    void*       ptr;
    wchar_t     wc;
    long double ld;
};

struct UnsafeWithPointer {
    uint32_t  id;
    char*     name;
    uint64_t  timestamp;
};

struct MixedSafety {
    uint32_t  id;
    double    value;
    int       count;
};

// =========================================================================
// Main
// =========================================================================

int main(int argc, char* argv[]) {
    // Parse arguments
    std::string output_dir;
    std::string custom_platform;

    if (argc >= 2) output_dir = argv[1];
    if (argc >= 3) custom_platform = argv[2];

    // Create exporter
    SigExporter ex;
    if (!custom_platform.empty()) {
        ex = SigExporter(custom_platform);
    }

    // Register types — safe
    ex.add<PacketHeader>("PacketHeader");
    ex.add<SharedMemRegion>("SharedMemRegion");
    ex.add<FileHeader>("FileHeader");
    ex.add<SensorRecord>("SensorRecord");
    ex.add<IpcCommand>("IpcCommand");

    // Register types — unsafe (platform-dependent)
    ex.add<UnsafeStruct>("UnsafeStruct");
    ex.add<UnsafeWithPointer>("UnsafeWithPointer");
    ex.add<MixedSafety>("MixedSafety");

    // Output
    if (output_dir.empty()) {
        // Write to stdout
        ex.write_stdout();
        return 0;
    } else {
        // Create directory if needed
        std::filesystem::create_directories(output_dir);

        // Write to file
        std::string path = output_dir;
        if (path.back() != '/' && path.back() != '\\') path += '/';
        path += ex.platform_name() + ".sig.hpp";
        return ex.write(path);
    }
}