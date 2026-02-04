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
- Tested with EDG and Bloomberg Clang P2996 implementations
- CMake build support (B2 in progress)
- Comprehensive test suite

## P2996 Status

P2996 is currently at R13, in CWG/LWG wording stage (as of 2025-06-20). Given this mature state, I believe the API is essentially frozen. TypeLayout tracks the latest P2996 revisions and has been tested against both available implementations.

I would appreciate community guidance on timing:

1. **Is the Boost community open to reviewing libraries that depend on C++26 features before ratification?**
2. Would it be preferable to:
   - a) Wait for C++26 ratification (expected late 2026)
   - b) Begin the review process now with conditional acceptance
   - c) Propose TypeLayout as an "experimental" library first

I'm flexible and happy to follow whatever path the community prefers.

## Links

- GitHub: https://github.com/ximicpp/TypeLayout
- Documentation: (in README, GitHub Pages coming soon)
- Docker: (coming soon - use source build for now)

## Questions

1. Is there interest in compile-time layout verification for binary compatibility?
2. Any additional use cases I should explore?
3. What's the community's stance on C++26 dependencies at this stage?

I welcome all feedback.

Best regards,  
Fanchen Su

---

## Email Checklist Before Sending

- [ ] Subscribe to boost mailing list first (https://lists.boost.org/mailman/listinfo.cgi/boost)
- [ ] Verify GitHub repo is public and README is up-to-date
- [ ] Test all links work
- [ ] Choose optimal timing: Tuesday-Thursday, avoid major holidays
- [ ] Proofread for typos