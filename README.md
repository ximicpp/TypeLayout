# Boost.TypeLayout

[![CI](https://github.com/ximicpp/TypeLayout/actions/workflows/ci.yml/badge.svg)](https://github.com/ximicpp/TypeLayout/actions/workflows/ci.yml)
[![C++ Standard](https://img.shields.io/badge/C%2B%2B-26-blue.svg)](https://en.cppreference.com/w/cpp/26)
[![Header Only](https://img.shields.io/badge/Header-only-green.svg)]()
[![Boost License](https://img.shields.io/badge/License-Boost%201.0-blue.svg)](https://www.boost.org/LICENSE_1_0.txt)

Compile-time type layout signatures via C++26 static reflection (P2996).

```cpp
#include <boost/typelayout/typelayout.hpp>
using namespace boost::typelayout;

struct Message { uint32_t id; uint64_t timestamp; };

constexpr auto sig = get_layout_signature<Message>();
// "[64-le]record[s:16,a:8]{@0:u32[s:4,a:4],@8:u64[s:8,a:8]}"

static_assert(layout_signatures_match<SenderMsg, ReceiverMsg>(),
    "Binary layout mismatch -- unsafe to memcpy");
```

## What It Does

TypeLayout uses C++26 static reflection to generate deterministic, human-readable
signatures that fully describe a type's memory layout at compile time with zero
runtime cost. A signature encodes field types, sizes, alignments, and offsets;
field names and inheritance structure are erased so that only byte identity is
preserved. The core question it answers is: "Can this struct be safely `memcpy`'d
between A and B?" — where A and B may be two processes, two library versions, two
compilers, or two platforms.

## What It Does NOT Do

- Serialize or deserialize data
- Define wire formats or data exchange protocols
- Check vtables, function signatures, or full C++ ABI compatibility
- Detect field renames (byte layout unchanged = signatures unchanged)
- Convert between byte orders

## Public API

Two `consteval` functions form the core surface:

| Function | Returns | Description |
|----------|---------|-------------|
| `get_layout_signature<T>()` | `FixedString` | Full byte-level layout signature of `T` |
| `layout_signatures_match<T, U>()` | `bool` | `true` if `T` and `U` have identical byte layouts |

A `true` result from `layout_signatures_match` guarantees memcpy-compatibility:
identical field offsets, sizes, and alignments.

## Supported Types

| Category | Examples |
|----------|---------|
| Fixed-width integers | `int8_t` ... `uint64_t` |
| Fundamental types | `int`, `long`, `char`, `bool`, `float`, `double`, `long double` |
| Characters | `char8_t`, `char16_t`, `char32_t`, `wchar_t` |
| Pointers and references | `T*`, `T&`, `T&&`, `T C::*`, function pointers |
| Enums | Underlying type preserved |
| Arrays | `T[N]`; byte arrays (`char[N]`, `uint8_t[N]`, `std::byte[N]`) normalized |
| Structs and classes | Fields, padding, alignment, base classes |
| Inheritance | Single, multiple, multi-level, empty base optimization |
| Unions | Member-level signatures |

## Safety Levels

The tools layer classifies types into five tiers (ordered best to worst):

| Level | Value | Meaning |
|-------|-------|---------|
| `TrivialSafe` | 0 | Safe for zero-copy, cross-process, cross-platform transfer |
| `PaddingRisk` | 1 | Padding bytes may leak uninitialized data |
| `PlatformVariant` | 2 | Layout differs across platforms (`wchar_t`, `long double`, bit-fields) |
| `PointerRisk` | 3 | Contains pointers; dangling after memcpy |
| `Opaque` | 4 | Unanalyzable; safety unknown |

## Cross-Platform Pipeline

For comparing types across platforms, TypeLayout uses a two-phase pipeline:
Phase 1 compiles and runs a small exporter on each target platform, producing
`.sig.hpp` header files; Phase 2 includes those headers and compares signatures
using `CompatReporter` or the `TYPELAYOUT_CHECK_COMPAT` macro.

## Build and Test

```bash
cmake -B build -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_CXX_FLAGS="-std=c++26 -freflection -freflection-latest -stdlib=libc++"
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

See [CLAUDE.md](CLAUDE.md) for WSL and Docker build instructions.

## Requirements

- **Compiler**: Bloomberg Clang P2996 fork (`-freflection`)
- **Standard**: C++26
- **Dependencies**: None (header-only)

## Documentation

- [Quickstart](docs/quickstart.md) — end-to-end tutorial
- [API Reference](docs/api-reference.md) — all public symbols
- [Applications](docs/applications.md) — IPC, network, plugin, cross-platform use cases
- [Examples](example/) — runnable demo sources

## License

[Boost Software License 1.0](LICENSE_1_0.txt)
