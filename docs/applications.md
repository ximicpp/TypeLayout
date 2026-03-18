# TypeLayout Applications

Four concrete use cases with complete code. Each section describes the problem,
shows the TypeLayout solution using the actual API, and states what is and is not
guaranteed.

---

## 1. IPC and Shared Memory

### Problem

Two processes share a memory region via `mmap` or POSIX shared memory. Process A
writes a struct, Process B reads it by pointer cast. The struct definition exists
in two separate translation units (or two separate binaries). If the definitions
diverge — different field ordering, different compiler flags, or an added field —
the read produces undefined behavior silently.

```cpp
// writer.cpp (process A)
auto* shm = static_cast<SharedData*>(mmap(...));
shm->timestamp = now();
shm->value = 42.0;

// reader.cpp (process B, separately compiled)
auto* shm = static_cast<SharedData*>(mmap(...));
double v = shm->value;  // safe only if byte layout matches exactly
```

### Solution

For same-binary or same-build-system scenarios where both definitions are visible
at compile time:

```cpp
#include <boost/typelayout/typelayout.hpp>
#include <boost/typelayout/tools/classify.hpp>
using namespace boost::typelayout;

// Verify byte layout compatibility at compile time
static_assert(layout_signatures_match<WriterSharedData, ReaderSharedData>(),
    "Shared memory layout mismatch -- unsafe IPC");

// Verify that the struct contains no pointers (pointer values are
// address-space-specific and cannot be shared between processes)
static_assert(classify_v<WriterSharedData> <= SafetyLevel::PaddingRisk,
    "SharedData contains pointers or opaque fields -- unsafe for IPC");
```

For runtime handshake (processes negotiate at startup):

```cpp
#include <boost/typelayout/tools/transfer.hpp>

// In the writer process, expose the signature via a named pipe or socket:
constexpr auto local_sig = get_layout_signature<SharedData>();

// In the reader process, after receiving the writer's signature string:
if (!is_transfer_safe<SharedData>(received_sig)) {
    // Layout mismatch or safety concern -- abort
}
```

### What Is Guaranteed

- `layout_signatures_match<A, B>() == true` implies identical field offsets,
  sizes, and alignments in both types on the current platform.
- The arch prefix (`[64-le]`, `[32-le]`, etc.) distinguishes pointer widths and
  endianness, so cross-architecture mismatches are detected automatically.

### What Is NOT Guaranteed

- Pointer values are not portable across process address spaces. A layout match
  on a struct containing `int*` means the pointer field occupies the same bytes,
  not that the pointer value is meaningful in the other process.
- Padding bytes are not initialized by the language. `memcmp` on
  layout-compatible structs may return non-zero.
- Field rename (same offset and type, different name) is not detectable.
  TypeLayout operates on byte identity, not field names.

---

## 2. Network Protocol

### Problem

Client and server exchange binary packets over TCP. The packet header is a C++
struct cast directly to/from the network buffer. If the client and server struct
definitions drift — a field added at the wrong position, a type changed from
`uint32_t` to `uint64_t` — the protocol breaks silently.

```cpp
struct PacketHeader {
    uint32_t magic;
    uint16_t version;
    uint16_t type;
    uint32_t payload_len;
    uint32_t checksum;
};

// Sender
PacketHeader hdr{MAGIC, 1, MSG_DATA, payload.size(), crc32(payload)};
send(sock, &hdr, sizeof(hdr), 0);

// Receiver
PacketHeader hdr;
recv(sock, &hdr, sizeof(hdr), 0);
```

### Solution

If client and server share a header (same build), a single compile-time check
replaces all manual `offsetof`/`sizeof` assertions:

