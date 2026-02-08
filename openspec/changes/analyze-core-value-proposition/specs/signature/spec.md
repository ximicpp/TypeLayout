## ADDED Requirements

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
- **AND** when layout signatures match, users MAY skip serialization entirely for zero-copy transfer
