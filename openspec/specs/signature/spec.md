# signature Specification

> **Layer**: Core (`<boost/typelayout.hpp>`)

## Purpose

Defines the two-layer signature generation system — the core capability of Boost.TypeLayout.
Signatures provide deterministic, human-readable descriptions of type memory layouts
for compile-time type identity verification.

## Core Values

| # | Value | Formal Expression | Description |
|---|-------|-------------------|-------------|
| V1 | Layout signature **reliability** | `layout_sig(T) == layout_sig(U) ⟹ memcmp-compatible(T, U)` | Same signature → same byte layout (conservative: reverse does not hold) |
| V2 | Definition signature **precision** | `def_sig(T) == def_sig(U) ⟹ identical field names, types, and hierarchy` | Distinguishes all structural differences |
| V3 | Two-layer **projection** | `def_match(T, U) ⟹ layout_match(T, U)` | Definition is a refinement of Layout; reverse does not hold |

## Design Philosophy

TypeLayout performs **Structural Analysis**, not Nominal Analysis.
Two differently-named types (`struct Point`, `struct Coord`) with identical field names, types, and layout
will have identical Definition signatures. The signature **does not include the type's own name** — this is
an intentional design choice because TypeLayout answers "are two types structurally equivalent?", not "are they
the same type?".

## Usage Guidance

| Use Case | Recommended Layer | Rationale |
|----------|-------------------|-----------|
| Shared memory / IPC | Layout | Only byte-layout compatibility matters |
| Network protocol verification | Layout | Only byte alignment and offsets matter |
| Compiler ABI verification | Layout | Binary compatibility check |
| Serialization version check | Definition | Detects field renames and structural changes |
| API compatibility check | Definition | Semantic-level structural consistency |
| ODR violation detection | Definition | Requires full structural information |
## Requirements
### Requirement: Two-Layer Signature Architecture
The library SHALL provide two complementary layers of compile-time type signatures.

#### Scenario: Cross-platform correctness guarantee
- **GIVEN** the same type T compiled on platforms A and B
- **WHEN** `layout_sig(T@A) == layout_sig(T@B)`
- **THEN** `sizeof(T)`, `alignof(T)`, and all field offsets SHALL be identical on both platforms
- **AND** for types classified as Safe by `classify_safety()` (no pointer fields, no bit-fields, no platform-dependent types), `memcpy` transfer between platforms SHALL be safe under the IEEE 754 assumption
- **AND** for types classified as Warning (contains pointers or vptr), the memory layout SHALL be compatible but pointer values SHALL NOT be valid across address spaces
- **AND** for types classified as Risk (contains bit-fields), the layout signature match SHALL NOT guarantee identical bit-level semantics across different compilers
- **NOTE** The zero-serialization transfer condition is: Layout Signature Match (C1) AND Safety Classification = Safe (C2), under the IEEE 754 axiom (A1)
- **NOTE** C1 already subsumes endianness and pointer width verification because the architecture prefix is part of the signature string
- **NOTE** Pointer values are not transferable across address spaces even if layout matches
- **NOTE** Bit-field ordering is implementation-defined and may differ across compilers

### Requirement: Signature Comparison
The library SHALL provide compile-time signature comparison functions.

#### Scenario: Layout comparison
- **GIVEN** two types T and U
- **WHEN** calling `layout_signatures_match<T, U>()`
- **THEN** the result SHALL be true if and only if their Layout signatures are identical

#### Scenario: Definition comparison
- **GIVEN** two types T and U
- **WHEN** calling `definition_signatures_match<T, U>()`
- **THEN** the result SHALL be true if and only if their Definition signatures are identical

### Requirement: Layout Signature Flattening
The Layout layer SHALL flatten all structural hierarchy to pure byte identity.

#### Scenario: Inheritance flattening
- **GIVEN** `struct Base { int32_t x; }; struct Derived : Base { double y; };`
- **AND** `struct Flat { int32_t x; double y; };`
- **WHEN** Layout signatures are compared
- **THEN** `layout_signatures_match<Derived, Flat>()` SHALL return true

#### Scenario: Multi-level inheritance flattening
- **GIVEN** `A { int x; }; B : A { int y; }; C : B { int z; };`
- **AND** `Flat { int x; int y; int z; };`
- **WHEN** Layout signatures are compared
- **THEN** `layout_signatures_match<C, Flat>()` SHALL return true

