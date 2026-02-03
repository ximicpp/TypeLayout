# Boost Interest Email Draft

**To:** boost@lists.boost.org  
**Subject:** [interest] TypeLayout - Compile-time Layout Signatures with C++26 Reflection

---

Dear Boost Community,

I'd like to gauge interest in a library that generates compile-time memory layout signatures using C++26 static reflection (P2996).

## The Problem

C++ gives us precise control over memory layout, but **no standard way to observe or verify it at compile time**.

We can query `sizeof` and `alignof`, but the complete picture—field offsets, padding, bit-field positions, inheritance layout—remains opaque. This matters whenever types cross binary boundaries: shared memory, network protocols, file formats, plugin interfaces.

Today's options all have tradeoffs:
- **Manual checks**: `static_assert(offsetof(...) == N)` is tedious and incomplete
- **IDL-based tools**: Require code generation, can't use native C++ types
- **Macro annotations**: Intrusive, must be maintained for every type

## The Solution

TypeLayout uses P2996 static reflection to generate a complete layout signature at compile time:

```cpp
#include <boost/typelayout.hpp>

struct Message { uint32_t id; uint64_t timestamp; };

constexpr auto sig = boost::typelayout::get_layout_signature<Message>();
// "[64-le]struct[s:16,a:8]{@0[id]:u32[s:4,a:4],@8[timestamp]:u64[s:8,a:8]}"

constexpr auto hash = boost::typelayout::get_layout_hash<Message>();
// 64-bit hash for fast comparison
```

The signature captures: platform (64-bit little-endian), size, alignment, field names, offsets, and types—everything needed to verify binary compatibility.

**Core guarantee**: *Identical signature ⟺ Identical memory layout*.

## What Makes This Different

Libraries like Boost.PFR and Boost.Describe solve related problems elegantly. TypeLayout addresses a specific gap: **automatic layout verification without annotations**.

- Works with any type—including third-party libraries and STL
- Captures everything: offsets, sizes, bit-fields, inheritance, virtual tables
- Zero runtime overhead—all computation at compile time
- Human-readable for debugging, hashable for verification

## Use Cases

- **Shared Memory IPC** - Verify layout before mapping
- **Network Protocols** - Detect version mismatch on receive
- **Plugin Systems** - Reject incompatible binaries at load time
- **Binary Files** - Validate schema on read

## Current Status

- Header-only, BSL-1.0 licensed
- Requires Bloomberg Clang P2996 fork (C++26 experimental)
- CMake + B2 build support
- Comprehensive test suite

## C++26 Dependency

I'm aware P2996 isn't standardized yet. However, Bloomberg's implementation is mature, and I believe early exploration helps the community prepare for C++26 reflection. TypeLayout could serve as a proving ground for reflection-based APIs.

## Links

- GitHub: https://github.com/ximicpp/TypeLayout
- Documentation: https://ximicpp.github.io/TypeLayout
- Try it: `docker pull ghcr.io/ximicpp/typelayout-p2996:latest`

## Questions

1. Is there interest in this kind of compile-time layout verification?
2. Any use cases I should explore?
3. Concerns about the P2996 dependency?

I welcome all feedback.

Best regards,  
[Your Name]

---

## Email Checklist Before Sending

- [ ] Subscribe to boost mailing list first
- [ ] Replace [Your Name] with actual name
- [ ] Verify GitHub repo is public
- [ ] Proofread for typos