```cpp
#include <boost/typelayout/typelayout.hpp>
#include <boost/typelayout/tools/classify.hpp>
using namespace boost::typelayout;

// Replaces: static_assert(sizeof(PacketHeader) == 16);
//           static_assert(offsetof(PacketHeader, magic) == 0);
//           static_assert(offsetof(PacketHeader, checksum) == 12);
//           ... (one line per field, manually maintained)
static_assert(layout_signatures_match<ClientPacketHeader, ServerPacketHeader>(),
    "Wire format mismatch between client and server");

// Confirm no pointers and no platform-variant types in the wire struct
static_assert(classify_v<PacketHeader> <= SafetyLevel::PaddingRisk,
    "PacketHeader is not safe for wire transfer");
```

For cross-platform deployments (client on Windows, server on Linux):

```cpp
// Phase 1: export on each platform
// export_types.cpp:
#include <boost/typelayout/tools/sig_export.hpp>
#include "protocol.hpp"
TYPELAYOUT_EXPORT_TYPES(PacketHeader, ResponseHeader)

// Phase 2: compare
// check_compat.cpp:
#include "sigs/x86_64_linux_clang.sig.hpp"
#include "sigs/x86_64_windows_msvc.sig.hpp"
#include <boost/typelayout/tools/compat_auto.hpp>
TYPELAYOUT_ASSERT_COMPAT(x86_64_linux_clang, x86_64_windows_msvc)
// Compilation fails if PacketHeader or ResponseHeader differ
```

### What Is Guaranteed

- A signature match guarantees that every field starts at the same byte offset
  and occupies the same number of bytes on the current (or both compared) platforms.
- The arch prefix encodes endianness. Comparing `[64-le]` and `[64-be]` signatures
  will not match, correctly flagging the byte-order difference.

### What Is NOT Guaranteed

- Byte order conversion (`ntohl`/`htonl`) is the user's responsibility. TypeLayout
  detects endianness difference but does not convert.
- Field semantics (e.g., `timeout_ms` reinterpreted as `timeout_seconds`) are
  invisible to TypeLayout.
- Protocol versioning logic (what to do when layouts differ) is the user's
  responsibility.

---

## 3. Plugin ABI

### Problem

A host application loads plugins via `dlopen`/`LoadLibrary`. Host and plugin
exchange data through shared structs. If the plugin is compiled with different
flags (e.g., different packing or alignment), the struct layouts diverge and
data access produces undefined behavior. This is detected only at runtime, if
at all.

```cpp
// host.h (shared between host and plugin -- but compiled separately)
struct PluginConfig {
    uint32_t version;
    uint64_t flags;
    char     name[32];
};

// Host
auto* init = (PluginInitFn)dlsym(handle, "plugin_init");
PluginConfig cfg{1, 0, "default"};
init(&cfg);

// Plugin (separately compiled)
void plugin_init(const PluginConfig* cfg) {
    uint64_t f = cfg->flags;  // reads from the offset the plugin expects
}
```

### Solution

When plugin and host are built from the same source tree (compile-time check):

```cpp
#include <boost/typelayout/typelayout.hpp>
using namespace boost::typelayout;

static_assert(layout_signatures_match<HostPluginConfig, PluginPluginConfig>(),
    "Plugin ABI mismatch: PluginConfig layout differs between host and plugin");
```

When plugins are separately distributed and compiled (runtime check at load time):

```cpp
#include <boost/typelayout/tools/transfer.hpp>
using namespace boost::typelayout;

// Plugin exports its signature as a C function
extern "C" const char* plugin_config_layout() {
    static constexpr auto sig = get_layout_signature<PluginConfig>();
    return sig.c_str();
}

// Host verifies immediately after dlopen
void* handle = dlopen("plugin.so", RTLD_LAZY);
auto get_sig = (const char*(*)())dlsym(handle, "plugin_config_layout");

if (get_sig == nullptr || !is_transfer_safe<PluginConfig>(get_sig())) {
    dlclose(handle);
    return Error::ABIMismatch;
}
```

### What Is Guaranteed

- `is_transfer_safe<T>(remote_sig)` returns `true` only if both the local type
  passes compile-time safety checks and the remote signature matches the local
  signature exactly.