#### Scenario: Composition flattening
- **GIVEN** `struct Inner { int a; int b; }; struct Composed { Inner x; };`
- **AND** `struct Flat { int a; int b; };`
- **WHEN** Layout signatures are compared
- **THEN** `layout_signatures_match<Composed, Flat>()` SHALL return true

#### Scenario: Virtual base inclusion
- **GIVEN** a type with virtual base classes
- **WHEN** Layout signature is generated
- **THEN** virtual base class fields SHALL be included at their correct offsets

#### Scenario: Union members not flattened
- **GIVEN** `struct Inner { int a; int b; }; union U { Inner x; double y; };`
- **WHEN** Layout signature is generated
- **THEN** union member `x` SHALL appear as its complete type signature (e.g., `record[s:8,a:4]{...}`) rather than being recursively flattened into individual fields

### Requirement: Polymorphic Type Safety
The library SHALL distinguish polymorphic from non-polymorphic types in Layout signatures.

#### Scenario: vptr marker in Layout
- **GIVEN** a polymorphic type `struct P { virtual void f(); int x; };`
- **WHEN** Layout signature is generated
- **THEN** the signature SHALL contain `,vptr` marker
- **AND** the signature SHALL NOT match a non-polymorphic type with the same fields

### Requirement: Qualified Names in Definition
The Definition layer SHALL use fully qualified names for disambiguation.
Qualified names SHALL include the complete namespace path from the global namespace,
regardless of nesting depth.

#### Scenario: Base class qualified names
- **GIVEN** `namespace ns1 { struct Tag { int id; }; }` and `namespace ns2 { struct Tag { int id; }; }`
- **AND** `struct A : ns1::Tag {}; struct B : ns2::Tag {};`
- **WHEN** Definition signatures are compared
- **THEN** `definition_signatures_match<A, B>()` SHALL return false

#### Scenario: Enum qualified names
- **GIVEN** `namespace ns { enum class Color : uint8_t {}; enum class Shape : uint8_t {}; }`
- **WHEN** Definition signatures are compared
- **THEN** `definition_signatures_match<Color, Shape>()` SHALL return false
- **AND** `layout_signatures_match<Color, Shape>()` SHALL return true

#### Scenario: Deep namespace qualified names
- **GIVEN** `namespace a { namespace b { namespace c { struct T { int x; }; } } }`
- **AND** `namespace d { namespace b { namespace c { struct T { int x; }; } } }`
- **WHEN** Definition signatures are compared
- **THEN** the qualified name for the first SHALL contain `a::b::c::T`
- **AND** the qualified name for the second SHALL contain `d::b::c::T`
- **AND** `definition_signatures_match` between types using these as bases SHALL return false

### Requirement: Type Categories
The library SHALL support the following type categories in signatures.

#### Scenario: Fundamental types
- **GIVEN** fixed-width integers, floats, chars, bool
- **WHEN** signature is generated
- **THEN** each SHALL produce a deterministic signature with `[s:SIZE,a:ALIGN]`

#### Scenario: Record types (struct/class)
- **GIVEN** a struct or class type
- **WHEN** signature is generated
- **THEN** the prefix SHALL be `record`

#### Scenario: Enum types
- **GIVEN** an enum type with underlying type U
- **WHEN** Layout signature is generated
- **THEN** the format SHALL be `enum[s:SIZE,a:ALIGN]<underlying_sig>`
- **WHEN** Definition signature is generated
- **THEN** the format SHALL be `enum<QualifiedName>[s:SIZE,a:ALIGN]<underlying_sig>`

#### Scenario: Union types
- **GIVEN** a union type
- **WHEN** signature is generated
- **THEN** the prefix SHALL be `union` with all members at their offsets

#### Scenario: Array types
- **GIVEN** a fixed-size array `T[N]`
- **WHEN** signature is generated
- **THEN** the format SHALL be `array[s:SIZE,a:ALIGN]<element_sig,N>`
- **AND** byte arrays (char[], uint8_t[], std::byte[]) SHALL normalize to `bytes[s:N,a:1]`

#### Scenario: Bit-field types
- **GIVEN** a struct with bit-fields
- **WHEN** signature is generated
- **THEN** the format SHALL be `@BYTE.BIT:bits<WIDTH,underlying_sig>` (Layout)
- **OR** `@BYTE.BIT[NAME]:bits<WIDTH,underlying_sig>` (Definition)

