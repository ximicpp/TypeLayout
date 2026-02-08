## ADDED Requirements

### Requirement: Example Coverage Principle
The documentation SHALL maintain examples that cover all core value dimensions (V1/V2/V3).

#### Scenario: Minimum coverage per core value
- **GIVEN** the library has three core values V1 (Layout reliability), V2 (Definition precision), V3 (Projection)
- **THEN** the example suite SHALL include at least one type that demonstrates each value
- **AND** each value SHALL have both a "positive case" (value confirmed) and a "boundary case" (value limitation)

#### Scenario: V2 demonstration requirement
- **GIVEN** V2 distinguishes structural differences invisible to Layout
- **THEN** the examples SHALL include at least one pair of types where Layout matches but Definition differs
- **AND** the documentation SHALL explain why Definition is needed for that scenario

#### Scenario: Safety classification coverage
- **GIVEN** the safety classification system (Safe/Warning/Risk)
- **THEN** the examples SHALL include at least one type at each safety level
- **AND** the documentation SHALL explain why each type received its rating
