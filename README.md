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

## What It Does

TypeLayout uses C++26 static reflection to generate **deterministic, human-readable
signatures** that fully describe a type's memory layout — at compile time, with zero
runtime cost.

A signature encodes: field types, sizes, alignments, offsets, padding, inheritance
structure, and platform characteristics (pointer width, endianness).

## Two-Layer Signature System

TypeLayout provides two complementary layers of type identity:

```
Definition Signature ──project()──→ Layout Signature
    (many)                              (one)
```

| Layer | What it captures | Inheritance | Field Names | Use case |
|-------|-----------------|-------------|-------------|----------|
| **Layout** | Pure byte identity | Flattened | No | Binary compatibility |
| **Definition** | Full type structure | Preserved | Yes | API/ABI identity |

**Core Values**:

| # | Promise | Formal Guarantee |
|---|---------|-----------------|
| V1 | Layout **reliability** | `layout_sig(T) == layout_sig(U) ⟹ memcmp-compatible(T, U)` |
| V2 | Definition **precision** | `def_sig(T) == def_sig(U) ⟹ identical field names, types, hierarchy` |
| V3 | **Projection** relationship | `definition_match(T, U) ⟹ layout_match(T, U)` |

The converse of V3 does not hold — two types can be byte-identical (Layout match) yet
structurally different (Definition mismatch):

```cpp
struct Base { int32_t x; };
struct Derived : Base { int32_t y; };
struct Flat { int32_t x; int32_t y; };

static_assert( layout_signatures_match<Derived, Flat>());   // same bytes
static_assert(!definition_signatures_match<Derived, Flat>()); // different structure
```

## API

Four `consteval` functions — the entire public surface:

```cpp
namespace boost::typelayout {
    template<class T>        consteval auto get_layout_signature();
    template<class T>        consteval auto get_definition_signature();
    template<class T, class U> consteval bool layout_signatures_match();
    template<class T, class U> consteval bool definition_signatures_match();
}
```

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
| Polymorphic types | `virtual` marker in Definition layer |
| Unions | member-level signatures |

## Design Philosophy

TypeLayout performs **Structural Analysis**, not Nominal Analysis. Two differently-named
types with identical field names, types, and layout will produce identical Definition
signatures. The signature **does not include the type's own name** — by design, TypeLayout
answers *"are these two types structurally equivalent?"*, not *"are they the same type?"*.

## When to Use Which Layer

| Use Case | Layer | Why |
|----------|-------|-----|
| Shared memory / IPC | **Layout** | Only byte-layout compatibility matters |
| Network protocols | **Layout** | Only byte alignment and offsets matter |
| ABI verification | **Layout** | Binary compatibility check |
| Serialization versioning | **Definition** | Detects field renames and structural changes |
| API compatibility | **Definition** | Semantic-level structural consistency |
| ODR violation detection | **Definition** | Requires full structural information |

## Formal Proofs

The correctness of the two-layer signature system is formally proven in
[`PROOFS.md`](PROOFS.md). Key results:

| Theorem | Statement |
|---------|-----------|
| **Soundness** | Signature match ⟹ identical byte layout (zero false positives) |
| **Injectivity** | Different layouts ⟹ different signatures |
| **Conservativeness** | Same layout may produce different signatures (safe direction) |
| **Projection** | `definition_match ⟹ layout_match` (strict refinement) |
| **Compiler-verified** | All offsets from P2996 intrinsics, not manual calculation |

## Known Design Limits

These are intentional choices, not bugs:

| Limit | Rationale |
|-------|-----------|
| Signature match is `⟹` not `⟺` | Conservative — `int[3]` and `int,int,int` are byte-identical but semantically distinct |
| Type's own name not in signature | Structural analysis, not nominal |
| Union members not recursively flattened | Flattening would collide overlapping members |
| Arrays not expanded to discrete fields | Preserves semantic boundary; signatures stay precise |

## Applications

TypeLayout signatures enable a range of compile-time and cross-compilation analyses:

- **Shared memory** — verify type layout before `mmap` / IPC
- **Network protocols** — prove wire-format compatibility at compile time
- **File formats** — ensure header structs match across reader/writer
- **Plugin systems** — validate ABI at load time
- **Cross-platform analysis** — compare signatures across architectures

See [`example/`](example/README.md) for a multi-platform comparison demo.

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
