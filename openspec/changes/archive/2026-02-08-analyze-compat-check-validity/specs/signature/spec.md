## ADDED Requirements

### Requirement: Cross-Platform Correctness Boundary
The library SHALL document the precise conditions under which layout signature matching
guarantees binary compatibility for cross-platform data transfer.

#### Scenario: Fixed-width integer POD types
- **GIVEN** a struct containing only fixed-width integer fields (`uint32_t`, `int64_t`, etc.)
- **WHEN** Layout signatures match across two platforms
- **THEN** the type SHALL be safe for zero-copy transfer via `memcpy`

#### Scenario: IEEE 754 floating-point types
- **GIVEN** a struct containing `float` or `double` fields
- **WHEN** Layout signatures match across two platforms that both use IEEE 754
- **THEN** the type SHALL be safe for zero-copy transfer
- **AND** the library SHALL document the IEEE 754 assumption

#### Scenario: Pointer-containing types
- **GIVEN** a struct containing pointer fields (`ptr`, `fnptr`, `memptr`)
- **WHEN** Layout signatures match across two platforms
- **THEN** the memory layout SHALL be compatible
- **BUT** pointer values SHALL NOT be valid across different address spaces
- **AND** the compatibility report SHOULD warn about pointer fields

#### Scenario: Bit-field types
- **GIVEN** a struct containing bit-field members
- **WHEN** Layout signatures are compared across platforms
- **THEN** the library SHALL warn that bit-field ordering is implementation-defined
- **AND** matching signatures do not guarantee identical bit-level layout across different compilers

#### Scenario: Platform-dependent types
- **GIVEN** a struct containing `long`, `wchar_t`, or `long double`
- **WHEN** Layout signatures are compared across platforms with different ABI conventions
- **THEN** signatures SHALL naturally differ reflecting the actual size differences
- **AND** the compatibility report SHALL correctly identify these as layout mismatches

## MODIFIED Requirements

### Requirement: Two-Layer Signature Architecture
The library SHALL provide two complementary layers of compile-time type signatures.

#### Scenario: Layout signature (Layer 1)
- **GIVEN** a type T
- **WHEN** calling `get_layout_signature<T>()`
- **THEN** the result SHALL encode size, alignment, architecture prefix, and all leaf fields at their absolute byte offsets
- **AND** inheritance SHALL be flattened (base class fields appear at absolute offsets)
- **AND** composition SHALL be flattened (nested struct fields are recursively expanded)
- **AND** field names SHALL NOT be included
- **AND** polymorphic types SHALL include a `,vptr` marker
- **AND** union members SHALL NOT be recursively flattened (each member retains its type signature as an atomic unit)

#### Scenario: Definition signature (Layer 2)
- **GIVEN** a type T
- **WHEN** calling `get_definition_signature<T>()`
- **THEN** the result SHALL encode size, alignment, architecture prefix, field names, and type structure
- **AND** inheritance SHALL be preserved as `~base<QualifiedName>:record{...}`
- **AND** virtual bases SHALL be marked as `~vbase<QualifiedName>:record{...}`
- **AND** polymorphic types SHALL include a `,polymorphic` marker
- **AND** base class names SHALL use fully qualified names (namespace::Name)
- **AND** enum types SHALL include their fully qualified name

#### Scenario: Projection relationship
- **GIVEN** two types T and U
- **WHEN** `definition_signatures_match<T,U>()` returns true
- **THEN** `layout_signatures_match<T,U>()` SHALL also return true
- **NOTE** The reverse does not hold: layout match does not imply definition match

#### Scenario: Cross-platform correctness guarantee
- **GIVEN** the same type T compiled on platforms A and B
- **WHEN** `layout_sig(T@A) == layout_sig(T@B)`
- **THEN** `sizeof(T)`, `alignof(T)`, and all field offsets SHALL be identical on both platforms
- **AND** for POD types with only scalar fields (fixed-width integers, IEEE 754 floats, enums, byte arrays), `memcpy` transfer between platforms SHALL be safe
- **NOTE** This guarantee assumes IEEE 754 floating-point representation on both platforms
- **NOTE** Pointer values are not transferable across address spaces even if layout matches
- **NOTE** Bit-field ordering is implementation-defined and may differ across compilers
