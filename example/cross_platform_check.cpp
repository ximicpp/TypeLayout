// Cross-platform signature extraction tool.
// Outputs layout/definition signatures as JSON for comparison across platforms.
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#include <boost/typelayout/typelayout.hpp>
#include <iostream>
#include <cstdint>

using namespace boost::typelayout;

// --- Representative types ---

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

// Platform-dependent members (expected to differ across platforms)
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

// --- JSON output ---

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

    emit_type_entry<PacketHeader>("PacketHeader", first);
    emit_type_entry<SharedMemRegion>("SharedMemRegion", first);
    emit_type_entry<FileHeader>("FileHeader", first);
    emit_type_entry<SensorRecord>("SensorRecord", first);
    emit_type_entry<IpcCommand>("IpcCommand", first);

    emit_type_entry<UnsafeStruct>("UnsafeStruct", first);
    emit_type_entry<UnsafeWithPointer>("UnsafeWithPointer", first);

    emit_type_entry<MixedSafety>("MixedSafety", first);

    std::cout << "\n  ]\n";
    std::cout << "}\n";

    return 0;
}
