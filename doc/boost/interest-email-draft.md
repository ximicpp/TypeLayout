# Boost Interest Email Draft

**To:** boost@lists.boost.org  
**Subject:** [interest] Boost.TypeLayout - Compile-time Layout Verification with C++26 Reflection

---

Dear Boost Community,

I would like to gauge interest in a library for compile-time type layout verification built on C++26 static reflection (P2996).

## The Problem

Binary compatibility bugs are notoriously difficult to debug:

```cpp
// Process A (compiled with GCC, libstdc++)
struct Message { int id; std::string name; double value; };

// Process B (compiled with Clang, libc++)
struct Message { int id; std::string name; double value; };

// Same source code, but std::string has DIFFERENT memory layouts!
// Result: silent data corruption in shared memory / network protocols
```

Current solutions have significant limitations:
- **Manual `offsetof` checks**: Tedious, incomplete, doesn't support bit-fields
- **IDL-based (Protobuf, FlatBuffers)**: Requires code generation, not native C++
- **Version numbers**: Easily forgotten, doesn't detect field reordering
- **Boost.Describe**: Complete but requires macro annotations for every type

## The Solution: Boost.TypeLayout

TypeLayout leverages C++26 static reflection to automatically generate complete memory layout signatures at compile time—with zero annotations and zero runtime overhead.

```cpp
#include <boost/typelayout.hpp>

struct Point { int32_t x, y; };

// Automatic compile-time signature generation
constexpr auto sig = boost::typelayout::get_layout_signature<Point>();
// Result: "[64-le]struct[s:8,a:4]{@0[x]:i32[s:4,a:4],@4[y]:i32[s:4,a:4]}"

// 64-bit hash for runtime verification
constexpr auto hash = boost::typelayout::get_layout_hash<Point>();

// Compile-time layout compatibility check
static_assert(boost::typelayout::LayoutCompatible<PointA, PointB>);
```

## Key Features

| Feature | Description |
|---------|-------------|
| **Zero annotation** | Works with any type, including third-party and STL types |
| **Complete layout info** | Size, alignment, field offsets, bit-fields, inheritance |
| **Zero runtime overhead** | All computation happens at compile time |
| **Dual-hash verification** | FNV-1a + DJB2 for ~2^128 collision resistance |
| **Human-readable signatures** | Easy to diff and debug |

## Type Support

- Primitives (fixed-width integers, floats, pointers)
- Compound types (struct, class, union, enum)
- Inheritance hierarchies (single, multiple, virtual)
- Bit-fields (with bit-level offset and width)
- Anonymous members
- STL types (transparent reflection of actual internal layout)

## Real-World Applications

1. **Shared Memory IPC** - Verify producer/consumer layout agreement
2. **Zero-copy Networking** - Embed layout hash in message headers
3. **Plugin Systems** - Compile-time ABI contract enforcement
4. **Binary File Formats** - Detect version incompatibilities

## Comparison with Existing Solutions

| Capability | TypeLayout | Boost.PFR | Boost.Describe | Manual |
|------------|------------|-----------|----------------|--------|
| Zero annotation | ✅ | ✅ | ❌ (macros) | ❌ |
| Inheritance support | ✅ | ❌ | ✅ | ✅ |
| Bit-field support | ✅ | ❌ | Limited | ❌ |
| Offset information | ✅ | ❌ | ❌ | ✅ |
| Signature generation | ✅ | ❌ | ❌ | ❌ |
| Hash verification | ✅ | ❌ | ❌ | ❌ |

## Current Status

- **Compiler**: Requires Bloomberg Clang P2996 fork (C++26 experimental)
- **Build**: CMake + B2/Jamfile support
- **Tests**: Comprehensive test suite with Boost.Test
- **Documentation**: QuickBook reference + tutorial
- **License**: BSL-1.0

## Addressing the C++26 Dependency

I acknowledge that P2996 is not yet standardized. However:

1. **C++26 timeline**: Expected finalization in late 2026
2. **Implementation maturity**: Bloomberg's P2996 implementation is feature-complete
3. **Precedent**: Boost has historically accepted forward-looking libraries (Mp11, Hana)
4. **Unique value**: This is the only solution providing zero-annotation complete layout verification

I believe TypeLayout can serve as a reference implementation and help the community prepare for C++26 reflection.

## Links

- **GitHub**: https://github.com/ximicpp/TypeLayout
- **Documentation**: https://ximicpp.github.io/TypeLayout
- **Live Demo**: Docker image `ghcr.io/ximicpp/typelayout-p2996:latest`

## Questions for the Community

1. Is there interest in a compile-time layout verification library?
2. Are there additional use cases I should consider?
3. What concerns do you have about the C++26/P2996 dependency?
4. Would you be willing to review this library?

I welcome all feedback and suggestions.

Best regards,  
[Your Name]

---

## Email Checklist Before Sending

- [ ] Subscribe to boost mailing list first
- [ ] Replace [Your Name] with actual name
- [ ] Add hosted documentation link
- [ ] Verify GitHub repo is public
- [ ] Proofread for typos
- [ ] Test all code examples compile
