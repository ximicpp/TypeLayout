## MODIFIED Requirements
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
