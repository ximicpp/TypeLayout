## MODIFIED Requirements

### Requirement: Serialization Signature Generation
The library SHALL generate serialization **status indicators** (not full layout signatures) that include serializability status.

#### Scenario: Renamed API for clarity
- **GIVEN** the need to check serialization status
- **WHEN** calling the serialization check function
- **THEN** the function SHALL be named `serialization_status<T, P>()`
- **AND** NOT `serialization_signature<T, P>()` (to avoid confusion with layout signature)

#### Scenario: Serializable type status
- **GIVEN** a POD struct `struct Point { int x; int y; };`
- **WHEN** calling `serialization_status<Point, P>()`
- **THEN** the result SHALL be a status string like `"[64-le]serial"`
- **AND** this is a STATUS INDICATOR, not a layout signature

#### Scenario: Non-serializable type status
- **GIVEN** a struct with pointer `struct Node { int val; Node* next; };`
- **WHEN** calling `serialization_status<Node, P>()`
- **THEN** the result SHALL include `!serial:ptr` status marker

#### Scenario: Bit-field type status
- **GIVEN** a struct with bit-field `struct Flags { int a : 4; int b : 4; };`
- **WHEN** calling `serialization_status<Flags, P>()`
- **THEN** the result SHALL include `!serial:bitfield` status marker
- **RATIONALE** Bit-fields have implementation-defined layout

## ADDED Requirements

### Requirement: Bit-field Serialization Blocker
Types with bit-fields SHALL be rejected from serialization compatibility.

#### Scenario: Direct bit-field rejection
- **GIVEN** a struct with direct bit-field member
- **WHEN** checking `is_serializable_v<T, P>`
- **THEN** the result SHALL be `false`
- **AND** `serialization_blocker_v<T, P>` SHALL be `HasBitField`

#### Scenario: Nested bit-field rejection
- **GIVEN** a struct containing a nested struct with bit-fields
- **WHEN** checking `is_serializable_v<T, P>`
- **THEN** the result SHALL be `false`
- **RATIONALE** Bit-field layout is implementation-defined and not portable
