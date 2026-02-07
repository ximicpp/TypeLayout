## MODIFIED Requirements

### Requirement: Two-Layer Signature Core
The system SHALL provide exactly two signature functions:
- `get_layout_signature<T>()` — pure byte layout (flattened, no names)
- `get_definition_signature<T>()` — full type definition (tree, with names)

And exactly two match functions:
- `layout_signatures_match<T, U>()` — byte-level comparison
- `definition_signatures_match<T, U>()` — structural comparison

No hash, verification, concept, cstr, or macro APIs SHALL be provided in the minimal core.

#### Scenario: Minimal API surface
- **WHEN** a user includes `<boost/typelayout.hpp>`
- **THEN** only `get_layout_signature`, `get_definition_signature`, `layout_signatures_match`, `definition_signatures_match`, and `LayoutSupported` are available

## REMOVED Requirements

### Requirement: Hash Functions
**Reason**: Non-core utility. Can be reimplemented by users with standard hash algorithms.
**Migration**: Users compute their own hash over `get_layout_signature<T>().c_str()`.

### Requirement: Dual-Hash Verification
**Reason**: Non-core utility built on top of hash functions.
**Migration**: Removed entirely.

### Requirement: Collision Detection
**Reason**: Non-core utility built on top of hash functions.
**Migration**: Removed entirely.

### Requirement: Layout Concepts
**Reason**: Non-core sugar. 4 concepts that wrap match functions.
**Migration**: Users write their own concepts or use match functions directly.

### Requirement: Signature Macros
**Reason**: Non-core sugar (TYPELAYOUT_BIND_LAYOUT, TYPELAYOUT_ASSERT_*, etc.)
**Migration**: Users write their own `static_assert` with match functions.

### Requirement: C-String Accessors
**Reason**: Non-core runtime utility.
**Migration**: Users create `static constexpr auto sig = ...; sig.c_str();` themselves.

### Requirement: Variable Templates
**Reason**: Non-core sugar (`layout_signature_v`, `layout_hash_v`, etc.)
**Migration**: Users call functions directly.