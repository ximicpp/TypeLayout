# TypeLayout Quickstart

## 1. Prerequisites

- **Compiler**: Bloomberg Clang P2996 fork with `-freflection` (C++26 static reflection)
- **Standard**: C++26 for core functions; C++17 suffices for the tools layer
- **Dependencies**: None (header-only library)

## 2. Minimal Example

Create `check.cpp`:

```cpp
#include <boost/typelayout/typelayout.hpp>
#include <iostream>
#include <cstdint>

using namespace boost::typelayout;

struct SenderMsg   { uint32_t id; uint64_t timestamp; double value; };
struct ReceiverMsg { uint32_t id; uint64_t timestamp; double value; };

static_assert(layout_signatures_match<SenderMsg, ReceiverMsg>(),
    "Binary layout mismatch -- unsafe to memcpy");

int main() {
    constexpr auto sig = get_layout_signature<SenderMsg>();
    std::cout << sig << "\n";
}
```

Compile and run:

```bash
clang++ -std=c++26 -freflection -freflection-latest -stdlib=libc++ \
    -I include check.cpp -o check
./check
```

Expected output on x86-64 little-endian:

```
[64-le]record[s:24,a:8]{@0:u32[s:4,a:4],@8:u64[s:8,a:8],@16:f64[s:8,a:8]}
```

## 3. Reading a Signature

```
[64-le]record[s:24,a:8]{@0:u32[s:4,a:4],@8:u64[s:8,a:8],@16:f64[s:8,a:8]}
  ^      ^      ^    ^    ^    ^    ^   ^
  |      |      |    |    |    |    |   alignment of this field
  |      |      |    |    |    |    size of this field
  |      |      |    |    |    field type name
  |      |      |    |    byte offset of this field
  |      |      |    alignment of the record
  |      |      total size of the record
  |      aggregate kind: record / union / enum / array
  platform prefix: pointer width + endianness
```

The gap between `@0:u32[s:4]` (covers bytes 0-3) and `@8:u64[s:8]` (starts at byte 8)
indicates four padding bytes at offsets 4-7. Padding is not encoded explicitly; it is
implied by the offset gaps.

## 4. Catching Mismatches

If fields are reordered, the static assertion fails at compile time:

```cpp
struct SenderMsg   { uint32_t id; uint64_t timestamp; double value; };
struct ReceiverMsg { uint32_t id; double value; uint64_t timestamp; }; // reordered

static_assert(layout_signatures_match<SenderMsg, ReceiverMsg>());
// error: static assertion failed -- offsets differ
```

The compiler reports the failure immediately; no runtime test is needed.

## 5. layout_traits\<T\>

`layout_traits<T>` aggregates the signature and derived properties in one place:

```cpp
#include <boost/typelayout/layout_traits.hpp>
using namespace boost::typelayout;

struct Packet {
    uint32_t id;
    double   value;  // 4 bytes of padding before this on most platforms
};

using T = layout_traits<Packet>;

static_assert(T::has_padding);           // true: gap between id and value
static_assert(!T::has_pointer);          // no pointers
static_assert(!T::has_opaque);           // no opaque-registered sub-types
static_assert(!T::is_platform_variant);  // no wchar_t or long double
static_assert(T::field_count == 2);      // direct non-static data members only
static_assert(T::total_size == sizeof(Packet));
static_assert(T::alignment == alignof(Packet));
```

All members:

| Member | Type | Description |
|--------|------|-------------|
| `signature` | `FixedString` | Full layout signature string |
| `has_pointer` | `bool` | Contains `ptr`, `fnptr`, `memptr`, `ref`, or `rref` |
| `has_bit_field` | `bool` | Contains bit-fields |
| `has_opaque` | `bool` | Contains opaque-registered sub-types (recursive) |
| `is_platform_variant` | `bool` | Contains `wchar_t` or `long double` |
| `has_padding` | `bool` | Uncovered bytes in layout (bitmap analysis, recursive) |
| `field_count` | `size_t` | Direct non-static data members (not inherited) |
| `total_size` | `size_t` | `sizeof(T)` |
| `alignment` | `size_t` | `alignof(T)` |

## 6. Safety Classification

The tools layer classifies any type or signature string into one of five tiers.

