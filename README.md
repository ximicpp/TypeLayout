# Boost.TypeLayout

[![CI](https://github.com/ximicpp/TypeLayout/actions/workflows/ci.yml/badge.svg)](https://github.com/ximicpp/TypeLayout/actions/workflows/ci.yml)
[![C++ Standard](https://img.shields.io/badge/C%2B%2B-26-blue.svg)](https://en.cppreference.com/w/cpp/26)
[![Header Only](https://img.shields.io/badge/Header-only-green.svg)]()
[![Boost License](https://img.shields.io/badge/License-Boost%201.0-blue.svg)](https://www.boost.org/LICENSE_1_0.txt)

> Compile-time type layout signatures via C++26 static reflection (P2996).

```cpp
#include <boost/typelayout.hpp>
using namespace boost::typelayout;

struct Message { uint32_t id; uint64_t timestamp; };

// Two layers of compile-time layout analysis:
constexpr auto layout = get_layout_signature<Message>();
// → "[64-le]record[s:16,a:8]{@0:u32[s:4,a:4],@8:u64[s:8,a:8]}"

constexpr auto defn = get_definition_signature<Message>();
// → "[64-le]record[s:16,a:8]{@0[id]:u32[s:4,a:4],@8[timestamp]:u64[s:8,a:8]}"
```

## The Problem

When sharing data across platforms — via shared memory, mmap, network sockets,
or file I/O — you need to know: **does this type have the exact same memory
layout on both sides?** Get it wrong and you get silent data corruption.

Traditional solutions (protobuf, FlatBuffers, manual serialization) add runtime
overhead and complexity. TypeLayout answers this question **at compile time, with
zero overhead**.

## Two-Layer Signature System

```
Definition Signature ──project()──→ Layout Signature
    (many)                              (one)
```

| Layer | What it captures | Inheritance | Field Names |
|-------|-----------------|-------------|-------------|
| **Layout** | Pure byte identity | Flattened | No |
| **Definition** | Full type structure | Preserved | Yes |

**Guarantee**: `definition_match(T,U) ⟹ layout_match(T,U)`

```cpp
struct Base { int x; };
struct Derived : Base { int y; };
struct Flat { int x; int y; };

// Same bytes → Layout matches
static_assert(layout_signatures_match<Derived, Flat>());

// Different structure → Definition differs
static_assert(!definition_signatures_match<Derived, Flat>());
```

## API (4 functions)

```cpp
get_layout_signature<T>()             // Pure byte layout
get_definition_signature<T>()         // Full type definition
layout_signatures_match<T, U>()       // Byte-level comparison
definition_signatures_match<T, U>()   // Structural comparison
```

## Cross-Platform Compatibility Check

The killer application: determine which types can be shared across platforms
**without serialization**.

```bash
# Step 1: Compile & run on each target platform
./build/cross_platform_check > sig_linux.json    # on Linux x86_64
./build/cross_platform_check > sig_windows.json  # on Windows x86_64

# Step 2: Compare
python3 scripts/compare_signatures.py sig_linux.json sig_windows.json
```

**Example output:**

```
Platforms compared: 2
  • 64-le (sig_linux)    — pointer=8B, long=8B, wchar_t=4B, long_double=16B
  • 64-le (sig_windows)  — pointer=8B, long=4B, wchar_t=2B, long_double=8B

  Type                       Size     Layout     Definition  Verdict
  PacketHeader                 16   ✅ MATCH     ✅ MATCH   🟢 Serialization-free
  SharedMemRegion              24   ✅ MATCH     ✅ MATCH   🟢 Serialization-free
  SensorRecord                 24   ✅ MATCH     ✅ MATCH   🟢 Serialization-free
  UnsafeStruct              48/24   ❌ DIFFER    ❌ DIFFER  🔴 Needs serialization
```

Types using fixed-width integers (`uint32_t`, `int64_t`, `float`, `double`)
are portable. Types using `long`, `wchar_t`, `long double`, or pointers
will differ across platforms. See [`example/README.md`](example/README.md)
for the full workflow.

## Project Structure

```
include/boost/typelayout/
  core/signature.hpp        ← 4 public API functions
  core/type_signature.hpp   ← signature generation engine
  core/reflection_helpers.hpp ← P2996 reflection utilities
  core/compile_string.hpp   ← compile-time string type
  core/config.hpp           ← platform detection
example/
  cross_platform_check.cpp  ← signature extraction program (JSON output)
  README.md                 ← cross-platform workflow guide
scripts/
  compare_signatures.py     ← multi-platform signature comparison tool
test/
  test_two_layer.cpp        ← core test suite
```

## Build & Test

```bash
cmake -B build
cmake --build build
ctest --test-dir build
```

## Requirements

- **Compiler**: Bloomberg Clang P2996 fork (C++26 with static reflection)
- **Standard**: C++26
- **Dependencies**: None (header-only)

## License

[Boost Software License 1.0](LICENSE_1_0.txt)