# §7 Related Work

TypeLayout intersects several lines of research: type introspection and
reflection, ABI compatibility checking, serialization frameworks, and
formal verification of systems software.

## 7.1 Type Introspection and Reflection

**C++ RTTI.** The C++ standard provides `typeid` and `dynamic_cast` for
runtime type identification. However, RTTI provides only type *identity*
(name comparison), not type *structure*. `typeid(T) == typeid(U)` means
the types have the same mangled name—it says nothing about their memory
layout. Furthermore, RTTI incurs runtime overhead and requires virtual
functions to be meaningful.

**Boost.PFR** [Polukhin 2018] provides "magic" aggregate reflection via
structured bindings, enabling field-by-field access without macros. However,
PFR (1) does not expose field offsets or sizes, (2) does not handle
inheritance, (3) is limited to simple aggregates, and (4) does not generate
a comparable signature. TypeLayout uses P2996 reflection which provides
compiler-authoritative offset information for all type categories.

**P2996 static reflection** [Childers et al. 2024] is the enabling
technology for TypeLayout. Prior to P2996, no standard C++ mechanism could
enumerate fields, retrieve offsets, and inspect inheritance at compile time.
TypeLayout is, to our knowledge, the first library to leverage P2996 for
type layout verification. Other early P2996 applications focus on
serialization, code generation, and metaprogramming utilities.

**Boost.Describe** [Dimov 2021] provides macro-based type description for
reflection-like functionality. Unlike P2996, it requires manual annotation
of each type, does not provide offset information, and cannot verify layout
compatibility.

## 7.2 ABI Compatibility Checking

**ABI Compliance Checker** (ACC) [Ponomarenko 2009] is the most
comprehensive existing ABI verification tool. It analyzes DWARF debug
information from compiled binaries to detect ABI-breaking changes between
library versions. ACC covers data layout changes, virtual table
modifications, function signature changes, and symbol visibility.

TypeLayout and ACC are complementary. TypeLayout operates at compile time on
source code, covering approximately 44% of ACC's feature set (the data
layout portion). ACC operates post-build on binaries, covering the full ABI
surface including vtables and symbols. TypeLayout provides formal correctness
proofs; ACC does not. TypeLayout can be embedded in `static_assert`; ACC
is an external tool.

**libabigail** [Red Hat] is another DWARF-based ABI analysis tool, used
by Linux distributions to verify library ABI stability. Like ACC, it
operates on compiled binaries and covers the full ABI surface.

**Microsoft's C++ ABI annotations** (`__declspec(uuid(...))`,
`#pragma detect_mismatch`) provide limited ABI checking capabilities within
MSVC. These are compiler-specific, manual, and do not provide systematic
layout verification.

## 7.3 Serialization Frameworks

**Protocol Buffers** [Google 2008], **FlatBuffers** [Google 2014],
**Cap'n Proto** [Varda 2013] solve cross-platform data exchange by defining
explicit schema languages and wire formats. They guarantee
cross-platform safety by *construction*—the schema defines the layout,
and code generators produce platform-conformant accessors.

TypeLayout takes a fundamentally different approach: instead of replacing
native structs with generated types, it *verifies* that existing native
structs have compatible layouts. This is valuable in contexts where
serialization overhead is unacceptable (shared memory, memory-mapped I/O)
or where native structs cannot be replaced (legacy codebases, hardware
register mappings).

**Comparison:** Serialization frameworks provide stronger cross-platform
guarantees (they *define* the layout) but impose runtime overhead and
require non-native types. TypeLayout provides weaker guarantees (it
*verifies* existing layouts) but with zero overhead and native types.

## 7.4 Formal Verification of Systems Software

**seL4** [Klein et al. 2009] is the gold standard for formally verified
systems software: a complete functional correctness proof of an OS kernel
in Isabelle/HOL. TypeLayout's proof methodology draws inspiration from
seL4's approach—both use *refinement* to relate abstract specifications
to concrete implementations.

**CompCert** [Leroy 2009] is a formally verified C compiler whose core
theorem (semantic preservation) is structurally analogous to TypeLayout's
Encoding Faithfulness theorem: CompCert proves that compilation preserves
program semantics; TypeLayout proves that signature generation preserves
layout information.

**Cogent** [O'Connor et al. 2016] is a restricted systems language with
a certifying compiler that produces C code with formal proofs. Like
TypeLayout, Cogent uses a refinement-based approach to relate high-level
specifications to low-level implementations.

TypeLayout operates at a smaller scale than these systems—it verifies a
specific property (type layout compatibility) rather than full functional
correctness. However, it shares their philosophical commitment to formal
proofs as a means of establishing trust in safety-critical tools.

## 7.5 Structural vs Nominal Type Equivalence

The distinction between TypeLayout's Layout and Definition signatures
connects to a classical PL theory question: *structural* vs *nominal*
type equivalence [Pierce 2002].

In nominal type systems (Java, C#, most of C++), two types are equivalent
only if they have the same name. In structural type systems (OCaml modules,
TypeScript, Go interfaces), two types are equivalent if they have the same
structure.

TypeLayout occupies a middle ground: its Definition layer uses *structural
equivalence with names* (same field names and types, regardless of the
type's own name), while its Layout layer uses *pure structural equivalence*
(same byte layout, regardless of names). This two-layer design provides a
practical framework for choosing the appropriate level of type equivalence
for different use cases—a question that is typically resolved at the
language-design level but here is deferred to the programmer.