```cpp
#include <boost/typelayout/tools/classify.hpp>
#include <boost/typelayout/tools/safety_level.hpp>
using namespace boost::typelayout;

struct Safe   { uint32_t x; uint32_t y; };
struct Padded { uint32_t x; double   y; };

static_assert(classify_v<Safe>   == SafetyLevel::TrivialSafe);
static_assert(classify_v<Padded> == SafetyLevel::PaddingRisk);

// Convenience predicates:
static_assert(is_trivial_safe_v<Safe>);    // only TrivialSafe
static_assert(is_layout_compatible_v<Padded>);   // TrivialSafe or PaddingRisk
```

SafetyLevel enum (ordered best to worst):

| Enumerator | Value | Meaning |
|------------|-------|---------|
| `TrivialSafe` | 0 | Safe for zero-copy and cross-platform transfer |
| `PaddingRisk` | 1 | Padding bytes may leak uninitialized data |
| `PlatformVariant` | 2 | Layout differs across platforms |
| `PointerRisk` | 3 | Contains pointers; dangling after memcpy |
| `Opaque` | 4 | Unanalyzable; safety unknown |

Runtime classification from a string (C++17, no P2996 needed):

```cpp
#include <boost/typelayout/tools/safety_level.hpp>
SafetyLevel level = classify_signature("[64-le]record[s:8,a:4]{@0:u32[s:4,a:4],@4:u32[s:4,a:4]}");
// SafetyLevel::TrivialSafe
```

## 7. Cross-Platform Verification

For comparing types across platforms, TypeLayout uses a two-phase pipeline.

### Phase 1: Export (per-platform, requires P2996)

Write one file that lists the types to export:

```cpp
// export_types.cpp
#include <boost/typelayout/tools/sig_export.hpp>
#include "my_types.hpp"

TYPELAYOUT_EXPORT_TYPES(PacketHeader, SensorRecord, SharedMemRegion)
```

`TYPELAYOUT_EXPORT_TYPES` generates `main()`. Compile and run on each target platform:

```bash
clang++ -std=c++26 -freflection -freflection-latest -stdlib=libc++ \
    -I./include -o sig_export export_types.cpp
./sig_export sigs/
# writes sigs/x86_64_linux_clang.sig.hpp
```

Using Docker (any platform):

```bash
docker run --rm -v $(pwd):/workspace -w /workspace \
    -e LD_LIBRARY_PATH=/opt/p2996-toolchain/lib/x86_64-unknown-linux-gnu \
    ghcr.io/ximicpp/typelayout-p2996:latest \
    bash -c 'clang++ -std=c++26 -freflection -freflection-latest -stdlib=libc++ \
        -I./include -o sig_export export_types.cpp && ./sig_export sigs/'
```

### Phase 2: Check (any platform, C++17 only)

Write one file that includes the generated headers and selects the comparison macro:

```cpp
// check_compat.cpp
#include "sigs/x86_64_linux_clang.sig.hpp"
#include "sigs/arm64_macos_clang.sig.hpp"
#include "sigs/x86_64_windows_msvc.sig.hpp"
#include <boost/typelayout/tools/compat_auto.hpp>

// Option A: runtime report, exits 0
TYPELAYOUT_CHECK_COMPAT(x86_64_linux_clang, arm64_macos_clang, x86_64_windows_msvc)

// Option B: compile-time -- fails to compile on any mismatch
// TYPELAYOUT_ASSERT_COMPAT(x86_64_linux_clang, arm64_macos_clang, x86_64_windows_msvc)
```

Compile and run the checker with any C++17 compiler:

```bash
clang++ -std=c++17 -stdlib=libc++ -I./include -I./example \
    -o compat_check check_compat.cpp
./compat_check
```

### Example Report Output

```
========================================================================
  Cross-Platform Compatibility Report
========================================================================

Platforms compared: 3
  * x86_64_linux_clang [64-le]
    pointer=8B, long=8B, wchar_t=4B, long_double=16B, max_align=16B
  * arm64_macos_clang [64-le]
    pointer=8B, long=8B, wchar_t=4B, long_double=8B, max_align=16B
  * x86_64_windows_msvc [64-le]
    pointer=8B, long=4B, wchar_t=2B, long_double=8B, max_align=16B

------------------------------------------------------------------------
  Type                      Layout  Safety  Verdict
------------------------------------------------------------------------
  PacketHeader               MATCH    ***  Serialization-free
  SensorRecord               MATCH    ***  Serialization-free
  UnsafeStruct              DIFFER    *--  Needs serialization
------------------------------------------------------------------------
```

