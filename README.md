# Boost.TypeLayout

[![CI](https://github.com/ximicpp/TypeLayout/actions/workflows/ci.yml/badge.svg)](https://github.com/ximicpp/TypeLayout/actions/workflows/ci.yml)
[![Boost License](https://img.shields.io/badge/License-Boost%201.0-blue.svg)](https://www.boost.org/LICENSE_1_0.txt)
[![C++ Standard](https://img.shields.io/badge/C%2B%2B-26-blue.svg)](https://en.cppreference.com/w/cpp/26)
[![Header Only](https://img.shields.io/badge/Header-only-green.svg)]()

> Verify that types have identical memory layouts — at compile time, with zero overhead.

```cpp
static_assert(layout_signatures_match<MyStruct, TheirStruct>());  // ✓ Same layout = Safe to share memory
```

## The Problem

When sharing data between **processes**, **plugins**, or **network endpoints**, you must guarantee that both sides interpret memory identically. A silent layout mismatch causes:

- 🔥 **Data corruption** — wrong offsets, reading garbage
- 💥 **Crashes** — dereferencing misaligned pointers  
- 🐛 **Heisenbugs** — works on your machine, fails in production

Traditional solutions require tedious manual `static_assert` chains, external code generators, or runtime checks that come too late.

## The Solution

**One line. Compile time. Zero overhead.**

```cpp
#include <boost/typelayout.hpp>
using namespace boost::typelayout;

struct Message { uint32_t id; uint64_t timestamp; };

// Layout signature: pure byte-level identity
constexpr auto layout = get_layout_signature<Message>();
// → "[64-le]record[s:16,a:8]{@0:u32[s:4,a:4],@8:u64[s:8,a:8]}"

// Definition signature: full type structure with field names
constexpr auto defn = get_definition_signature<Message>();
// → "[64-le]record[s:16,a:8]{@0[id]:u32[s:4,a:4],@8[timestamp]:u64[s:8,a:8]}"

// Verify at compile time — mismatches become build errors
TYPELAYOUT_BIND_LAYOUT(Message, "[64-le]record[s:16,a:8]{@0:u32[s:4,a:4],@8:u64[s:8,a:8]}");
```

## Two-Layer Signature System (v2.0)

TypeLayout v2.0 introduces a mathematically grounded two-layer architecture:

```
Definition Signature ──project()──→ Layout Signature
    (many)                              (one)
```

| Layer | Purpose | Inheritance | Field Names | Use Case |
|-------|---------|-------------|-------------|----------|
| **Layout** | Pure byte identity | Flattened | Excluded | Shared memory, FFI, serialization |
| **Definition** | Full type definition | Preserved as tree | Included | Plugin ABI, ODR detection, version evolution |

**Mathematical guarantee**: `definition_match(T,U) ⟹ layout_match(T,U)` (but not vice versa)

```cpp
struct Base { int x; };
struct Derived : Base { int y; };
struct Flat { int x; int y; };

// Layout layer: same bytes → match
static_assert(layout_signatures_match<Derived, Flat>());   // ✅ Same memory layout

// Definition layer: different structure → no match
static_assert(!definition_signatures_match<Derived, Flat>()); // ❌ Different type definition

// Field names matter in Definition layer
struct PointA { float x, y; };
struct PointB { float horizontal, vertical; };

static_assert(layout_signatures_match<PointA, PointB>());       // ✅ Same layout
static_assert(!definition_signatures_match<PointA, PointB>());  // ❌ Different field names
```

### Choosing a Layer

| Use Case | Recommended Layer |
|----------|-----------------|
| Shared memory IPC | **Layout** (byte-level match is sufficient) |
| C interop / FFI | **Layout** (C has no inheritance) |
| Network protocol | **Layout** (byte identity matters) |
| Plugin ABI contracts | **Definition** (structure changes are breaking) |
| ODR violation detection | **Definition** (must match exactly) |
| Version evolution tracking | **Definition** (field names track changes) |

## Core Value: Safe Data Sharing Across Boundaries

> **Same Signature = Same Memory Layout = Safe to Share**

### 🔄 Cross-Process (Shared Memory / IPC)

```cpp
struct Packet { uint32_t id; uint64_t timestamp; char data[64]; };

// Process A: Writer
void* shm = shm_open("packets", ...);
auto* packet = new (shm) Packet{42, now(), "hello"};

// Process B: Reader (same layout hash = safe to read)
if (header->layout_hash != get_layout_hash<Packet>()) {
    throw std::runtime_error("Layout mismatch!");
}
auto* packet = static_cast<Packet*>(shm_data);
// ✅ Safe: Both processes have identical memory layout
```

### 🌐 Cross-Machine (Network / Files)

```cpp
struct Config { int32_t version; float threshold; };

// x86_64 Linux
constexpr auto sig_a = get_layout_signature<Config>();
// → "[64-le]record[s:8,a:4]{@0:i32[s:4,a:4],@4:f32[s:4,a:4]}"

// 32-bit Windows (different layout!)
// → "[32-le]record[s:8,a:4]{...}"  // Different prefix!
// Signature comparison catches this at compile time
```

### ⏳ Cross-Time (Binary Compatibility)

```cpp
// Version 1.0
struct UserData_v1 { int32_t id; char name[32]; };
constexpr uint64_t V1_HASH = get_layout_hash<UserData_v1>();

// Version 2.0 — struct changed!
struct UserData_v2 { int32_t id; char name[64]; };
constexpr uint64_t V2_HASH = get_layout_hash<UserData_v2>();
// V1_HASH != V2_HASH → Detected at compile time
```

## Why TypeLayout vs Alternatives?

| Feature | **TypeLayout** | Boost.Describe | Boost.PFR |
|---------|:--------------:|:--------------:|:---------:|
| Requires macros | ❌ No | ✅ Yes | ❌ No |
| Requires invasive changes | ❌ No | ✅ Yes | ❌ No |
| Works with third-party types | ✅ Yes | ❌ No | ⚠️ Limited |
| Layout hash for comparison | ✅ Yes | ❌ No | ❌ No |
| Captures padding/offsets | ✅ Yes | ❌ No | ❌ No |
| Bit-field support | ✅ Yes | ❌ No | ❌ No |
| Pure constexpr | ✅ Yes | ⚠️ Partial | ✅ Yes |
| Zero runtime overhead | ✅ Yes | ✅ Yes | ✅ Yes |

**TypeLayout** uses C++26 static reflection (P2996) — no macros, no intrusive annotations, works with *any* type including STL containers and third-party libraries.

## Key Features

| Feature | Description |
|---------|-------------|
| **Zero annotation** | Works with any type—including third-party and STL |
| **Two-layer signatures** | Layout (bytes) and Definition (structure) |
| **Complete layout** | Captures offsets, padding, bit-fields, inheritance, vtables |
| **Zero runtime cost** | All analysis at compile time |
| **Dual-hash verification** | FNV-1a + DJB2 for robust collision resistance |
| **Human-readable** | Easy to diff and debug |

## How It Works

TypeLayout generates a **canonical layout signature** that captures every detail affecting memory interpretation:

```
┌────────────────────────────────────────────────────────────────────┐
│  [64-le]record[s:16,a:8]{@0:u32[s:4,a:4],@8:u64[s:8,a:8]}       │
│   ▲  ▲    ▲      ▲   ▲   ▲      ▲                                │
│   │  │    │      │   │   │      └── Member type details           │
│   │  │    │      │   │   └── Field offset                         │
│   │  │    │      │   └── Overall alignment                        │
│   │  │    │      └── Overall size                                 │
│   │  │    └── Type category (record for all class/struct)         │
│   │  └── Endianness (le/be)                                       │
│   └── Pointer size (32/64)                                        │
└────────────────────────────────────────────────────────────────────┘
```

**If two types have identical Layout signatures, they have identical memory layouts** — regardless of member name, namespace, inheritance, or source file.

## API Reference

### Functions (Layer 1: Layout)

| Function | Description |
|----------|-------------|
| `get_layout_signature<T>()` | Layout signature (flattened, no names) |
| `get_layout_signature_cstr<T>()` | C-string pointer (static storage) |
| `get_layout_hash<T>()` | 64-bit FNV-1a hash |
| `layout_signatures_match<T, U>()` | Check byte-level layout compatibility |
| `layout_hashes_match<T, U>()` | Fast hash comparison |
| `get_layout_verification<T>()` | Dual-hash (FNV-1a + DJB2) verification |
| `layout_verifications_match<T, U>()` | Dual-hash comparison |

### Functions (Layer 2: Definition)

| Function | Description |
|----------|-------------|
| `get_definition_signature<T>()` | Definition signature (tree, with names) |
| `get_definition_signature_cstr<T>()` | C-string pointer (static storage) |
| `get_definition_hash<T>()` | 64-bit FNV-1a hash |
| `definition_signatures_match<T, U>()` | Check structural definition compatibility |
| `definition_hashes_match<T, U>()` | Fast hash comparison |
| `get_definition_verification<T>()` | Dual-hash verification |
| `definition_verifications_match<T, U>()` | Dual-hash comparison |

### Concepts

| Concept | Description |
|---------|-------------|
| `LayoutSupported<T>` | Type can be analyzed |
| `LayoutCompatible<T, U>` | Types have identical Layout (byte-level) signatures |
| `LayoutHashCompatible<T, U>` | Types have matching Layout hashes |
| `DefinitionCompatible<T, U>` | Types have identical Definition signatures |
| `DefinitionHashCompatible<T, U>` | Types have matching Definition hashes |

### Macros

| Macro | Description |
|-------|-------------|
| `TYPELAYOUT_ASSERT_LAYOUT_COMPATIBLE(T1, T2)` | Assert Layout match |
| `TYPELAYOUT_BIND_LAYOUT(T, Sig)` | Bind type to expected Layout signature |
| `TYPELAYOUT_ASSERT_DEFINITION_COMPATIBLE(T1, T2)` | Assert Definition match |
| `TYPELAYOUT_BIND_DEFINITION(T, Sig)` | Bind type to expected Definition signature |

### Collision Detection

| Function | Description |
|----------|-------------|
| `no_hash_collision<Types...>()` | Verify no Layout hash collisions in type library |
| `no_verification_collision<Types...>()` | Verify no dual-hash collisions |

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
    requires LayoutSupported<T>
T* map_shared_memory(const char* name) {
    auto* hdr = get_shm_header(name);
    if (hdr->layout_hash != get_layout_hash<T>()) {
        throw std::runtime_error("Layout mismatch!");
    }
    return static_cast<T*>(hdr->data_ptr);
}
```

## Example: Plugin ABI Protection

```cpp
extern "C" PluginAPI* load_plugin(const char* path) {
    auto handle = dlopen(path, RTLD_NOW);
    auto get_hash = dlsym(handle, "get_api_layout_hash");
    auto plugin_hash = reinterpret_cast<uint64_t(*)()>(get_hash)();
    
    if (plugin_hash != get_layout_hash<PluginAPI>()) {
        dlclose(handle);
        throw std::runtime_error("Plugin ABI mismatch!");
    }
    // Safe to load...
}
```

## Bugs Prevented

| Bug Type | Severity | Detection Difficulty | Prevented |
|----------|----------|----------------------|-----------|
| Cross-compiler layout differences | High | Very Hard | ✅ |
| Cross-platform layout differences | High | Very Hard | ✅ |
| Struct modification without update | Medium | Hard | ✅ |
| Version mismatch | High | Medium | ✅ |
| Bit-field layout assumptions | High | Very Hard | ✅ |
| Padding byte reads | Low | Hard | ✅ |

## Performance

### Compile-Time Overhead

TypeLayout performs all analysis at **compile time**, meaning **zero runtime cost**. The compile-time overhead is:

| Type Complexity | Members | Overhead |
|-----------------|---------|----------|
| Simple struct | 5 | ~52ms |
| Medium struct | 20 | ~243ms |
| Complex struct | 30-35 | ~293ms |

**Estimated**: ~10ms per member (linear scaling)

### Practical Limits

Due to `constexpr` step limits in current P2996 implementations:

- **Recommended**: ≤40 members per struct
- **Workaround**: Split larger structs into nested sub-structures

See [full benchmark results](bench/compile_time/RESULTS.md) for detailed methodology.

## API Stability

TypeLayout follows the Boost Library Guidelines for API stability:

- **Major version** (1.x → 2.x): May include breaking changes
- **Minor version** (2.0 → 2.1): New features only, full backward compatibility
- **Patch version** (2.0.0 → 2.0.1): Bug fixes only

```cpp
#include <boost/typelayout/core/config.hpp>
// BOOST_TYPELAYOUT_VERSION = 200000 (2.0.0)
// BOOST_TYPELAYOUT_VERSION_MAJOR = 2
// BOOST_TYPELAYOUT_VERSION_MINOR = 0
// BOOST_TYPELAYOUT_VERSION_PATCH = 0
```

## Related Work

- [P2996 - Reflection for C++26](https://wg21.link/P2996)
- [Bloomberg Clang P2996](https://github.com/bloomberg/clang-p2996)
- [Boost.PFR](https://github.com/boostorg/pfr)

## Documentation

- **Online**: https://ximicpp.github.io/TypeLayout
- **GitHub**: https://github.com/ximicpp/TypeLayout

## License

[Boost Software License 1.0](LICENSE_1_0.txt)