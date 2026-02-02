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
| **C++20 Concepts** | `Serializable`, `LayoutCompatible`, `LayoutMatch` constraints |
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

// Serialization safety checking
static_assert(Serializable<Point>);

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

## CI Cross-Platform Compatibility Check

TypeLayout provides a complete toolset for verifying type layout compatibility across multiple platforms in your CI pipeline.

### Quick Start (2 Steps)

**Step 1: Create configuration file** (`typelayout.config.hpp`)

```cpp
#include <boost/typelayout/compat.hpp>

struct MyData { int32_t id; float value; };
struct Packet { uint64_t seq; int32_t data[4]; };

TYPELAYOUT_TYPES(MyData, Packet)
TYPELAYOUT_PLATFORMS(linux_x64, windows_x64)  // Optional
```

**Step 2: Add GitHub workflow** (`.github/workflows/compat.yml`)

```yaml
name: Type Compatibility
on: [push]
jobs:
  check:
    uses: ximicpp/typelayout/.github/workflows/compat-check.yml@main
```

### How It Works

1. CI compiles your types on each target platform
2. Generates layout signatures (hash, size, alignment)
3. Compares signatures across all platforms
4. Fails if any type has different layouts

### Example Output

```
✅ COMPATIBLE TYPES:
   MyData
   Packet

❌ INCOMPATIBLE TYPES:
   BadLongType:
   │ Platform     │ Hash               │ Size │ Align │
   │ linux-x64    │ 0x27797f26d671c700 │ 16   │ 8     │
   │ windows-x64  │ 0x4ef2fe4dace38e00 │ 8    │ 4     │
```

### Supported Platforms

| Platform | OS | Architecture | Data Model |
|----------|-----|--------------|------------|
| `linux_x64` | Linux | x86_64 | LP64 |
| `linux_arm64` | Linux | AArch64 | LP64 |
| `windows_x64` | Windows | x86_64 | LLP64 |
| `macos_arm64` | macOS | ARM64 | LP64 |

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

## Supported Types

TypeLayout provides comprehensive layout signature support for virtually all C++ types:

### Fully Supported Types

| Category | Types | Signature Examples |
|----------|-------|-------------------|
| **Integer Types** | `int8_t`, `int16_t`, `int32_t`, `int64_t`, `uint*_t` | `i32[s:4,a:4]`, `u64[s:8,a:8]` |
| **Floating Types** | `float`, `double`, `long double` | `f32[s:4,a:4]`, `f64[s:8,a:8]` |
| **Character Types** | `char`, `wchar_t`, `char8_t`, `char16_t`, `char32_t` | `char[s:1,a:1]` |
| **Boolean/Special** | `bool`, `std::byte`, `std::nullptr_t` | `bool[s:1,a:1]` |
| **Pointer Types** | `T*`, `T**`, `void*`, `const T*` | `ptr[s:8,a:8]` |
| **Function Pointers** | `R(*)(Args...)`, `noexcept` variants | `fnptr[s:8,a:8]` |
| **References** | `T&`, `T&&` | `ref[s:8,a:8]`, `rref[s:8,a:8]` |
| **Member Pointers** | `T C::*`, `R (C::*)(Args...)` | `memptr[s:8,a:8]` |
| **Arrays** | `T[N]`, `T[M][N]` | `array[s:40,a:4]<i32,10>` |
| **Structs/Classes** | POD, nested, with pointers | `struct[s:8,a:4]{@0[x]:i32,...}` |
| **Inheritance** | Single, multiple, virtual | `class[inherited]{...}` |
| **Polymorphic** | Classes with virtual functions | `class[polymorphic]{...}` |
| **Unions** | Standard unions | `union[s:4,a:4]` |
| **Enums** | `enum`, `enum class` with underlying types | `enum[s:4,a:4]<u32>` |
| **Bit-fields** | With precise bit offset | `bits<4,u32>` at `@0.0` |
| **Smart Pointers** | `unique_ptr`, `shared_ptr`, `weak_ptr` | `unique_ptr[s:8,a:8]` |
| **std::tuple** | With full internal layout | Complete field details |
| **Template Types** | User-defined templates, nested | Full recursive expansion |

### Special Attributes Support

| Attribute | Support | Notes |
|-----------|---------|-------|
| `[[no_unique_address]]` | ✅ Full | Empty base optimization detected |
| `alignas(N)` | ✅ Full | Custom alignment in signature |
| `__attribute__((packed))` | ✅ Full | Reduced padding detected |
| CV-qualifiers | ✅ Stripped | `const`/`volatile` don't affect layout |

### Anonymous Member Support

