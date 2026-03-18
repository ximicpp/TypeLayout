## ADDED Requirements

### Requirement: get_arch_prefix in detail namespace
`get_arch_prefix()` SHALL reside in `boost::typelayout::detail` namespace,
not in the public `boost::typelayout` namespace. It is an implementation
detail used only by `get_layout_signature<T>()` and `SigExporter`.

#### Scenario: get_arch_prefix not accessible from public namespace
- **WHEN** user code calls `boost::typelayout::get_arch_prefix()`
- **THEN** compilation fails (symbol not found in public namespace)

#### Scenario: Internal usage via detail namespace
- **WHEN** `get_layout_signature<T>()` or `SigExporter` calls `detail::get_arch_prefix()`
- **THEN** the call succeeds and returns the correct arch prefix string

### Requirement: API documentation tiers
The API reference documentation SHALL organize all public symbols into 4 tiers:

1. **Core** (4 concepts): `get_layout_signature<T>()`, `is_byte_copy_safe_v<T>`,
   `is_transfer_safe<T>(sig)`, `TYPELAYOUT_REGISTER_OPAQUE` macros.
2. **Convenience**: `layout_signatures_match<T,U>()`.
3. **Diagnostic**: `layout_traits<T>`.
4. **Tools** (separate include): `SignatureRegistry`, `SigExporter`, `CompatReporter`,
   `compat::SafetyLevel`, `platform_detect`.

#### Scenario: api-reference.md tier ordering
- **WHEN** a user reads `docs/api-reference.md`
- **THEN** the Core tier appears first, followed by Convenience, Diagnostic, then Tools

#### Scenario: quickstart leads with core concepts
- **WHEN** a user reads `docs/quickstart.md`
- **THEN** the first code example uses only Core tier APIs

### Requirement: AGENTS.md tier documentation
`AGENTS.md` and `.codemaker/rules/rules.mdc` SHALL include an "API Tiers"
section in the Code Map that lists all 4 tiers with their symbols.

#### Scenario: Code Map includes API Tiers
- **WHEN** an AI agent reads `AGENTS.md`
- **THEN** it finds an "API Tiers" table mapping each public symbol to its tier