#### Scenario: Anonymous members
- **GIVEN** a struct with anonymous members
- **WHEN** Definition signature is generated
- **THEN** anonymous members SHALL use `<anon:N>` placeholder

#### Scenario: Function pointer types
- **GIVEN** any function pointer type including noexcept and variadic variants
- **WHEN** signature is generated
- **THEN** all variants SHALL produce `fnptr[s:SIZE,a:ALIGN]`

### Requirement: Architecture Prefix
Every signature SHALL include an architecture prefix.

#### Scenario: 64-bit little-endian
- **WHEN** compiling on a 64-bit little-endian platform
- **THEN** signatures SHALL be prefixed with `[64-le]`

#### Scenario: Platform-dependent types
- **GIVEN** `long` or `wchar_t` types
- **WHEN** signature is generated
- **THEN** the signature SHALL reflect the actual size on the current platform

### Requirement: Empty Type Handling
The library SHALL correctly handle empty types.

#### Scenario: Empty base optimization
- **GIVEN** `struct Empty {}; struct WithEmpty : Empty { int x; double y; };`
- **AND** `struct Plain { int x; double y; };`
- **WHEN** Layout signatures are compared
- **THEN** `layout_signatures_match<WithEmpty, Plain>()` SHALL return true

### Requirement: Alignment Support
The library SHALL capture alignment information in signatures.

#### Scenario: Custom alignment
- **GIVEN** `struct alignas(16) Aligned { int a; int b; };`
- **WHEN** signature is generated
- **THEN** the signature SHALL reflect `a:16` alignment

### Requirement: Cross-Platform Correctness Boundary
The library SHALL document the precise conditions under which layout signature matching
guarantees binary compatibility for cross-platform data transfer.

#### Scenario: Fixed-width integer POD types
- **GIVEN** a struct containing only fixed-width integer fields (`uint32_t`, `int64_t`, etc.)
- **WHEN** Layout signatures match across two platforms
- **THEN** the type SHALL be safe for zero-copy transfer via `memcpy`
- **AND** the type SHALL be classified as "Safe" in the zero-serialization taxonomy

#### Scenario: IEEE 754 floating-point types
- **GIVEN** a struct containing `float` or `double` fields
- **WHEN** Layout signatures match across two platforms that both use IEEE 754
- **THEN** the type SHALL be safe for zero-copy transfer
- **AND** the library SHALL document the IEEE 754 assumption
- **AND** the type SHALL be classified as "Safe" in the zero-serialization taxonomy (assuming IEEE 754)

#### Scenario: Pointer-containing types
- **GIVEN** a struct containing pointer fields (`ptr`, `fnptr`, `memptr`)
- **WHEN** Layout signatures match across two platforms
- **THEN** the memory layout SHALL be compatible
- **BUT** pointer values SHALL NOT be valid across different address spaces
- **AND** the compatibility report SHOULD warn about pointer fields
- **AND** the type SHALL be classified as "Conditional" in the zero-serialization taxonomy

#### Scenario: Bit-field types
- **GIVEN** a struct containing bit-field members
- **WHEN** Layout signatures are compared across platforms
- **THEN** the library SHALL warn that bit-field ordering is implementation-defined
- **AND** matching signatures do not guarantee identical bit-level layout across different compilers
- **AND** the type SHALL be classified as "Unsafe" in the zero-serialization taxonomy

#### Scenario: Platform-dependent types
- **GIVEN** a struct containing `long`, `wchar_t`, or `long double`
- **WHEN** Layout signatures are compared across platforms with different ABI conventions
- **THEN** signatures SHALL naturally differ reflecting the actual size differences
- **AND** the compatibility report SHALL correctly identify these as layout mismatches
- **AND** the type SHALL be classified as "Conditional" in the zero-serialization taxonomy

### Requirement: Core Value Correctness Boundary
The library SHALL document the correctness boundary and assumptions for each core value (V1/V2/V3).

#### Scenario: V1 completeness assumptions
- **GIVEN** the V1 guarantee `layout_sig(T) == layout_sig(U) ⟹ memcmp-compatible(T, U)`
- **THEN** the library SHALL document the following assumptions:
  - IEEE 754 floating-point representation on both platforms
  - Same endianness (encoded in architecture prefix)
  - Same pointer width (encoded in architecture prefix)
