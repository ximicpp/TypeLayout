## MODIFIED Requirements

### Requirement: Header Organization
The library SHALL organize headers following Boost conventions:
- Public API headers (`signature.hpp`, `opaque.hpp`, `fixed_string.hpp`)
  SHALL reside at the library root (`boost/typelayout/`).
- Implementation-only headers SHALL reside in `detail/`.
- Platform configuration macros SHALL reside in `config.hpp`.
- The `FixedString<N>` type SHALL reside in its own header with no P2996
  dependency.
- The signature engine, reflection helpers, and type specializations SHALL
  be in separate `detail/` headers.
- Old `core/` headers SHALL remain as backward-compatibility shims.

#### Scenario: FixedString in dedicated header
- **WHEN** a user includes `boost/typelayout/fixed_string.hpp`
- **THEN** `FixedString<N>` and `to_fixed_string()` SHALL be available
- **AND** no P2996 reflection headers SHALL be included

#### Scenario: Backward-compatible core includes
- **WHEN** a user includes `boost/typelayout/core/fwd.hpp`
- **THEN** `FixedString<N>` SHALL still be available via transitive include
- **AND** existing code SHALL compile without modification

#### Scenario: Signature engine modularity
- **WHEN** a contributor modifies a TypeSignature specialization
- **THEN** the change SHALL be isolated to `detail/type_map.hpp`
- **AND** the Layout/Definition engine code in `detail/signature_impl.hpp` SHALL not need editing

#### Scenario: Config isolation
- **WHEN** a user includes `boost/typelayout/config.hpp`
- **THEN** only platform detection macros (e.g., `TYPELAYOUT_LITTLE_ENDIAN`) SHALL be defined
- **AND** no types or functions SHALL be declared

#### Scenario: Custom export workflow
- **WHEN** a user calls `TYPELAYOUT_REGISTER_TYPES(exporter, Type1, Type2)`
- **THEN** the types SHALL be registered on the provided `SigExporter` instance
- **AND** no `main()` function SHALL be generated

#### Scenario: Detail header include chain
- **WHEN** a user includes `boost/typelayout/signature.hpp`
- **THEN** all detail headers SHALL be transitively included in the correct order
- **AND** the compilation SHALL succeed without additional includes