# Value Proposition: TypeLayout

## One-Line Pitch

> **TypeLayout**: Compile-time memory layout verification for C++ using P2996 static reflection.

> 📖 **逻辑推导**：本文档的所有主张均有严密的逻辑支撑，详见 [核心价值推导](core_value_derivation.md)。

---

## The Problem

When two separately compiled C++ modules share data structures, layout mismatches cause **silent data corruption**:

- A library update changes struct padding
- A plugin compiled with different compiler flags
- A shared memory region accessed by mismatched binaries

These bugs don't cause compile errors. The code builds, links, and runs—until it corrupts memory. Symptoms appear far from the root cause, making diagnosis extremely difficult.

**Current workarounds fail:**
- Manual version numbers get forgotten
- IDL-based serialization requires rewriting all types
- Static analyzers detect problems after the fact, not prevent them

---

## The Solution

TypeLayout generates a **deterministic memory layout signature** for any C++ type at compile time:

```cpp
constexpr auto hash = get_layout_hash<MyStruct>();
```

**The core guarantee**: *Same hash ⟺ Same memory layout*

With this hash, you can:

```cpp
// Compile-time: Fail build if layout changed from contract
static_assert(get_layout_hash<Message>() == PROTOCOL_V1_HASH,
              "ABI contract violated!");
```

---

## The Core Value

### Compile-Time ABI Prevention

TypeLayout shifts ABI verification from **runtime detection** to **compile-time prevention**:

| Traditional Approach | TypeLayout |
|---------------------|------------|
| Bug discovered in production | Bug caught at compile time |
| Crash / corruption / security hole | Compilation error |
| Hours of debugging | Immediate, clear failure |

This is only possible because P2996 static reflection provides:
- Access to private members
- Exact byte offsets
- Bit-field layouts
- Inheritance hierarchies

All computed at compile time, with zero runtime cost.

---

## Safe Data Sharing Across Three Boundaries

The core guarantee—**Same Signature ⟺ Same Memory Layout**—enables safe data sharing across three critical boundaries:

### 🔄 Cross-Process (Shared Memory / IPC)

Data in shared memory can be safely accessed by multiple processes when signatures match:

```cpp
// Process A: Writer
struct SharedData { int32_t id; float value; };
TYPELAYOUT_BIND(SharedData, "[64-le]struct[s:8,a:4]{...}");

void* shm = create_shared_memory("my_shm", sizeof(SharedData));
new (shm) SharedData{42, 3.14f};
```

```cpp
// Process B: Reader (different compilation unit)
struct SharedData { int32_t id; float value; };
TYPELAYOUT_BIND(SharedData, "[64-le]struct[s:8,a:4]{...}");  // Same signature = safe!

auto* data = static_cast<SharedData*>(open_shared_memory("my_shm"));
assert(data->id == 42);  // Guaranteed correct
```

### 🌐 Cross-Machine (Network / Files)

Binary data transmitted over networks or stored in files can be safely interpreted when signatures match:

```cpp
// Server (x86_64 Linux)
struct Packet { uint32_t seq; uint64_t ts; };
constexpr auto PACKET_HASH = get_layout_hash<Packet>();

void send(socket, &packet, sizeof(Packet), PACKET_HASH);
```

```cpp
// Client (ARM64 macOS)
struct Packet { uint32_t seq; uint64_t ts; };
static_assert(get_layout_hash<Packet>() == EXPECTED_HASH,
    "Binary protocol mismatch!");

recv(socket, &packet, sizeof(Packet));  // Safe if hash matches
```

### ⏳ Cross-Time (Binary Compatibility / Versioning)

Saved binary data remains valid across software versions when signatures are preserved:

```cpp
// Version 1.0 (2024) - saved to disk
struct Config { int32_t flags; float threshold; };
TYPELAYOUT_BIND(Config, "[64-le]struct[s:8,a:4]{...}");
save_binary("config.bin", &config);
```

```cpp
// Version 2.0 (2026) - loads old files
struct Config { int32_t flags; float threshold; };
TYPELAYOUT_BIND(Config, "[64-le]struct[s:8,a:4]{...}");  // Must match v1.0!
load_binary("config.bin", &config);  // Safe if signature unchanged
```

---

## What the Signature Guarantees

| If Signatures Match | Guaranteed? |
|---------------------|-------------|
| Same `sizeof(T)` | ✅ Yes |
| Same `alignof(T)` | ✅ Yes |
| Same member offsets | ✅ Yes |
| Same padding distribution | ✅ Yes (implicit) |
| Same bit-field layout | ✅ Yes |
| Safe `memcpy` / binary copy | ✅ Yes |
| Safe pointer cast | ✅ Yes |

---

## Summary

| Aspect | TypeLayout |
|--------|------------|
| **What** | Memory layout fingerprinting |
| **When** | Compile time |
| **Cost** | Zero runtime overhead |
| **Requires** | No code changes, no IDL, no macros |
| **Enables** | ABI contract enforcement via `static_assert` |