# Boost.TypeLayout

[![CI](https://github.com/ximicpp/TypeLayout/actions/workflows/ci.yml/badge.svg)](https://github.com/ximicpp/TypeLayout/actions/workflows/ci.yml)
[![Boost License](https://img.shields.io/badge/License-Boost%201.0-blue.svg)](https://www.boost.org/LICENSE_1_0.txt)
[![C++ Standard](https://img.shields.io/badge/C%2B%2B-26-blue.svg)](https://en.cppreference.com/w/cpp/26)
[![Header Only](https://img.shields.io/badge/Header-only-green.svg)]()

> **Compile-Time Memory Layout Signatures via P2996 Static Reflection**

## Overview

Boost.TypeLayout generates human-readable memory layout signatures at compile time using C++26 static reflection (P2996). It enables robust binary interface verification without annotations, code generation, or runtime overhead.

**Core guarantee**: *Identical signature ⟺ Identical memory layout*

## Quick Start

```cpp
#include <boost/typelayout.hpp>
using namespace boost::typelayout;

struct Message { uint32_t id; uint64_t timestamp; };

// Generate layout signature at compile time
constexpr auto sig = get_layout_signature<Message>();
// "[64-le]struct[s:16,a:8]{@0[id]:u32[s:4,a:4],@8[timestamp]:u64[s:8,a:8]}"

// Get 64-bit hash for fast comparison
constexpr auto hash = get_layout_hash<Message>();

// Verify layout at compile time
static_assert(LayoutHashMatch<Message, 0x1234567890ABCDEF>);
```

## Key Features

| Feature | Description |
|---------|-------------|
| **Zero annotation** | Works with any type—including third-party and STL |
| **Complete layout** | Captures offsets, padding, bit-fields, inheritance, vtables |
| **Zero runtime cost** | All analysis at compile time |
| **Dual-hash verification** | FNV-1a + DJB2 for robust collision resistance |
| **Human-readable** | Easy to diff and debug |

## Use Cases

- **Shared Memory IPC** — Verify layout before mapping
- **Network Protocols** — Detect version mismatch at compile time
- **Plugin Systems** — Reject incompatible binaries at load time
- **Binary Files** — Validate schema on read

## API Reference

### Functions

| Function | Description |
|----------|-------------|
| `get_layout_signature<T>()` | Complete layout signature string |
| `get_layout_signature_cstr<T>()` | C-string pointer (static storage) |
| `get_layout_hash<T>()` | 64-bit FNV-1a hash |
| `get_layout_verification<T>()` | Dual-hash verification struct |
| `signatures_match<T, U>()` | Check if two types have identical layouts |
| `hashes_match<T, U>()` | Fast hash comparison |

### Concepts

| Concept | Description |
|---------|-------------|
| `LayoutSupported<T>` | Type can be analyzed |
| `LayoutCompatible<T, U>` | Types have identical layouts |
| `LayoutMatch<T, Sig>` | Layout matches expected signature |
| `LayoutHashMatch<T, Hash>` | Layout hash matches expected value |
| `LayoutHashCompatible<T, U>` | Types have matching hashes |

## Type Support

TypeLayout supports virtually all C++ types:
- Primitives, pointers, references, arrays
- Classes with private members, inheritance, virtual functions
- Bit-fields with bit-level precision
- Enums, unions, `std::pair`, `std::tuple`, `std::optional`, `std::variant`

See [full documentation](https://ximicpp.github.io/TypeLayout) for detailed type coverage.

## Requirements

**Compiler**: Bloomberg Clang P2996 fork (C++26 with static reflection)

```bash
clang++ -std=c++26 -freflection -freflection-latest -stdlib=libc++ your_code.cpp
```

**Platform**: 32/64-bit, little/big-endian (auto-detected)

## Quick Build

```bash
# Using Docker (recommended)
docker pull ghcr.io/ximicpp/typelayout-p2996:latest
docker run --rm -v $(pwd):/workspace -w /workspace \
    ghcr.io/ximicpp/typelayout-p2996:latest \
    bash -c "cmake -B build && cmake --build build && ctest --test-dir build"

# Using CMake directly (requires P2996 compiler)
cmake -B build -DCMAKE_CXX_COMPILER=/path/to/p2996-clang++
cmake --build build
```

## Example: Shared Memory Verification

```cpp
template<typename T>
    requires LayoutHashMatch<T, EXPECTED_HASH>
T* map_shared_memory(const char* name) {
    return static_cast<T*>(shm_open_and_map(name));
}
```

## Documentation

- **Online**: https://ximicpp.github.io/TypeLayout
- **GitHub**: https://github.com/ximicpp/TypeLayout

## License

[Boost Software License 1.0](LICENSE_1_0.txt)

## Related Work

- [P2996 - Reflection for C++26](https://wg21.link/P2996)
- [Bloomberg Clang P2996](https://github.com/bloomberg/clang-p2996)
- [Boost.PFR](https://github.com/boostorg/pfr)