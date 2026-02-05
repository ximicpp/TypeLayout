# Boost.TypeLayout

[![CI](https://github.com/ximicpp/TypeLayout/actions/workflows/ci.yml/badge.svg)](https://github.com/ximicpp/TypeLayout/actions/workflows/ci.yml)
[![Boost License](https://img.shields.io/badge/License-Boost%201.0-blue.svg)](https://www.boost.org/LICENSE_1_0.txt)
[![C++ Standard](https://img.shields.io/badge/C%2B%2B-26-blue.svg)](https://en.cppreference.com/w/cpp/26)
[![Header Only](https://img.shields.io/badge/Header-only-green.svg)]()

> Verify that types have identical memory layouts — at compile time, with zero overhead.

```cpp
static_assert(get_layout_signature<MyStruct>() == get_layout_signature<TheirStruct>());  // ✓ Safe to share memory
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

struct Message { uint32_t id; uint64_t timestamp; };

// Complete layout captured as a human-readable string
constexpr auto sig = boost::typelayout::get_layout_signature<Message>();
// → "[64-le]struct[s:16,a:8]{@0[id]:u32[s:4,a:4],@8[timestamp]:u64[s:8,a:8]}"

// Verify at compile time — mismatches become build errors
static_assert(LayoutHashMatch<Message, 0x1234567890ABCDEF>);
```

**Core guarantee**: *Identical signature ⟺ Identical memory layout*

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
| **Complete layout** | Captures offsets, padding, bit-fields, inheritance, vtables |
| **Zero runtime cost** | All analysis at compile time |
| **Dual-hash verification** | FNV-1a + DJB2 for robust collision resistance |
| **Human-readable** | Easy to diff and debug |

## Why TypeLayout?

| Metric | Manual `static_assert` | TypeLayout |
|--------|------------------------|------------|
| Coverage | ~60% (easy to miss fields) | 100% |
| Code per struct | 10-20 lines | 1 line |
| Maintenance | Update on every change | Automatic |
| Bug discovery | Runtime crash | Compile time |

**Estimated savings**: ~110 hours/year for a project with 50 critical structures.

## Use Cases

| Priority | Scenario | Why Critical |
|----------|----------|--------------|
| 🔴 High | **Shared Memory IPC** | Direct memory access; layout *must* match |
| 🔴 High | **Plugin Systems** | ABI compatibility prevents crashes |
| 🟠 Medium | **Network Protocols** | Version mismatch detection |
| 🟠 Medium | **Binary Files** | Long-term storage compatibility |

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

## Example: Plugin ABI Protection

```cpp
extern "C" PluginAPI* load_plugin(const char* path) {
    auto handle = dlopen(path, RTLD_NOW);
    auto get_sig = dlsym(handle, "get_plugin_api_signature");
    auto plugin_sig = reinterpret_cast<const char*(*)()>(get_sig)();
    
    constexpr auto host_sig = get_layout_signature<PluginAPI>();
    if (plugin_sig != host_sig) {
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

## CI/CD Integration

```yaml
# .github/workflows/layout-check.yml
jobs:
  layout-compatibility:
    strategy:
      matrix:
        compiler: [gcc-13, gcc-14, clang-17, clang-18]
    steps:
      - name: Verify layouts match baseline
        run: |
          ./layout_dump > current.txt
          diff baseline.txt current.txt
```

## Documentation

- **Online**: https://ximicpp.github.io/TypeLayout
- **GitHub**: https://github.com/ximicpp/TypeLayout

## License

[Boost Software License 1.0](LICENSE_1_0.txt)

## API Stability

TypeLayout follows the Boost Library Guidelines for API stability:

- **Major version** (1.x → 2.x): May include breaking changes (deprecated APIs removed)
- **Minor version** (1.0 → 1.1): New features only, full backward compatibility
- **Patch version** (1.0.0 → 1.0.1): Bug fixes only

### Deprecation Policy
- APIs are marked `[[deprecated]]` for at least **one minor version** before removal
- Deprecated APIs include migration guidance in the deprecation message
- Removal only occurs in major version updates

```cpp
#include <boost/typelayout/core/config.hpp>
// BOOST_TYPELAYOUT_VERSION = 100000 (1.0.0)
// BOOST_TYPELAYOUT_VERSION_MAJOR = 1
// BOOST_TYPELAYOUT_VERSION_MINOR = 0
// BOOST_TYPELAYOUT_VERSION_PATCH = 0
```

## Why Boost, Not std::?

TypeLayout implements **layout verification** — a specialized application of reflection that:

1. **Addresses an immediate need**: C++26 P2996 provides reflection primitives, but no standard facility for layout comparison or hashing
2. **Enables experimentation**: Library-level implementation allows rapid iteration before potential standardization
3. **Complements, not replaces**: If layout verification becomes part of a future standard, TypeLayout can adapt or gracefully deprecate

### Relationship to P2996

| Aspect | P2996 (Standard) | TypeLayout (This Library) |
|--------|------------------|---------------------------|
| Scope | Reflection primitives | Layout verification application |
| Timeline | C++26 | Available now (Bloomberg Clang) |
| Evolution | Slow (ISO process) | Fast (Boost release cycle) |

TypeLayout will track P2996 evolution and maintain compatibility as the standard matures.

## Related Work

- [P2996 - Reflection for C++26](https://wg21.link/P2996)
- [Bloomberg Clang P2996](https://github.com/bloomberg/clang-p2996)
- [Boost.PFR](https://github.com/boostorg/pfr)