- **AND** the library SHALL document that padding byte content is undefined and memcmp may not return 0 even for equivalent values

#### Scenario: V1 conservativeness guarantee
- **GIVEN** the V1 guarantee is a one-way implication (⟹ not ⟺)
- **THEN** the library SHALL document that this is an intentional safety property
- **AND** false positives (signature match but layout mismatch) SHALL be impossible under stated assumptions
- **AND** false negatives (layout match but signature mismatch) are acceptable and documented (e.g., `int[3]` vs `int,int,int`)

#### Scenario: V2 discrimination boundary
- **GIVEN** the V2 guarantee `def_sig(T) == def_sig(U) ⟹ identical field names, types, and hierarchy`
- **THEN** the library SHALL document that V2 does NOT distinguish:
  - Types with different names but identical structure (by design: structural analysis)
  - cv-qualifiers (`const int` vs `int`)
- **AND** V2 SHALL distinguish all structural differences including field names, inheritance, namespaces, and enum qualified names

#### Scenario: V3 algebraic correctness
- **GIVEN** the V3 guarantee `def_match(T, U) ⟹ layout_match(T, U)`
- **THEN** the library SHALL document that this is an algebraic consequence of Definition being a refinement of Layout
- **AND** the reverse SHALL NOT hold (layout match does not imply definition match)

### Requirement: Competitive Positioning
The library SHALL document its differentiated positioning relative to alternative approaches.

#### Scenario: Complementary to serialization frameworks
- **GIVEN** serialization frameworks (protobuf, FlatBuffers) generate serialization code
- **THEN** the library SHALL document that TypeLayout is complementary, not competitive
- **AND** TypeLayout's role is to determine "whether serialization is needed" while serialization frameworks determine "how to serialize"
- **AND** when layout signatures match AND safety classification is Safe (C1 ∧ C2), users MAY skip serialization entirely for zero-copy transfer
- **AND** the decision criterion SHALL be documented as: C1 ∧ C2 → zero-copy; otherwise → serialize

### Requirement: Architecture Decision Documentation
The library SHALL document the rationale for key architectural decisions.

#### Scenario: Two-layer justification
- **GIVEN** the library provides two signature layers (Layout and Definition)
- **THEN** the documentation SHALL explain why a single layer is insufficient
- **AND** the documentation SHALL explain why a third layer (semantic) is unnecessary
- **AND** the documentation SHALL describe Layout as answering "can I memcpy?" and Definition as answering "is the structure identical?"

#### Scenario: Two-phase pipeline justification
- **GIVEN** the library separates signature export (Phase 1) from compatibility check (Phase 2)
- **THEN** the documentation SHALL explain that Phase 1 requires P2996 while Phase 2 requires only C++17
- **AND** the documentation SHALL explain the persistent value of signature files (.sig.hpp) for version control and auditing
- **AND** the documentation SHALL note that the two-phase design retains value even after P2996 standardization

#### Scenario: FixedString performance boundary
- **GIVEN** the library uses FixedString<N> for compile-time string manipulation
- **THEN** the documentation SHALL document the performance boundary (~100 fields with default constexpr step limits)
- **AND** the documentation SHALL provide compiler flag recommendations for larger types

### Requirement: Formal Accuracy Guarantees
The library SHALL provide formally proven accuracy guarantees for both signature layers, using a rigorous framework based on denotational semantics and refinement theory.

#### Scenario: Layout signature soundness (no false positives)
- **GIVEN** two types T and U on the same platform P
- **WHEN** `layout_signatures_match<T, U>()` returns true
- **THEN** the layout denotation ⟦T⟧_L SHALL equal ⟦U⟧_L
- **AND** by the Encoding Faithfulness theorem (decode ∘ ⟦·⟧_L = L_P), L_P(T) SHALL equal L_P(U)
- **AND** the types SHALL be memcmp-compatible (T ≅_mem U)

#### Scenario: Layout signature conservativeness (intentional false negatives)
- **GIVEN** two types T and U that have identical byte layout (e.g., `int32_t[3]` and `struct{int32_t x,y,z;}`)
- **WHEN** their signatures are compared
- **THEN** the signatures MAY differ despite identical byte layout
- **AND** ker(⟦·⟧_L) ⊂ ≅_byte (the equivalence kernel is strictly smaller than byte identity)

