## ADDED Requirements

### Requirement: API Compatibility Check Scenario Analysis

The signature system SHALL provide a formal analysis document explaining the relationship between the API compatibility checking scenario and the two-layer signature architecture, demonstrating how Definition layer provides semantic-level API verification and how the ABI/API distinction maps to the Layout/Definition distinction.

#### Scenario: Definition layer is primary for API compatibility
- **GIVEN** a library exposing public API structures (parameter types, return types)
- **WHEN** the library is upgraded and structures may have changed
- **THEN** the analysis SHALL demonstrate that Definition signature (V2) is the primary API compatibility tool:
  - API compatibility requires field name preservation (users access fields by name)
  - API compatibility requires inheritance hierarchy preservation (users may upcast/downcast)
  - Definition signature encodes both (Theorem 3.5: faithful encoding of D_P including names and bases)
  - Corollary 3.5.1: Definition match guarantees complete structural identity

#### Scenario: ABI compatibility vs API compatibility distinction
- **GIVEN** a library upgrade changes `struct Result { int error_code; ... }` to `struct Result : ErrorBase { ... }` with identical byte layout
- **WHEN** comparing with both layers
- **THEN** the analysis SHALL demonstrate the critical distinction:
  - Layout signatures match → ABI compatible (binary safe to link, same byte layout)
  - Definition signatures differ → API incompatible (inheritance changed, client code using `result.error_code` may need updating)
  - This maps precisely to the two-layer design: V1 = ABI compatibility, V2 = API compatibility
  - Formal basis: Theorem 4.3 — ker(⟦·⟧_D) ⊊ ker(⟦·⟧_L), strict refinement

#### Scenario: Structural analysis design choice implications
- **GIVEN** TypeLayout performs structural analysis (not nominal analysis)
- **WHEN** two different-named types have identical structure (e.g., `struct Point` and `struct Coord` with same fields)
- **THEN** the analysis SHALL explain the implications for API checking:
  - Definition signatures will match for structurally identical types even with different names
  - This is an intentional design choice: TypeLayout answers "are these structures equivalent?" not "are these the same type?"
  - For nominal identity checking, users should combine TypeLayout with type name comparison
  - This limitation is acknowledged in project.md and is a conscious trade-off for structural analysis
