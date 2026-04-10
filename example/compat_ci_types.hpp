// Green-path types for the reference compatibility pipeline.
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#ifndef BOOST_TYPELAYOUT_EXAMPLE_COMPAT_CI_TYPES_HPP
#define BOOST_TYPELAYOUT_EXAMPLE_COMPAT_CI_TYPES_HPP

#include <cstdint>

struct PacketHeader {
    std::uint32_t magic;
    std::uint16_t version;
    std::uint16_t type;
    std::uint32_t payload_len;
    std::uint32_t checksum;
};

struct SharedMemRegion {
    std::uint64_t offset;
    std::uint64_t size;
    std::uint32_t flags;
    std::uint32_t owner_pid;
};

struct FileHeader {
    char          magic[4];
    std::uint32_t version;
    std::uint64_t timestamp;
    std::uint32_t entry_count;
    std::uint32_t reserved;
};

struct SensorRecord {
    std::uint64_t timestamp_ns;
    float         temperature;
    float         humidity;
    float         pressure;
    std::uint32_t sensor_id;
};

struct IpcCommand {
    std::uint32_t cmd_id;
    std::uint32_t flags;
    std::int64_t  arg1;
    std::int64_t  arg2;
    char          payload[64];
};

struct MixedSafety {
    std::uint32_t id;
    double        value;
    int           count;
};

#endif // BOOST_TYPELAYOUT_EXAMPLE_COMPAT_CI_TYPES_HPP
