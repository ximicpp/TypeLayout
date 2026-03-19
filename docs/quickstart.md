# TypeLayout Quickstart

## 1. Prerequisites

- **Compiler**: Bloomberg Clang P2996 fork with `-freflection` (C++26 static reflection)
- **Standard**: C++26
- **Dependencies**: None (header-only library)

## 2. The 3 Core Questions

Most users only need to learn TypeLayout through 3 questions:

1. What is the byte layout of `T`?
2. Is `T` safe for byte-copy transport?
3. Can local `T` be transferred to a remote endpoint?

The sections below follow that order. Opaque registration and cross-platform tools
come later as extension topics.

## 3. Minimal Example

Create `check.cpp`:

```cpp
#include <boost/typelayout/typelayout.hpp>
#include <iostream>
#include <cstdint>

using namespace boost::typelayout;

struct SenderMsg   { uint32_t id; uint64_t timestamp; double value; };
struct ReceiverMsg { uint32_t id; uint64_t timestamp; double value; };

// Compare layout signatures directly with operator==
static_assert(get_layout_signature<SenderMsg>() == get_layout_signature<ReceiverMsg>(),
    "Binary layout mismatch -- unsafe to memcpy");

// Verify byte-copy safety (no pointers, no unsafe opaque members)
static_assert(is_byte_copy_safe_v<SenderMsg>,
    "SenderMsg must be safe for byte-copy transport");

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

> Start with the 3 core APIs above. Opaque registration and the export/report tools
> are extension mechanisms that you only need for third-party opaque types or
> cross-platform workflows.
>
> See [API Reference](api-reference.md) for the full reference.

## 4. Reading a Signature

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

## 5. Catching Mismatches

If fields are reordered, the static assertion fails at compile time:

```cpp
struct SenderMsg   { uint32_t id; uint64_t timestamp; double value; };
struct ReceiverMsg { uint32_t id; double value; uint64_t timestamp; }; // reordered

static_assert(get_layout_signature<SenderMsg>() == get_layout_signature<ReceiverMsg>());
// error: static assertion failed -- offsets differ
```

The compiler reports the failure immediately; no runtime test is needed.

## 6. Runtime Handshake with is_transfer_safe

For scenarios where the remote type's signature is received at runtime (e.g., a
plugin that exports its signature string over IPC or RPC):

```cpp
#include <boost/typelayout/tools/transfer.hpp>
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

### Phase 2: Check

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

Compile and run the checker:

```bash
clang++ -std=c++26 -freflection -freflection-latest -stdlib=libc++ \
    -I./include -I./example -o compat_check check_compat.cpp
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
  PacketHeader               MATCH    ***  Transfer-safe
  SensorRecord               MATCH    ***  Transfer-safe
  UnsafeStruct              DIFFER    *--  Layout mismatch
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

Opaque registration is an extension mechanism, not part of the first-line mental
model. Reach for it only when reflection alone cannot describe a type you need to
transport.

- **Ordinary opaque**: use `TYPELAYOUT_REGISTER_OPAQUE(Type, Tag, HasPointer)` for
  trivially copyable third-party or ABI-stable blob types.
- **Relocatable opaque**: use the relocatable opaque macros for non-trivially-copyable
  wrappers and template containers whose transport safety is asserted by the user.

## 9. CMake Integration

The library is header-only. The minimum CMake setup is:

```cmake
target_include_directories(my_target PRIVATE path/to/typelayout/include)
target_compile_options(my_target PRIVATE
    -std=c++26 -freflection -freflection-latest -stdlib=libc++)
```

## 10. Common Pitfalls

**Padding bytes are uninitialized.** Matching layout signatures guarantee identical
field offsets and sizes, not identical byte values. `memcmp` on layout-compatible
structs may return non-zero if padding bytes differ. Zero-fill with `= {}` or
`memset` before comparing or transmitting.

**Signature match does not imply semantic equivalence.** If a field is renamed
(`timeout_ms` to `timeout_seconds`) but its type and position are unchanged, the
signatures are identical. TypeLayout operates on byte identity, not field names.

**Unsupported types.** `void`, unbounded arrays (`T[]`), and bare function types
(`void(int)`) cannot produce signatures and will cause a compile error. Use
`void*`, `T[N]`, or function pointers (`void(*)(int)`) instead.

**Both Phase 1 and Phase 2 require P2996.** The `.sig.hpp` files produced by
Phase 1 are plain constexpr data, but the tools layer headers require the P2996
compiler.

## 11. Next Steps

- [API Reference](api-reference.md) — all public symbols with declarations and examples
- [Applications](applications.md) — IPC, network protocol, plugin ABI, cross-platform file use cases
- [Examples](../example/) — runnable demo sources
