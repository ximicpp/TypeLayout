# TypeLayout Quickstart

Get started with TypeLayout in under 5 minutes.

## Prerequisites

- **Compiler**: Bloomberg Clang P2996 fork (C++26 with static reflection)
- **Standard**: C++26 (`-std=c++26 -freflection`)
- **Dependencies**: None (header-only library)

## 1. Minimal Example

Create a file `check.cpp`:

```cpp
#include <boost/typelayout/typelayout.hpp>
#include <iostream>
#include <cstdint>

using namespace boost::typelayout;

// Two structs in different subsystems -- are they compatible?
struct SenderMsg   { uint32_t id; uint64_t timestamp; double value; };
struct ReceiverMsg { uint32_t id; uint64_t timestamp; double value; };

// Compile-time verification: both layers
static_assert(layout_signatures_match<SenderMsg, ReceiverMsg>(),
    "Binary layout mismatch -- unsafe to memcpy");
static_assert(definition_signatures_match<SenderMsg, ReceiverMsg>(),
    "Structural mismatch -- field names or hierarchy differ");

int main() {
    // Print the signatures for inspection
    constexpr auto layout = get_layout_signature<SenderMsg>();
    constexpr auto defn   = get_definition_signature<SenderMsg>();

    std::cout << "Layout:     " << layout     << "\n";
    std::cout << "Definition: " << defn        << "\n";
    std::cout << "Match: YES\n";
    return 0;
}
```

## 2. Build and Run

```bash
cmake -B build
cmake --build build

# Or compile directly:
clang++ -std=c++26 -freflection -I include check.cpp -o check
./check
```

Expected output (on x86-64, little-endian):

```
Layout:     [64-le]record[s:24,a:8]{@0:u32[s:4,a:4],@8:u64[s:8,a:8],@16:f64[s:8,a:8]}
Definition: [64-le]record[s:24,a:8]{@0[id]:u32[s:4,a:4],@8[timestamp]:u64[s:8,a:8],@16[value]:f64[s:8,a:8]}
Match: YES
```

## 3. Catching Mismatches

If a field is renamed or reordered, the compiler catches it immediately:

```cpp
struct SenderMsg   { uint32_t id; uint64_t timestamp; double value; };
struct ReceiverMsg { uint32_t id; double value; uint64_t timestamp; };  // reordered!

// This static_assert FAILS at compile time:
static_assert(layout_signatures_match<SenderMsg, ReceiverMsg>());
//            ^^^ error: layout mismatch -- offsets differ
```

## 4. Reading a Signature

```
[64-le]record[s:24,a:8]{@0[id]:u32[s:4,a:4],@8[timestamp]:u64[s:8,a:8],@16[value]:f64[s:8,a:8]}
 ^      ^      ^    ^    ^  ^    ^    ^   ^
 |      |      |    |    |  |    |    |   alignment
 |      |      |    |    |  |    |    size
 |      |      |    |    |  |    type name
 |      |      |    |    |  field name (Definition only)
 |      |      |    |    byte offset
 |      |      |    struct alignment
 |      |      struct size
 |      aggregate type (record / union / enum)
 platform prefix: pointer width + endianness
```

## 5. Which Function to Use?

| Question | Function |
|----------|----------|
| "Can I safely `memcpy` between these types?" | `layout_signatures_match<T, U>()` |
| "Are these types structurally identical?" | `definition_signatures_match<T, U>()` |
| "I am not sure which one I need" | `definition_signatures_match` (safer default) |

`definition_match` implies `layout_match` (Projection property), so
Definition is strictly safer -- it catches everything Layout catches,
plus field renames and inheritance changes.

## 6. Cross-Platform Verification

For comparing types across different platforms (e.g., ARM64 vs x86-64),
see the [two-phase pipeline example](../example/README.md):

1. **Phase 1** (on each platform): Export signatures to `.sig.hpp` files using `SigExporter`
2. **Phase 2** (on any platform): Compare `.sig.hpp` files using `CompatReporter`

## Common Pitfalls

### Padding bytes are uninitialized

Layout signature match guarantees identical field offsets and sizes, but
**padding bytes between fields are uninitialized**. If you `memcmp` two
structs that are layout-compatible, the comparison may fail due to
differing padding contents. Always compare field-by-field, or zero-fill
the struct before use (`= {}` or `memset`).

### Unsupported types

The following types cannot produce signatures (compile error):

- `void` -- use `void*` instead
- `T[]` (unbounded array) -- use `T[N]` with a known size
- Bare function types like `void(int)` -- use function pointers `void(*)(int)`

## Next Steps

- [API Reference](api-reference.md) -- all public symbols with signatures and examples
- [Migration Guide](migration-guide.md) -- upgrading from earlier versions
- [Examples](../example/README.md) -- end-to-end pipeline demos
