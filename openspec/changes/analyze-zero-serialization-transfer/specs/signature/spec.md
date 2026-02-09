## ADDED Requirements

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

## MODIFIED Requirements

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
