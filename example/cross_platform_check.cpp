// Boost.TypeLayout — Cross-Platform Serialization-Free Compatibility Check
//
// This program extracts layout and definition signatures for a set of
// representative types and outputs them as JSON. By compiling and running
// this on multiple target platforms, you can diff the outputs to determine
// which types can be shared directly (via shared memory, mmap, network,
// file I/O) WITHOUT serialization.
//
// Workflow:
//   1. Compile & run on Platform A → signatures_x86_64_linux.json
//   2. Compile & run on Platform B → signatures_arm64_linux.json
//   3. python3 scripts/compare_signatures.py signatures_*.json
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#include <boost/typelayout/typelayout.hpp>
#include <iostream>
#include <cstdint>

using namespace boost::typelayout;

// =============================================================================
// Representative Types — things users actually share across platforms
// =============================================================================

// 1. Network packet header (fixed-width, should be portable)
struct PacketHeader {
    uint32_t magic;       // Protocol magic number
    uint16_t version;     // Protocol version
    uint16_t type;        // Message type
    uint32_t payload_len; // Payload length
    uint32_t checksum;    // CRC32
};

// 2. Shared-memory region descriptor (fixed-width, should be portable)
struct SharedMemRegion {
    uint64_t offset;      // Byte offset into shared segment
    uint64_t size;        // Region size in bytes
    uint32_t flags;       // Access flags
    uint32_t owner_pid;   // Owner process ID
};

// 3. File format header (fixed-width, should be portable)
struct FileHeader {
    char     magic[4];    // "TLAY"
    uint32_t version;
    uint64_t timestamp;
    uint32_t entry_count;
    uint32_t reserved;
};

// 4. Sensor data record (fixed-width + float, should be portable via IEEE 754)
struct SensorRecord {
    uint64_t timestamp_ns;  // Nanoseconds since epoch
    float    temperature;   // Celsius
    float    humidity;      // Percentage
    float    pressure;      // hPa
    uint32_t sensor_id;
};

// 5. IPC command (mixed fixed-width, should be portable)
struct IpcCommand {
    uint32_t  cmd_id;
    uint32_t  flags;
    int64_t   arg1;
    int64_t   arg2;
    char      payload[64];
};

// 6. ⚠️ UNSAFE: type with platform-dependent members
struct UnsafeStruct {
    long        a;          // 4 bytes on Windows, 8 bytes on Linux x86_64
    void*       ptr;        // 4 bytes on 32-bit, 8 bytes on 64-bit
    wchar_t     wc;         // 2 bytes on Windows, 4 bytes on Linux
    long double ld;         // 8/10/12/16 bytes depending on platform
};

// 7. ⚠️ UNSAFE: type with pointer members
struct UnsafeWithPointer {
    uint32_t  id;
    char*     name;        // Pointer — size depends on architecture
    uint64_t  timestamp;
};

// 8. Mixed: mostly safe but contains one risky field
struct MixedSafety {
    uint32_t  id;
    double    value;
    int       count;        // int is typically 4 bytes everywhere, but not guaranteed
};

// =============================================================================
// JSON Output
// =============================================================================

// Helper: print a compile-time string as a JSON-escaped string
template <typename Sig>
void print_json_string(const Sig& sig) {
    std::cout << '"';
    for (std::size_t i = 0; i < sig.length(); ++i) {
        char c = sig.value[i];
        if (c == '"') std::cout << "\\\"";
        else if (c == '\\') std::cout << "\\\\";
        else std::cout << c;
    }
    std::cout << '"';
}

// Helper: output one type entry as JSON
template <typename T>
void emit_type_entry(const char* name, bool& first) {
    if (!first) std::cout << ",\n";
    first = false;

    constexpr auto layout = get_layout_signature<T>();
    constexpr auto defn   = get_definition_signature<T>();

    std::cout << "    {\n";
    std::cout << "      \"name\": \"" << name << "\",\n";
    std::cout << "      \"size\": " << sizeof(T) << ",\n";
    std::cout << "      \"align\": " << alignof(T) << ",\n";
    std::cout << "      \"layout_signature\": ";
    print_json_string(layout);
    std::cout << ",\n";
    std::cout << "      \"definition_signature\": ";
    print_json_string(defn);
    std::cout << "\n";
    std::cout << "    }";
}

int main() {
    // Platform identification
    constexpr auto arch_prefix = get_arch_prefix();

    std::cout << "{\n";
    std::cout << "  \"platform\": {\n";
    std::cout << "    \"arch_prefix\": ";
    print_json_string(arch_prefix);
    std::cout << ",\n";
    std::cout << "    \"pointer_size\": " << sizeof(void*) << ",\n";
    std::cout << "    \"sizeof_long\": " << sizeof(long) << ",\n";
    std::cout << "    \"sizeof_wchar_t\": " << sizeof(wchar_t) << ",\n";
    std::cout << "    \"sizeof_long_double\": " << sizeof(long double) << ",\n";
    std::cout << "    \"max_align\": " << alignof(std::max_align_t) << "\n";
    std::cout << "  },\n";

    std::cout << "  \"types\": [\n";
    bool first = true;

    // Safe types (expected to be portable across all platforms)
    emit_type_entry<PacketHeader>("PacketHeader", first);
    emit_type_entry<SharedMemRegion>("SharedMemRegion", first);
    emit_type_entry<FileHeader>("FileHeader", first);
    emit_type_entry<SensorRecord>("SensorRecord", first);
    emit_type_entry<IpcCommand>("IpcCommand", first);

    // Unsafe types (expected to differ across platforms)
    emit_type_entry<UnsafeStruct>("UnsafeStruct", first);
    emit_type_entry<UnsafeWithPointer>("UnsafeWithPointer", first);

    // Mixed
    emit_type_entry<MixedSafety>("MixedSafety", first);

    std::cout << "\n  ]\n";
    std::cout << "}\n";

    return 0;
}