- Any change in field offset, size, or type causes the signatures to differ and
  the check to return `false`.

### What Is NOT Guaranteed

- Virtual function tables are not checked. A class with virtual methods exposes
  its vptr in the signature, but the vtable content (function addresses) differs
  between binaries regardless of layout match.
- Static members and member function signatures are not encoded.
- Compiler mangling and symbol visibility are outside the scope of TypeLayout.

---

## 4. Cross-Platform Binary Files

### Problem

An application writes binary files (sensor logs, database headers, config blobs)
using `fwrite`. The file must be readable on a different platform or by a future
version of the application. If struct layout changes across platforms or versions,
`fread` silently misinterprets the data.

### Solution

Use the two-phase pipeline to verify that the file header struct has identical
layout on all target platforms before shipping.

**Phase 1: Export signatures on each platform.**

```cpp
// export_file_types.cpp
#include <boost/typelayout/tools/sig_export.hpp>
#include "file_format.hpp"

TYPELAYOUT_EXPORT_TYPES(FileHeader, RecordEntry, IndexBlock)
```

Compile and run on each target:

```bash
# Linux x86-64
clang++ -std=c++26 -freflection -freflection-latest -stdlib=libc++ \
    -I./include -o sig_export export_file_types.cpp
./sig_export sigs/

# Windows x86-64 (cross-compile or native)
./sig_export sigs/
# -> sigs/x86_64_windows_msvc.sig.hpp
```

**Phase 2: Compare and assert compatibility.**

```cpp
// check_file_compat.cpp
#include "sigs/x86_64_linux_clang.sig.hpp"
#include "sigs/arm64_macos_clang.sig.hpp"
#include "sigs/x86_64_windows_msvc.sig.hpp"
#include <boost/typelayout/tools/compat_auto.hpp>

// Compile-time: fails to compile if any type differs across platforms
TYPELAYOUT_ASSERT_COMPAT(x86_64_linux_clang, arm64_macos_clang, x86_64_windows_msvc)
```

For a human-readable report instead:

```cpp
TYPELAYOUT_CHECK_COMPAT(x86_64_linux_clang, arm64_macos_clang, x86_64_windows_msvc)
```

Compile Phase 2:

```bash
clang++ -std=c++26 -freflection -freflection-latest -stdlib=libc++ \
    -I./include -o check check_file_compat.cpp
./check
```

**Using CompatReporter directly** for custom reporting logic:

```cpp
#include "sigs/x86_64_linux_clang.sig.hpp"
#include "sigs/arm64_macos_clang.sig.hpp"
#include <boost/typelayout/tools/compat_check.hpp>

namespace lx = boost::typelayout::platform::x86_64_linux_clang;
namespace mc = boost::typelayout::platform::arm64_macos_clang;

boost::typelayout::compat::CompatReporter reporter;
reporter.add_platform(lx::get_platform_info());
reporter.add_platform(mc::get_platform_info());
reporter.print_diff_report(std::cout);  // with ^--- diff annotations on mismatches
```

### What Is Guaranteed

- If Phase 2 passes (all signatures match), every file header struct has identical
  `sizeof`, `alignof`, and field offsets on all compared platforms.
- `fwrite` on one platform and `fread` on another will interpret each byte
  correctly.
- The arch prefix in the signature encodes endianness. If platforms differ in
  byte order, signatures will not match.

### What Is NOT Guaranteed

- Field rename is invisible. If `uint32_t entry_count` is semantically replaced
  by `uint32_t record_count` with the same offset and type, signatures match and
  TypeLayout reports compatibility. Data correctness across versions with semantic
  changes is the user's responsibility.
- Adding a field in the middle shifts offsets and is detected. Appending a field
  at the end changes `sizeof` and is also detected.
- TypeLayout does not manage file format versioning or migration. It only verifies
  that the current binary layout is identical on the compared platforms.