| Type | Status | Signature Example |
|------|--------|-------------------|
| Anonymous unions | ✅ Supported | `@4[<anon:1>]:union[s:4,a:4]` |
| Anonymous structs | ✅ Supported | `@0[<anon:0>]:struct[s:8,a:4]{...}` |
| `std::optional<T>` | ✅ Supported | Internal anonymous union handled |
| `std::variant<Ts...>` | ✅ Supported | Internal anonymous members handled |

> **Note**: Anonymous members use `<anon:N>` placeholder naming where `N` is the member index. This provides stable, deterministic signatures for types with anonymous members.

## API Reference

### Core Functions

| Function | Description |
|----------|-------------|
| `get_layout_signature<T>()` | Get compile-time layout signature with architecture prefix |
| `get_layout_hash<T>()` | Get 64-bit FNV-1a hash of layout signature |
| `get_layout_verification<T>()` | Get dual-hash verification (FNV-1a + DJB2 + length) |
| `signatures_match<T1, T2>()` | Check if two types have identical layout signatures |
| `is_serializable_v<T, P>` | Check if type can be safely serialized for platform set P |
| `has_bitfields<T>()` | Check if type contains bit-fields |

### Layer 2: Serialization Compatibility (New)

For cross-process or cross-machine data transfer, TypeLayout provides **Layered Signatures**:

| Layer | Purpose | API |
|-------|---------|-----|
| **Layer 1: Layout** | Identical memory layout (size, alignment, offsets) | `get_layout_signature<T>()` |
| **Layer 2: Serialization** | Safe for `memcpy` across platform set | `serialization_status<T, P>()` |

#### Serialization API

| Function | Description |
|----------|-------------|
| `is_serializable_v<T, P>` | Check if type is memcpy-safe for platform set P |
| `serialization_blocker_v<T, P>` | Get reason why type is not serializable |
| `serialization_status<T, P>()` | Get serialization status string |
| `check_serialization_compatible<T, U, P>()` | Check if T and U can be safely transmitted |

#### Platform Sets

```cpp
// Predefined platform sets (by bitwidth + endianness)
PlatformSet::bits64_le()  // 64-bit little-endian (x64, arm64, etc.)
PlatformSet::bits64_be()  // 64-bit big-endian
PlatformSet::bits32_le()  // 32-bit little-endian (x86, arm32, etc.)
PlatformSet::bits32_be()  // 32-bit big-endian
PlatformSet::current()    // Current build platform
```

#### Serialization Blocker Reasons

| Blocker | Meaning |
|---------|---------|
| `None` | Type is serializable |
| `NotTriviallyCopyable` | Type has non-trivial copy |
| `HasPointer` | Contains pointer member |
| `HasReference` | Contains reference member |
| `IsPolymorphic` | Has virtual functions |
| `HasPlatformDependentSize` | Uses `long` or similar |
| `PlatformMismatch` | Build platform doesn't match target |

#### Example

```cpp
#include <boost/typelayout/typelayout_util.hpp>
using namespace boost::typelayout;

struct Message {
    int32_t id;
    float data[8];
};

// Check for 64-bit little-endian targets
constexpr auto platform = PlatformSet::bits64_le();
static_assert(is_serializable_v<Message, platform>, "Must be serializable");

// Get diagnostic status
constexpr auto sig = serialization_status<Message, platform>();
// Result: "[64-le]serial" for valid types
// Result: "[64-le]!serial:ptr" for types with pointers

// Note: 'long' is always rejected for cross-platform serialization
struct BadData { long value; };  // Different sizes on Windows vs Linux!
static_assert(!is_serializable_v<BadData, platform>, "long is not serializable");
```

### Concepts

| Concept | Description |
|---------|-------------|
| `Serializable<T>` | Type can be safely serialized for the current platform |
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
    requires Serializable<T>
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
│   ├── typelayout.hpp           # Core layer only (recommended)
│   └── typelayout/
│       ├── typelayout.hpp       # Core layer facade
│       ├── typelayout_util.hpp  # Utility layer (serialization)
│       ├── typelayout_all.hpp   # All features combined
│       ├── core/                # Layer 1: Layout Signature Engine
│       │   ├── config.hpp       # Compiler detection
│       │   ├── compile_string.hpp
│       │   ├── hash.hpp         # FNV-1a hash
│       │   ├── signature.hpp    # get_layout_signature<T>()
│       │   ├── verification.hpp # LayoutVerification
│       │   └── concepts.hpp     # LayoutCompatible, LayoutMatch
│       ├── util/                # Layer 2: Serialization Utilities
│       │   ├── platform_set.hpp # PlatformSet, SerializationBlocker
│       │   ├── serialization_check.hpp
│       │   └── concepts.hpp     # Serializable, ZeroCopyTransmittable
│       ├── fwd.hpp              # Forward declarations
│       └── compat.hpp           # Cross-platform compatibility tools
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