### Manual SigExporter Usage

When you need fine-grained control rather than the macro:

```cpp
boost::typelayout::SigExporter ex;                   // auto-detect platform
// boost::typelayout::SigExporter ex("my_platform"); // or explicit name
ex.add<PacketHeader>("PacketHeader");
ex.add<SensorRecord>("SensorRecord");
ex.write("sigs/x86_64_linux_clang.sig.hpp");         // returns int: 0 on success
```

### CMake Integration

```cmake
include(cmake/TypeLayoutCompat.cmake)

typelayout_add_compat_pipeline(
    NAME          my_compat
    EXPORT_SOURCE export_types.cpp
    CHECK_SOURCE  check_compat.cpp
    SIGS_DIR      ${CMAKE_SOURCE_DIR}/sigs
    ADD_TEST
)
```

## 8. Opaque Types

Types that cannot be reflected (third-party containers, non-trivial types) can be
registered as opaque so that structs containing them still produce signatures.

```cpp
#include <boost/typelayout/opaque.hpp>

namespace boost { namespace typelayout {

// Recommended form: type must be trivially copyable
TYPELAYOUT_REGISTER_OPAQUE(MyLib::Handle, "handle", false)
// Signature fragment: O(handle|8|8)

}} // namespace boost::typelayout
```

`TYPELAYOUT_REGISTER_OPAQUE(Type, Tag, HasPointer)`:
- `Type` — the C++ type to register; must satisfy `std::is_trivially_copyable_v<Type>`
- `Tag` — string tag embedded in the signature
- `HasPointer` — `true` if the type internally contains pointer-like fields

For template types, register each instantiation separately:
```cpp
TYPELAYOUT_REGISTER_OPAQUE(MyLib::XVector<int>, "xvector_int", true)
```

## 9. Runtime Handshake with is_transfer_safe

For scenarios where the remote type's signature is received at runtime (e.g., a
plugin that exports its signature string over IPC or RPC):

```cpp
#include <boost/typelayout/tools/serialization_free.hpp>
using namespace boost::typelayout;

// Plugin exports at load time:
extern "C" const char* get_packet_sig() {
    static constexpr auto sig = get_layout_signature<PacketHeader>();
    return sig.c_str();
}

// Host verifies after dlopen:
const char* remote_sig = get_packet_sig_from_plugin();
if (!is_transfer_safe<PacketHeader>(remote_sig)) {
    // layout mismatch or safety concern -- refuse the connection
}
```

For managing multiple types across a session, use `SignatureRegistry`:

```cpp
SignatureRegistry reg;
reg.register_local<PacketHeader>();
reg.register_local<SensorRecord>();

reg.register_remote("PacketHeader", received_packet_sig);
reg.register_remote("SensorRecord", received_sensor_sig);

if (!reg.is_serialization_free<PacketHeader>()) {
    std::cerr << reg.diagnose<PacketHeader>() << "\n";
}
```

## 10. CMake Integration

The library is header-only. The minimum CMake setup is:

```cmake
target_include_directories(my_target PRIVATE path/to/typelayout/include)
target_compile_options(my_target PRIVATE
    -std=c++26 -freflection -freflection-latest -stdlib=libc++)
```

## 11. Common Pitfalls

**Padding bytes are uninitialized.** `layout_signatures_match` guarantees identical
field offsets and sizes, not identical byte values. `memcmp` on layout-compatible
structs may return non-zero if padding bytes differ. Zero-fill with `= {}` or
`memset` before comparing or transmitting.

**Signature match does not imply semantic equivalence.** If a field is renamed
(`timeout_ms` to `timeout_seconds`) but its type and position are unchanged, the
signatures are identical. TypeLayout operates on byte identity, not field names.

**Unsupported types.** `void`, unbounded arrays (`T[]`), and bare function types
(`void(int)`) cannot produce signatures and will cause a compile error. Use
`void*`, `T[N]`, or function pointers (`void(*)(int)`) instead.

**Phase 1 requires P2996; Phase 2 does not.** The `.sig.hpp` files produced by
Phase 1 are plain C++17 headers. If your CI runs Phase 2 on a standard compiler,
no special flags are needed.

## Next Steps

- [API Reference](api-reference.md) — all public symbols with declarations and examples
- [Applications](applications.md) — IPC, network protocol, plugin ABI, cross-platform file use cases
- [Examples](../example/) — runnable demo sources
