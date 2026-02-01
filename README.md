# Boost.TypeLayout

[![CI](https://github.com/ximicpp/TypeLayout/actions/workflows/ci.yml/badge.svg)](https://github.com/ximicpp/TypeLayout/actions/workflows/ci.yml)
[![Docker Build](https://github.com/ximicpp/TypeLayout/actions/workflows/docker-build.yml/badge.svg)](https://github.com/ximicpp/TypeLayout/actions/workflows/docker-build.yml)
[![Boost License](https://img.shields.io/badge/License-Boost%201.0-blue.svg)](https://www.boost.org/LICENSE_1_0.txt)
[![C++ Standard](https://img.shields.io/badge/C%2B%2B-26-blue.svg)](https://en.cppreference.com/w/cpp/26)
[![Header Only](https://img.shields.io/badge/Header-only-green.svg)]()

> **Native C++ Structs as Zero-Overhead Data Protocols — No IDL, No Codegen, Automatic Layout Verification**

## Overview

Boost.TypeLayout is a header-only C++26 library that provides compile-time memory layout analysis and verification using static reflection (P2996). It generates human-readable signatures that uniquely identify type memory layouts, enabling robust binary interface verification and ABI compatibility checking.

### 🎯 Killer Applications

| Application | Description | Comparison |
|-------------|-------------|------------|
| **🥇 Shared Memory IPC** | Zero-overhead layout verification for cross-process data sharing | vs Boost.Interprocess (no auto-verification) |
| **🥇 Zero-Copy Network** | IDL-free wire protocol with automatic version detection | vs Protobuf/Cap'n Proto (no IDL, no codegen) |
| **🥈 Binary File Formats** | Automatic compatibility checking for save files/caches | vs Manual versioning (auto-detection) |

### Key Features

| Feature | Description |
|---------|-------------|
| **Layout Signatures** | Automatic compile-time generation of portable layout descriptions |
| **Platform Detection** | Architecture and endianness encoded in signatures |
| **Portability Analysis** | Identify non-portable types at compile time |
| **Dual-Hash Verification** | FNV-1a + DJB2 for ~2^128 collision resistance |
| **Runtime Verification** | Hash values usable for network/file data verification |
| **C++20 Concepts** | `Portable`, `LayoutCompatible`, `LayoutMatch` constraints |
| **Zero Runtime Cost** | All analysis happens at compile time |

## Quick Start

```cpp
#include <boost/typelayout.hpp>
using namespace boost::typelayout;

struct Point { int32_t x, y; };

// Automatic signature generation
constexpr auto sig = get_layout_signature<Point>();
// Result: "[64-le]struct[s:8,a:4]{@0[x]:i32[s:4,a:4],@4[y]:i32[s:4,a:4]}"

// Compile-time layout verification
TYPELAYOUT_BIND(Point, "[64-le]struct[s:8,a:4]{@0[x]:i32[s:4,a:4],@4[y]:i32[s:4,a:4]}");

// Portability checking
static_assert(is_portable<Point>());

// Template constraints using concepts
template<typename T>
    requires LayoutMatch<T, "[64-le]struct[s:8,a:4]{@0[x]:i32[s:4,a:4],@4[y]:i32[s:4,a:4]}">
void process_point(const T& p) { /* ... */ }
```

## Requirements

### Compiler

Currently requires a C++26 compiler with P2996 static reflection support:

- **[Bloomberg's Clang P2996 fork](https://github.com/bloomberg/clang-p2996)** (recommended)

```bash
clang++ -std=c++26 -freflection -freflection-latest -stdlib=libc++ your_code.cpp
```

### Platform

- 64-bit or 32-bit architecture (auto-detected)
- Little-endian or big-endian (auto-detected)
- IEEE 754 floating-point required

## Building

### Using CMake

```bash
mkdir build && cd build
cmake .. -DCMAKE_CXX_COMPILER=/path/to/p2996-clang++
cmake --build .

# Run tests
./test_all_types

# Run examples
./demo
```

### Using build script

```bash
./build_and_run.sh
```

### Using Docker (Recommended for CI/Development)

A pre-built Docker image with Bloomberg Clang P2996 is available:

```bash
# Pull the P2996 development image
docker pull ghcr.io/ximicpp/typelayout-p2996:latest

# Run tests in container
docker run --rm -v $(pwd):/workspace -w /workspace \
    ghcr.io/ximicpp/typelayout-p2996:latest \
    bash -c "cmake -B build -G Ninja && cmake --build build && ctest --test-dir build"

# Interactive development
docker run -it --rm -v $(pwd):/workspace -w /workspace \
    ghcr.io/ximicpp/typelayout-p2996:latest
```

The Docker image includes:
- Bloomberg Clang P2996 fork with `-freflection` support
- CMake, Ninja build system
- libc++ standard library

## API Reference

### Core Functions

| Function | Description |
|----------|-------------|
| `get_layout_signature<T>()` | Get compile-time layout signature with architecture prefix |
| `get_layout_hash<T>()` | Get 64-bit FNV-1a hash of layout signature |
| `get_layout_verification<T>()` | Get dual-hash verification (FNV-1a + DJB2 + length) |
| `signatures_match<T1, T2>()` | Check if two types have identical layout signatures |
| `is_portable<T>()` | Check if type is portable across platforms |
| `has_bitfields<T>()` | Check if type contains bit-fields |

### Concepts

| Concept | Description |
|---------|-------------|
| `Portable<T>` | Type contains no platform-dependent members |
| `LayoutCompatible<T, U>` | Two types have identical memory layouts |
| `LayoutMatch<T, Sig>` | Type layout matches expected signature string |
| `LayoutHashMatch<T, Hash>` | Type layout hash matches expected value |
| `ZeroCopyTransmittable<T>` | Type safe for zero-copy network/IPC transmission |
| `SharedMemorySafe<T>` | Type safe for cross-process shared memory |

### Macros

```cpp
TYPELAYOUT_BIND(Type, ExpectedSig)  // Static assert layout matches
```

## Use Cases

### Binary Protocol Verification

```cpp
struct NetworkHeader {
    uint32_t magic;
    uint32_t version;
    uint64_t timestamp;
};

// Verify ABI compatibility at compile time
TYPELAYOUT_BIND(NetworkHeader, 
    "[64-le]struct[s:16,a:8]{@0[magic]:u32[s:4,a:4],@4[version]:u32[s:4,a:4],@8[timestamp]:u64[s:8,a:8]}");
```

### Cross-Platform Serialization

```cpp
template<typename T>
    requires Portable<T>
void safe_binary_write(std::ostream& os, const T& obj) {
    os.write(reinterpret_cast<const char*>(&obj), sizeof(T));
}
```

### Shared Memory Verification

```cpp
template<typename T>
    requires LayoutHashMatch<T, EXPECTED_HASH>
T* map_shared_memory(const char* name) {
    // Safe to map - layout verified at compile time
    return static_cast<T*>(shm_open_and_map(name));
}
```

### Runtime Verification (Network/File)

```cpp
// Embed hash in packet header
struct PacketHeader {
    uint64_t payload_hash;  // get_layout_hash<Payload>()
    uint32_t payload_size;
};

// Verify at runtime
bool verify_packet(const PacketHeader& hdr) {
    return hdr.payload_hash == get_layout_hash<Payload>();
}
```

## Project Structure

```
typelayout/
├── include/boost/
│   ├── typelayout.hpp           # Convenience header
│   └── typelayout/
│       └── typelayout.hpp       # Main implementation
├── test/
│   └── test_all_types.cpp       # Comprehensive tests
├── example/
│   └── demo.cpp                 # Usage examples
├── doc/                         # Antora documentation
│   ├── antora.yml               # Antora component config
│   ├── antora-playbook.yml      # Antora site config
│   ├── build-docs.sh            # Build script (Linux/macOS)
│   ├── build-docs.cmd           # Build script (Windows)
│   └── modules/ROOT/pages/      # Documentation source
├── meta/
│   └── libraries.json           # Boost metadata
├── build.jam                    # B2 build file
├── CMakeLists.txt               # CMake build file
├── LICENSE                      # Boost Software License
└── README.md
```

## Documentation

Full documentation is built using [Antora](https://antora.org/) following Boost library standards.

### Building Documentation

```bash
# Linux/macOS
cd doc
./build-docs.sh serve

# Windows
cd doc
build-docs.cmd serve
```

### Documentation Contents

- **User Guide** - Layout signatures, type support, portability, concepts
- **API Reference** - Core functions, concepts, macros, utility classes
- **Design Rationale** - Signature format, hash algorithms, compile-time design
- **Examples** - Network protocols, shared memory, serialization

## Contributing

Contributions are welcome! Please ensure:

1. Code compiles with Bloomberg Clang P2996 fork
2. All static_assert tests pass
3. Follow existing code style
4. Add tests for new features

## License

Distributed under the [Boost Software License, Version 1.0](LICENSE).

## Comparison with Alternatives

### vs Protobuf / FlatBuffers / Cap'n Proto

| Feature | Protobuf | FlatBuffers | Cap'n Proto | **TypeLayout** |
|---------|----------|-------------|-------------|----------------|
| Encode overhead | High | Low | **Zero** | **Zero** |
| Decode overhead | High | Low | **Zero** | **Zero** |
| Requires IDL | ✅ .proto | ✅ .fbs | ✅ .capnp | **❌ Native C++** |
| Code generation | ✅ protoc | ✅ flatc | ✅ capnp | **❌ None** |
| Auto layout detection | ❌ | ❌ | ❌ | **✅ Automatic** |
| Learning curve | Medium | Medium | High | **Low** |

**TypeLayout positioning**: *"Cap'n Proto performance + No IDL convenience + Automatic layout verification"*

### vs Manual Versioning

| Approach | Pros | Cons |
|----------|------|------|
| Manual version numbers | Simple | Forget to update; can't detect field reorder |
| Type name comparison | Easy | Can't detect internal changes |
| **TypeLayout hash** | **Automatic, detects all changes** | **Requires C++26** |

## Related Work

- [P2996 - Reflection for C++26](https://wg21.link/P2996)
- [Boost.PFR](https://github.com/boostorg/pfr) - Basic reflection for user-defined types
- [Bloomberg Clang P2996](https://github.com/bloomberg/clang-p2996) - Reference implementation

---

**Repository**: https://github.com/ximicpp/TypeLayout
