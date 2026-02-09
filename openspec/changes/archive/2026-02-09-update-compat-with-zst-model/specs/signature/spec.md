## MODIFIED Requirements

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

### Requirement: Competitive Positioning
The library SHALL document its differentiated positioning relative to alternative approaches.

#### Scenario: Complementary to serialization frameworks
- **GIVEN** serialization frameworks (protobuf, FlatBuffers) generate serialization code
- **THEN** the library SHALL document that TypeLayout is complementary, not competitive
- **AND** TypeLayout's role is to determine "whether serialization is needed" while serialization frameworks determine "how to serialize"
- **AND** when layout signatures match AND safety classification is Safe (C1 ∧ C2), users MAY skip serialization entirely for zero-copy transfer
- **AND** the decision criterion SHALL be documented as: C1 ∧ C2 → zero-copy; otherwise → serialize