#### Scenario: Compiler-verified offset correctness
- **GIVEN** any struct or class type T with nesting depth d
- **WHEN** Layout or Definition signature is generated
- **THEN** all field offsets SHALL be proven correct by structural induction on d
- **AND** the base case (d=0) uses P2996 `offset_of` directly
- **AND** the inductive step accumulates OffsetAdj correctly

#### Scenario: Definition-to-Layout projection correctness
- **GIVEN** two types T and U
- **WHEN** `definition_signatures_match<T, U>()` returns true
- **THEN** there SHALL exist a well-defined erasure function π with four deterministic steps
- **AND** π(⟦T⟧_D) = ⟦T⟧_L SHALL hold for all T (Lemma 4.1.2)
- **AND** ker(⟦·⟧_D) ⊊ ker(⟦·⟧_L) (strict refinement)

#### Scenario: Per-category accuracy classification
- **GIVEN** the library supports multiple type categories
- **THEN** correctness SHALL be proven by structural induction on the type AST grammar
- **AND** the grammar SHALL define 8 constructors: Primitive, Record, Record+bases, PolyRecord, Array, Union, Enum, BitField

#### Scenario: Signature grammar unambiguity
- **GIVEN** the signature string follows a deterministic grammar
- **THEN** each valid signature string SHALL have exactly one parse tree
- **AND** a decode function SHALL exist as the inverse of the encoding

#### Scenario: CV-qualification erasure
- **GIVEN** a type T with const or volatile qualifiers
- **WHEN** a signature is generated
- **THEN** ⟦const T⟧ = ⟦volatile T⟧ = ⟦const volatile T⟧ = ⟦T⟧

#### Scenario: Formal methodology
- **GIVEN** the proof system for signature correctness
- **THEN** it SHALL be based on denotational semantics (signatures as denotations of types)
- **AND** it SHALL use refinement theory for the V3 projection relationship
- **AND** it SHALL use structural induction for per-category verification
- **AND** it SHALL reference established formal verification methodologies (CompCert semantic preservation, seL4 refinement)

### Requirement: Cross-Platform Collection Compatibility Analysis
The project SHALL provide a formal analysis document demonstrating how the two-layer
signature system enables batch cross-platform compatibility verification for type collections.

#### Scenario: Collection-level compatibility matrix
- **GIVEN** a set of N types exported on M platforms via SigExporter
- **WHEN** the analysis evaluates all N×M signatures
- **THEN** the document SHALL present a per-type compatibility matrix showing Layout and Definition match status across all platform pairs
- **AND** the document SHALL classify each type into a safety level based on signature pattern analysis

#### Scenario: Formal reasoning from signatures to safety
- **GIVEN** real .sig.hpp data from Linux x86_64, Windows x86_64, and macOS ARM64
- **WHEN** the analysis applies the V1/V2/V3 theorems
- **THEN** each compatibility judgment SHALL be traceable to a specific theorem (Thm 3.1, 3.2, 4.2, 4.3)
- **AND** the root cause of each incompatibility SHALL be identified (data model, ABI, architecture)

#### Scenario: Collection-level soundness theorem
- **GIVEN** a type collection C = {T₁, ..., Tₙ}
- **WHEN** all types in C have matching Layout signatures across platforms A and B
- **THEN** the document SHALL prove that the entire collection is zero-copy transferable between A and B
- **AND** the proof SHALL extend the per-type V1 guarantee to the collection level

### Requirement: Zero-Serialization Transfer Conditions Analysis
The signature system SHALL provide a formal analysis document defining the necessary and sufficient conditions for zero-serialization (zero-copy) binary data transfer across platforms, based on the two-layer signature architecture and formal verification theory.

#### Scenario: Formal definition of zero-serialization transfer
- **GIVEN** a type T and two platforms P₁ and P₂
- **WHEN** determining whether T can be transmitted via raw `memcpy`/`send`/`recv` without serialization
- **THEN** the analysis SHALL define the zero-serialization predicate ZST(T, P₁, P₂) as the conjunction of:
  - Layout signature match: `sig_layout(T, P₁) == sig_layout(T, P₂)`
  - IEEE 754 compliance on both platforms (for float/double fields)
  - No pointer-valued fields requiring cross-address-space validity
  - No bit-fields with implementation-defined ordering that differs across compilers
