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

constexpr auto sig = get_layout_signature<Message>();
// → "[64-le]record[s:16,a:8]{@0:u32[s:4,a:4],@8:u64[s:8,a:8]}"
```

## What It Does

TypeLayout is a **compile-time type layout verification library**. It answers one
question: *"Can my struct be safely `memcpy`'d between A and B?"* — where A and B
can be two processes, two platforms, two library versions, or a host and its plugins.

It uses C++26 static reflection (P2996) to generate **deterministic, human-readable
signatures** that fully describe a type's memory layout — at compile time, with zero
runtime cost.

A signature encodes: field types, sizes, alignments, offsets, padding, inheritance
structure, and platform characteristics (pointer width, endianness).

### Same-platform and cross-platform

```
Same-platform (compile-time, P2996 required):
  static_assert(layout_signatures_match<SenderMsg, ReceiverMsg>());

Cross-platform (two-phase, only Phase 2 needs C++17):
  Phase 1: export signatures on each platform  → .sig.hpp files
  Phase 2: compare signatures on any platform  → match / differ + safety level
```

### What TypeLayout is NOT

- **Not a serialization framework** — it does not serialize or deserialize data
- **Not a data exchange protocol** — it does not define wire formats
- **Not a full ABI checker** — it covers data layout, not vtables or function signatures

It is the **automated, complete replacement** for hand-written
`static_assert(sizeof(T) == N)` / `static_assert(offsetof(T, f) == M)` checks.

## API

Two `consteval` functions — the entire public surface for layout verification:

```cpp
namespace boost::typelayout {
    // Byte-identity comparison (offsets + sizes, no names)
    template<class T>          consteval auto get_layout_signature();
    template<class T, class U> consteval bool layout_signatures_match();
}
```

A `true` result from `layout_signatures_match<T, U>()` guarantees that `T` and `U`
are `memcpy`-compatible: identical field offsets, sizes, and alignments.

```cpp
struct SenderMsg   { uint32_t id; uint64_t timestamp; double value; };
struct ReceiverMsg { uint32_t id; uint64_t timestamp; double value; };

static_assert(layout_signatures_match<SenderMsg, ReceiverMsg>(),
    "Binary layout mismatch -- unsafe to memcpy");
```

## Core Guarantee

| Promise | Formal Statement |
|---------|-----------------|
| **Soundness** | `layout_sig(T) == layout_sig(U)` ⟹ `memcmp`-compatible byte layouts |
| **Injectivity** | Different layouts ⟹ different signatures |

## Supported Types

| Category | Examples |
|----------|---------|
| Fixed-width integers | `int8_t` … `uint64_t` |
| Fundamental types | `int`, `long`, `char`, `bool`, `float`, `double`, `long double` |
| Characters | `char8_t`, `char16_t`, `char32_t`, `wchar_t` |
| Pointers & references | `T*`, `T&`, `T&&`, `T C::*`, function pointers |
| Enums | with underlying type preserved |
| Arrays | `T[N]`, byte arrays normalized (`char[N]` ≡ `uint8_t[N]` ≡ `std::byte[N]`) |
| Structs & classes | fields, padding, alignment, base classes |
| Inheritance | single, multiple, multi-level, empty base optimization |
| Unions | member-level signatures |

## Known Design Limits

These are intentional choices, not bugs:

| Limit | Rationale |
|-------|-----------|
| Signature match is `⟹` not `⟺` | Conservative — `int[3]` and `int,int,int` are byte-identical but semantically distinct |
| Type's own name not in signature | Structural byte analysis, not nominal |
| Union members not recursively flattened | Flattening would collide overlapping members |
| Arrays not expanded to discrete fields | Preserves semantic boundary; signatures stay precise |

## Applications

TypeLayout signatures enable a range of compile-time and cross-compilation analyses:

- **Shared memory** — verify type layout before `mmap` / IPC
- **Network protocols** — prove wire-format compatibility at compile time
- **File formats** — ensure header structs match across reader/writer
- **Plugin systems** — validate ABI at load time
- **Cross-platform analysis** — compare signatures across architectures

See the [Quickstart Guide](docs/quickstart.md) for a full tutorial, or browse
[`example/`](example/) for runnable demo sources.

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
