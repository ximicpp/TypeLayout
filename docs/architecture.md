# TypeLayout Architecture

This document explains the layered design of Boost.TypeLayout and the distinction between layout verification and serialization safety.

## Core Design Principle

**TypeLayout is a layout verification tool, not a semantic verification tool.**

```
┌─────────────────────────────────────────────────────────────┐
│                    Core Layer (Layout Signature)            │
│                                                             │
│  get_layout_signature<T>() - Generates signatures for ANY  │
│                              type, including pointers       │
│  signatures_match<T1,T2>() - Compares layout signatures    │
│  get_layout_hash<T>()      - Returns 64-bit layout hash    │
│                                                             │
│  📌 Core Guarantee: identical signature ⟺ identical layout │
│  📌 No semantic judgment - pure layout description         │
└─────────────────────────────────────────────────────────────┘
                              │
                              │ Optional utility
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                Utility Layer (Serialization Safety)         │
│                                                             │
│  is_trivially_serializable<T>() - Checks if type can be    │
│                                   safely memcpy'd across   │
│                                   process boundaries       │
│  has_bitfields<T>()             - Detects bit-fields       │
│  is_platform_dependent<T>()     - Detects long, size_t...  │
│                                                             │
│  📌 Independent of signature system                        │
│  📌 Users choose to use based on their scenario            │
└─────────────────────────────────────────────────────────────┘
```

## Layout vs Semantics

### What TypeLayout Guarantees

| Aspect | Description | Guaranteed? |
|--------|-------------|-------------|
| Size | `sizeof(T)` encoded in signature | ✅ 100% |
| Alignment | `alignof(T)` encoded in signature | ✅ 100% |
| Member Offsets | Each member's byte offset | ✅ 100% |
| Architecture | `[64-le]`, `[32-be]` prefix | ✅ Detectable |
| Pointer Detection | `is_trivially_serializable<T*>()` returns `false` | ✅ 100% |

### What TypeLayout Does NOT Guarantee

| Aspect | Why Not Guaranteed | Responsibility |
|--------|-------------------|----------------|
| Enum Semantics | Only checks underlying type, not values | User |
| Type Alias Meaning | `using A = int` loses semantic info | User |
| Version Compatibility | Signatures must match exactly | User |
| Business Logic | Beyond type system scope | User |

## API Reference

### Core API (Layout Signatures)

```cpp
// Generate layout signature for ANY type
constexpr auto sig = get_layout_signature<MyStruct>();

// Compare two types' layouts
static_assert(signatures_match<TypeA, TypeB>());

// Get 64-bit hash for runtime validation
constexpr uint64_t hash = get_layout_hash<MyStruct>();
```

### Utility API (Serialization Safety)

```cpp
// Check if type is safe for cross-process transmission
static_assert(is_trivially_serializable<MyStruct>());

// Check for bit-fields (implementation-defined layout)
static_assert(!has_bitfields<MyStruct>());

// Use concept for function constraints
void send_message(TriviallySerializable auto const& msg);
```

## Trivially Serializable Criteria

A type is trivially serializable if and only if:

1. ❌ No pointer types (`T*`, `void*`)
2. ❌ No reference types (`T&`, `T&&`)
3. ❌ No member pointers (`T C::*`)
4. ❌ No `std::nullptr_t`
5. ❌ No platform-dependent types (`long`, `size_t`, etc.)
6. ❌ No bit-fields
7. ✅ All nested members and base classes are also trivially serializable

## Migration Guide

### From `is_portable` to `is_serializable_v`

The old `is_portable<T>()` API is deprecated. Update your code:

```cpp
// Old (deprecated)
static_assert(is_portable<MyType>());
void foo(Portable auto const& x);

// New (with explicit platform)
static_assert(is_serializable_v<MyType, PlatformSet::bits64_le()>);

// Or use the Serializable concept (defaults to current platform)
template<Serializable T>
void foo(const T& x);
```

The deprecated aliases still work but will emit compiler warnings.

## Design Philosophy

### Why Separate Layout from Serialization?

1. **Layout signatures are universal** - Every C++ type has a memory layout
2. **Serialization is context-specific** - Pointers are fine within one process
3. **User knows best** - Different scenarios need different constraints

### When to Use Each API

| Scenario | Use |
|----------|-----|
| Verify struct matches expected layout | `get_layout_signature<T>()` |
| Check compatibility between two types | `signatures_match<T1, T2>()` |
| Cross-process IPC/shared memory | `is_serializable_v<T, PlatformSet>` |
| Zero-copy network transmission | `ZeroCopyTransmittable<T>` concept |
| Binary file format persistence | `is_serializable_v<T, PlatformSet>` |

## Example

```cpp
#include <boost/typelayout.hpp>
using namespace boost::typelayout;

struct GoodMessage {
    int32_t type;
    float value;
};

struct BadMessage {
    int32_t type;
    void* data;  // Pointer - not serializable!
};

// Both types have valid layout signatures
constexpr auto sig1 = get_layout_signature<GoodMessage>();  // OK
constexpr auto sig2 = get_layout_signature<BadMessage>();   // OK

// But only GoodMessage is trivially serializable
static_assert(is_trivially_serializable<GoodMessage>());   // ✅
static_assert(!is_trivially_serializable<BadMessage>());   // ✅ (correctly rejected)

// Use concept to constrain IPC functions
template<TriviallySerializable T>
void send_over_ipc(const T& msg) {
    // Safe to memcpy and send
}

send_over_ipc(GoodMessage{});  // ✅ Compiles
// send_over_ipc(BadMessage{}); // ❌ Compile error
```
