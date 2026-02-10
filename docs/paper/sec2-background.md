# §2 Background

This section provides the technical background necessary to understand
TypeLayout's design: the C++ memory layout model, the P2996 static reflection
proposal, and the limitations of existing layout verification approaches.

## 2.1 The C++ Memory Layout Model

The C++ standard specifies that an object of class type occupies a contiguous
region of storage. Within this region, the compiler determines the placement of
non-static data members, base class subobjects, and padding bytes according to
the following rules:

**Size and alignment.** Every type `T` has a `sizeof(T)` (the number of bytes
it occupies) and an `alignof(T)` (the address boundary it must satisfy). For
a struct, `alignof` is the maximum alignment among its members and base
classes, unless overridden by `alignas`; `sizeof` is padded to a multiple of
`alignof`.

**Field offsets.** Within a single access specifier block, members are laid out
in declaration order. Each member `m` is placed at the first available offset
that satisfies `alignof(type_of(m))`. (Members across different access
specifiers may be reordered in non-standard-layout types; this is
implementation-defined.) The offset is deterministic for a given platform
and compiler, and can be queried at compile time via `offsetof(T, m)` for
standard-layout types.

**Padding.** Bytes inserted between members (internal padding) and after the
last member (tail padding) to satisfy alignment constraints. Padding bytes are
*not* initialized by default and are *not* part of the value representation—
but they *are* part of the object representation and affect `sizeof`.

**Inheritance.** Base class subobjects are placed before the derived class's
own members. Empty Base Optimization (EBO) allows a base class with no
non-static data members to occupy zero bytes. Virtual inheritance introduces
additional implementation-defined offsets (typically via vptr/vtable).

**Platform dependence.** Several aspects of the layout are implementation-
defined:

| Property | Platform variance |
|----------|-------------------|
| `sizeof(long)` | 4 (Windows LLP64) vs 8 (Linux LP64) |
| `sizeof(wchar_t)` | 2 (Windows) vs 4 (Linux/macOS) |
| `sizeof(long double)` | 8 / 12 / 16 depending on platform |
| Bit-field packing | Implementation-defined (C++11 §9.6) |
| Vtable pointer size | Typically `sizeof(void*)` but layout varies |

These differences mean that the *same source code* may produce *different
object layouts* on different platforms—a fundamental challenge for cross-
platform binary compatibility.

## 2.2 C++26 Static Reflection (P2996)

P2996 [Childers et al. 2024] introduces compile-time reflection into C++26,
enabling programs to inspect type structure during constant evaluation. The
key facilities used by TypeLayout are:

**Reflection operator.** The expression `^^T` produces a `std::meta::info`
value representing type `T` at compile time.

**Member introspection.**

```cpp
// Enumerate all non-static data members
constexpr auto members = std::meta::nonstatic_data_members_of(^^T);

// For each member:
std::meta::identifier_of(m)     // field name as string_view
std::meta::type_of(m)           // reflected type of the field
std::meta::offset_of(m).bytes   // byte offset (compiler-authoritative)
std::meta::offset_of(m).bits    // bit offset within byte (for bit-fields)
```

**Base class introspection.**

```cpp
constexpr auto bases = std::meta::bases_of(^^T);
// Each base provides: type, offset, virtual/non-virtual
```

**Splice operator.** `[:info:]` converts a reflection value back into a
type or expression, enabling generic programming over reflected types.

**Bit-field support.**

```cpp
std::meta::is_bit_field(m)       // is this member a bit-field?
std::meta::bit_size_of(m)        // width in bits
```

Critically, the offsets reported by P2996 are *compiler-authoritative*: they
are the actual offsets the compiler uses for code generation, not values
derived from manual calculation. This eliminates an entire class of errors
where a verification tool disagrees with the compiler about layout.

## 2.3 Limitations of Existing Approaches

Several existing tools and techniques address aspects of type layout
verification, but none provides a complete solution:

**Manual `sizeof`/`offsetof` asserts.** As discussed in §1.1, these are
incomplete, high-maintenance, cannot compare types, and have no cross-platform
story.

**RTTI (`typeid`).** C++ runtime type information provides type identity but
no layout information. `typeid(T) == typeid(U)` tells you the types have the
same name, not the same layout.

**Boost.PFR** [Polukhin 2020]. Provides reflection-like access to aggregate
fields via structured bindings. PFR can enumerate field count and field types,
but (1) does not provide field *offsets*, (2) does not handle inheritance, and
(3) is limited to simple aggregates.

**ABI Compliance Checker** [Ponomarenko 2009]. A powerful post-build tool
that analyzes DWARF debug information to detect ABI changes. However, it
operates on compiled binaries (not source code), requires debug symbols,
runs at "build time" (not compile time), and cannot be embedded in a
`static_assert`.

**Serialization frameworks** (Protocol Buffers, FlatBuffers, Cap'n Proto).
These define their own wire formats and schema languages, providing
cross-platform safety by construction—but they require abandoning native
C++ structs in favor of generated types, and impose runtime overhead for
serialization/deserialization.

| Approach | Compile-time | Complete | Automatic | Zero-overhead | Native structs |
|----------|:---:|:---:|:---:|:---:|:---:|
| `sizeof`/`offsetof` | ✅ | ❌ | ❌ | ✅ | ✅ |
| RTTI | ❌ | ❌ | ✅ | ❌ | ✅ |
| Boost.PFR | ✅ | ❌ | ✅ | ✅ | ✅ |
| ABI Checker | ❌ | ✅ | ✅ | N/A | ✅ |
| Protobuf et al. | ✅† | ✅ | ✅ | ❌ | ❌ |
| **TypeLayout** | **✅** | **✅** | **✅** | **✅** | **✅** |

† Protobuf's verification occurs at schema compilation / code generation time,
not during C++ compilation.

TypeLayout is the first system to occupy the intersection of all five
properties: compile-time, complete, automatic, zero-overhead, and
operating on native C++ structs.
