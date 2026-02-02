# Boost.TypeLayout

[![CI](https://github.com/ximicpp/TypeLayout/actions/workflows/ci.yml/badge.svg)](https://github.com/ximicpp/TypeLayout/actions/workflows/ci.yml)
[![Docker Build](https://github.com/ximicpp/TypeLayout/actions/workflows/docker-build.yml/badge.svg)](https://github.com/ximicpp/TypeLayout/actions/workflows/docker-build.yml)
[![Boost License](https://img.shields.io/badge/License-Boost%201.0-blue.svg)](https://www.boost.org/LICENSE_1_0.txt)
[![C++ Standard](https://img.shields.io/badge/C%2B%2B-26-blue.svg)](https://en.cppreference.com/w/cpp/26)
[![Header Only](https://img.shields.io/badge/Header-only-green.svg)]()

> **Compile-Time Memory Layout Signatures via P2996 Static Reflection**

## Overview

Boost.TypeLayout is a focused, header-only C++26 library that provides **compile-time memory layout analysis** using static reflection (P2996). It generates human-readable signatures that uniquely identify type memory layouts, enabling robust binary interface verification and ABI compatibility checking.

### 🎯 Core Capabilities

| Capability | Description |
|------------|-------------|
| **Layout Signatures** | Automatic compile-time generation of portable layout descriptions |
| **Platform Encoding** | Architecture (32/64-bit) and endianness encoded in signatures |
| **Dual-Hash Verification** | FNV-1a + DJB2 for ~2^128 collision resistance |
| **Zero Runtime Cost** | All analysis happens at compile time |
| **C++20 Concepts** | `LayoutSupported`, `LayoutCompatible`, `LayoutMatch` constraints |

### Use Cases

| Application | Description |
|-------------|-------------|
| **Shared Memory IPC** | Layout verification for cross-process data sharing |
| **Zero-Copy Network** | IDL-free wire protocol with automatic version detection |
| **Binary File Formats** | Automatic compatibility checking for save files/caches |
| **ABI Verification** | Compile-time detection of struct layout changes |

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

// Concept constraint for templates
static_assert(LayoutSupported<Point>);

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

### Using Docker (Recommended)

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
| **Arrays** | `T[N]`, `T[M][N]`, `std::array<T,N>` | `array[s:40,a:4]<i32,10>` |
| **Structs/Classes** | POD, nested, with pointers | `struct[s:8,a:4]{@0[x]:i32,...}` |
| **Inheritance** | Single, multiple, virtual | `class[inherited]{...}` |
| **Polymorphic** | Classes with virtual functions | `class[polymorphic]{...}` |
| **Unions** | Standard unions | `union[s:4,a:4]` |
| **Enums** | `enum`, `enum class` with underlying types | `enum[s:4,a:4]<u32>` |
| **Bit-fields** | With precise bit offset | `bits<4,u32>` at `@0.0` |
| **Smart Pointers** | `unique_ptr`, `shared_ptr`, `weak_ptr` | `unique_ptr[s:8,a:8]` |
| **STL Containers** | `std::pair`, `std::tuple`, `std::optional`, `std::variant` | Full internal layout |
| **std::atomic** | All atomic types | `atomic[s:4,a:4]<i32>` |
| **std::span** | Static and dynamic extent | `span[s:16,a:8,dynamic]<i32>` |
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

## API Reference

### Core Functions

| Function | Description |
|----------|-------------|
| `get_layout_signature<T>()` | Get compile-time layout signature with architecture prefix |
| `get_layout_hash<T>()` | Get 64-bit FNV-1a hash of layout signature |
| `get_layout_verification<T>()` | Get dual-hash verification (FNV-1a + DJB2 + length) |
| `signatures_match<T1, T2>()` | Check if two types have identical layout signatures |

### Concepts

| Concept | Description |
|---------|-------------|
| `LayoutSupported<T>` | Type can have its layout analyzed (fundamental check) |
| `LayoutCompatible<T, U>` | Two types have identical memory layouts |
| `LayoutMatch<T, Sig>` | Type layout matches expected signature string |
| `LayoutHashMatch<T, Hash>` | Type layout hash matches expected value |

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

### Zero-Copy Data Transfer

```cpp
template<LayoutSupported T>
void send_zero_copy(Socket& sock, const T& data) {
    // Layout is well-defined and analyzable
    sock.send(reinterpret_cast<const std::byte*>(&data), sizeof(T));
}
```

## Project Structure

```
typelayout/
├── include/boost/
│   ├── typelayout.hpp              # Main entry point
│   └── typelayout/
│       ├── typelayout.hpp          # Core facade
│       └── core/                   # Layout Signature Engine
│           ├── config.hpp          # Compiler/platform detection
│           ├── compile_string.hpp  # Compile-time string utilities
│           ├── hash.hpp            # FNV-1a + DJB2 hash
│           ├── type_signature.hpp  # Type signature specializations
│           ├── reflection_helpers.hpp # P2996 reflection utilities
│           ├── signature.hpp       # Public API
│           ├── verification.hpp    # Dual-hash verification
│           └── concepts.hpp        # C++20 concepts
├── test/
│   ├── test_all_types.cpp          # Core type tests
│   ├── test_signature_extended.cpp # Extended type coverage
│   ├── test_signature_comprehensive.cpp # Full audit
│   └── test_anonymous_member.cpp   # Anonymous member tests
├── example/
│   ├── demo.cpp                    # Quick start example
│   ├── network_protocol.cpp        # Network protocol verification
│   ├── file_format.cpp             # Binary file format versioning
│   └── shared_memory_demo.cpp      # Cross-process IPC
├── doc/                            # Antora documentation
├── CMakeLists.txt
├── Dockerfile
└── README.md
```

## Documentation

Full documentation is built using [Antora](https://antora.org/) following Boost library standards.

### Building Documentation

```bash
# Linux/macOS
cd doc && ./build-docs.sh serve

# Windows
cd doc && build-docs.cmd serve
```

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