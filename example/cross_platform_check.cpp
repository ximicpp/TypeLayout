// Cross-Platform Signature Export (Phase 1)
//
// Compile with P2996 Clang, run with: ./sig_export sigs/
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#include <boost/typelayout/tools/sig_export.hpp>
#include <cstdint>

// =========================================================================
// Types to check for cross-platform compatibility
// =========================================================================

struct PacketHeader {
    uint32_t magic;
    uint16_t version;
    uint16_t type;
    uint32_t payload_len;
    uint32_t checksum;
};

struct SharedMemRegion {
    uint64_t offset;
    uint64_t size;
    uint32_t flags;
    uint32_t owner_pid;
};

struct FileHeader {
    char     magic[4];
    uint32_t version;
    uint64_t timestamp;
    uint32_t entry_count;
    uint32_t reserved;
};

struct SensorRecord {
    uint64_t timestamp_ns;
    float    temperature;
    float    humidity;
    float    pressure;
    uint32_t sensor_id;
};

struct IpcCommand {
    uint32_t  cmd_id;
    uint32_t  flags;
    int64_t   arg1;
    int64_t   arg2;
    char      payload[64];
};

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

// One macro — generates main() that exports all types to .sig.hpp
TYPELAYOUT_EXPORT_TYPES(
    PacketHeader,
    SharedMemRegion,
    FileHeader,
    SensorRecord,
    IpcCommand,
    UnsafeStruct,
    UnsafeWithPointer,
    MixedSafety
)