- **AND** the analysis SHALL prove that these conditions are jointly sufficient for binary-safe transfer

#### Scenario: Type taxonomy for zero-serialization eligibility
- **GIVEN** the full space of C++ types
- **WHEN** classifying types by zero-serialization eligibility
- **THEN** the analysis SHALL define three categories:
  - **Safe**: Types composed exclusively of fixed-width integers, IEEE 754 floats, enums with fixed underlying type, and byte arrays — these satisfy ZST on all conforming platforms without modification
  - **Conditional**: Types containing pointers, vtables, or platform-dependent scalars (`long`, `wchar_t`, `long double`) — these MAY satisfy ZST on specific platform pairs but not universally
  - **Unsafe**: Types containing bit-fields, flexible array members, or types with compiler-specific padding behavior — these require structural redesign before zero-serialization is viable
- **AND** the classification SHALL be formally grounded in the signature encoding grammar

#### Scenario: Network transmission safety chain
- **GIVEN** a collection of types used in a network protocol
- **WHEN** determining whether the entire collection can use zero-copy `send()/recv()`
- **THEN** the analysis SHALL define the complete safety chain:
  - Step 1: Layout signature match across all communicating platforms (V1 guarantee)
  - Step 2: Same endianness (verified by architecture prefix comparison)
  - Step 3: No semantic pointer fields (verified by Safety Classification)
  - Step 4: Packed/aligned correctly for wire-format (no hidden padding dependencies)
- **AND** the analysis SHALL demonstrate that all four steps are machine-verifiable using TypeLayout's existing toolchain

#### Scenario: Decision tree for serialization vs zero-copy
- **GIVEN** a user deciding between zero-copy transfer and serialization frameworks
- **WHEN** consulting the analysis
- **THEN** the analysis SHALL provide a decision tree mapping:
  - Layout MATCH + Safe → zero-copy recommended
  - Layout MATCH + Conditional → zero-copy possible with documented caveats
  - Layout DIFFER or Unsafe → serialization framework required
- **AND** the analysis SHALL position TypeLayout as the diagnostic tool that answers "do I need to serialize?" before the user chooses a serialization framework

### Requirement: Opaque Type Signature Registration
The library SHALL provide macro-based registration for types whose internal layout is opaque (not introspectable via P2996 reflection), generating fixed-size/alignment signatures without requiring member-level reflection.

#### Scenario: Non-template opaque type
- **WHEN** a user registers a non-template type via `TYPELAYOUT_OPAQUE_TYPE(Type, name, size, align)`
- **THEN** the generated signature SHALL be `name[s:size,a:align]`
- **AND** the signature SHALL be identical for both Layout and Definition modes

#### Scenario: Single-parameter template container
- **WHEN** a user registers a single-type-parameter template via `TYPELAYOUT_OPAQUE_CONTAINER(Template, name, size, align)`
- **THEN** the generated signature SHALL be `name[s:size,a:align]<element_signature>`
- **AND** the element signature SHALL be recursively computed via `TypeSignature<T, Mode>::calculate()`

#### Scenario: Two-parameter template map
- **WHEN** a user registers a two-type-parameter template via `TYPELAYOUT_OPAQUE_MAP(Template, name, size, align)`
- **THEN** the generated signature SHALL be `name[s:size,a:align]<key_signature,value_signature>`
- **AND** both key and value signatures SHALL be recursively computed

### Requirement: Fixed Enum Detection
The library SHALL provide a compile-time function `is_fixed_enum<T>()` that determines whether an enum type has a fixed underlying type suitable for cross-process binary transfer.

#### Scenario: Scoped enum detection
- **GIVEN** a scoped enum (`enum class E : uint32_t { ... }`)
- **WHEN** `is_fixed_enum<E>()` is evaluated
- **THEN** the result SHALL be `true`

#### Scenario: Unscoped enum with explicit underlying type
- **GIVEN** an unscoped enum with explicit type (`enum E : int { ... }`)
- **WHEN** `is_fixed_enum<E>()` is evaluated
- **THEN** the result SHALL be `true`

#### Scenario: Non-enum type rejection
- **GIVEN** a non-enum type T
- **WHEN** `is_fixed_enum<T>()` is instantiated
- **THEN** compilation SHALL fail with a static_assert message

