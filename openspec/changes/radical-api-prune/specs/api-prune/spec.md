## REMOVED Symbols

### Requirement: Delete layout_signatures_match
`layout_signatures_match<T,U>()` SHALL be removed from `signature.hpp`.
Users SHALL use `get_layout_signature<T>() == get_layout_signature<U>()` instead.

#### Scenario: layout_signatures_match no longer compiles
- **WHEN** user code calls `layout_signatures_match<A,B>()`
- **THEN** compilation fails (symbol not found)

#### Scenario: Direct signature comparison works
- **WHEN** user code writes `get_layout_signature<A>() == get_layout_signature<B>()`
- **THEN** the expression compiles and returns the correct boolean

### Requirement: Delete SignatureRegistry
The `SignatureRegistry` class SHALL be removed from `tools/transfer.hpp`.
Only the free function `is_transfer_safe<T>(sig)` SHALL remain in that header.

#### Scenario: SignatureRegistry no longer compiles
- **WHEN** user code instantiates `SignatureRegistry`
- **THEN** compilation fails (symbol not found)

#### Scenario: is_transfer_safe free function still works
- **WHEN** user code calls `is_transfer_safe<T>(remote_sig)`
- **THEN** it compiles and returns the correct result

## INTERNALIZED Symbols

### Requirement: layout_traits moves to detail
`layout_traits<T>` SHALL reside in `boost::typelayout::detail` namespace.
It SHALL NOT be included by the umbrella header `typelayout.hpp`.
Internal code (admission, sig_export) continues to use it via `detail::layout_traits`.

#### Scenario: layout_traits not in public namespace
- **WHEN** user includes only `typelayout.hpp`
- **THEN** `boost::typelayout::layout_traits<T>` is not available

#### Scenario: Internal tests use detail::layout_traits
- **WHEN** a test includes `layout_traits.hpp` directly
- **THEN** `detail::layout_traits<T>` is accessible and functional

### Requirement: SafetyLevel moves to detail
`SafetyLevel` enum, `classify_signature()`, and `safety_level_name()` SHALL
reside in `boost::typelayout::detail` namespace (not `compat::`).
`CompatReporter` uses them internally.

#### Scenario: SafetyLevel not in compat namespace
- **WHEN** user code references `boost::typelayout::compat::SafetyLevel`
- **THEN** compilation fails

#### Scenario: CompatReporter report still contains safety labels
- **WHEN** `CompatReporter::generate_report()` is called
- **THEN** the report output includes safety tier labels (using detail::SafetyLevel internally)

### Requirement: platform_detect moves to detail
Functions and types in `platform_detect.hpp` SHALL reside in `detail` namespace.
Macros (preprocessor) remain unchanged (macros have no namespace).

#### Scenario: SigExporter auto-detection still works
- **WHEN** `SigExporter` is constructed without an explicit platform name
- **THEN** it auto-detects the platform using `detail::` platform detection

## RETAINED Symbols

### Requirement: Public API is exactly 6 symbols
The public API SHALL consist of exactly these 6 symbols:
1. `get_layout_signature<T>()` in `boost::typelayout`
2. `is_byte_copy_safe_v<T>` in `boost::typelayout`
3. `is_transfer_safe<T>(sig)` in `boost::typelayout`
4. `TYPELAYOUT_REGISTER_OPAQUE` macro (and variant macros)
5. `SigExporter` class in `boost::typelayout`
6. `CompatReporter` class in `boost::typelayout` (or `compat::`)

#### Scenario: All 6 symbols accessible via their respective headers
- **WHEN** user includes the appropriate headers
- **THEN** all 6 symbols compile and function correctly

#### Scenario: No other symbols in public namespace
- **WHEN** grepping for non-detail, non-infrastructure symbols in public namespace
- **THEN** only the 6 listed symbols (plus `FixedString` return type) are found
