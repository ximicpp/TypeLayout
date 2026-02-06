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

## Requirements

- **Compiler**: Bloomberg Clang P2996 fork (C++26 with static reflection)
- **Build**: `cmake -B build && cmake --build build && ctest --test-dir build`

## License

[Boost Software License 1.0](LICENSE_1_0.txt)