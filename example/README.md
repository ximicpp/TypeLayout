# Cross-Platform Serialization-Free Compatibility Check

## What It Does

Determines whether your C++ types can be shared **directly** across different 
platforms (via shared memory, mmap, network sockets, file I/O) **without any 
serialization** — using Boost.TypeLayout's compile-time signature system.

## How It Works

```
┌──────────────┐    ┌──────────────┐    ┌──────────────┐
│  x86_64      │    │  ARM64       │    │  x86_64      │
│  Linux       │    │  Linux       │    │  Windows     │
│  GCC/Clang   │    │  Clang       │    │  MSVC        │
└──────┬───────┘    └──────┬───────┘    └──────┬───────┘
       │                   │                   │
  compile & run       compile & run       compile & run
       │                   │                   │
       ▼                   ▼                   ▼
  sig_x64_linux.json  sig_arm64.json    sig_x64_win.json
       │                   │                   │
       └───────────┬───────┘───────────────────┘
                   ▼
    python3 compare_signatures.py sig_*.json
                   │
                   ▼
         Compatibility Report
```

## Quick Start

### Step 1: Build

```bash
cmake -B build
cmake --build build
```

### Step 2: Extract Signatures on Each Platform

```bash
# On Platform A (e.g., x86_64 Linux)
./build/cross_platform_check > signatures_x86_64_linux.json

# On Platform B (e.g., ARM64 Linux)  
./build/cross_platform_check > signatures_arm64_linux.json
```

### Step 3: Compare

```bash
python3 scripts/compare_signatures.py signatures_*.json
```

### Example Output

```
========================================================================
  Boost.TypeLayout — Cross-Platform Compatibility Report
========================================================================

Platforms compared: 2
  • 64-le (signatures_x86_64_linux)
    pointer=8B, long=8B, wchar_t=4B, long_double=16B, max_align=16B
  • 64-le (signatures_x86_64_windows)
    pointer=8B, long=4B, wchar_t=2B, long_double=8B, max_align=16B

------------------------------------------------------------------------
  Type                          Size    Layout   Definition  Verdict
------------------------------------------------------------------------
  PacketHeader                    16  ✅ MATCH   ✅ MATCH   🟢 Serialization-free
  SharedMemRegion                 24  ✅ MATCH   ✅ MATCH   🟢 Serialization-free
  FileHeader                      24  ✅ MATCH   ✅ MATCH   🟢 Serialization-free
  SensorRecord                    24  ✅ MATCH   ✅ MATCH   🟢 Serialization-free
  IpcCommand                      88  ✅ MATCH   ✅ MATCH   🟢 Serialization-free
  UnsafeStruct                 40/32  ❌ DIFFER  ❌ DIFFER  🔴 Needs serialization
  UnsafeWithPointer               24  ✅ MATCH   ✅ MATCH   🟢 Serialization-free
  MixedSafety                     24  ✅ MATCH   ✅ MATCH   🟢 Serialization-free
------------------------------------------------------------------------

  75% of types are serialization-free across all platforms.
  2 type(s) need serialization for cross-platform use.
========================================================================
```

## Why This Matters

Traditional approaches to cross-platform data sharing require:
- Hand-written serialization code
- Schema languages (protobuf, FlatBuffers, Cap'n Proto)
- Runtime checks and validation

With TypeLayout, you get a **compile-time proof** that your type's memory layout
is identical across platforms. If the layout signatures match, you can safely
`memcpy`, `mmap`, or send the raw bytes over the network — zero overhead.

## Adding Your Own Types

Edit `cross_platform_check.cpp` and add your types:

```cpp
struct MyProtocolMessage {
    uint32_t msg_id;
    uint64_t timestamp;
    float    data[8];
};

// In main(), add:
emit_type_entry<MyProtocolMessage>("MyProtocolMessage", first);
```
