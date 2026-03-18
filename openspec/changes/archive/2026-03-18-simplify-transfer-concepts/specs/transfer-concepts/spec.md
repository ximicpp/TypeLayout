## REMOVED Requirements

### Requirement: is_local_serialization_free predicate
The library SHALL NO LONGER provide `is_local_serialization_free<T>` or
`is_local_serialization_free_v<T>`. Users requiring this check SHALL use
`std::is_trivially_copyable_v<T> && is_byte_copy_safe_v<T>`.

### Requirement: serialization_free_assert diagnostic
The library SHALL NO LONGER provide `serialization_free_assert<T>`. Users requiring
compile-time admission checks SHALL use `static_assert(is_byte_copy_safe_v<T>)`.

### Requirement: is_trivial_safe_v alias
The library SHALL NO LONGER provide `is_trivial_safe_v<T>`. Users requiring this check
SHALL use `classify_v<T> == SafetyLevel::TrivialSafe` directly.

### Requirement: is_layout_compatible_v alias (naming collision)
The library SHALL NO LONGER provide `is_layout_compatible_v<T>`. This name collides with
`std::is_layout_compatible<T,U>` (C++20) which has completely different semantics.

### Requirement: is_memcpy_safe_v deprecated alias
The library SHALL NO LONGER provide `is_memcpy_safe_v<T>` (already marked deprecated).

### Requirement: signature_compare duplicate
The library SHALL NO LONGER provide `signature_compare<T,U>` or `signature_compare_v<T,U>`.
Users SHALL use `layout_signatures_match<T1,T2>()` instead.

#### Scenario: signature_compare does not exist
- **WHEN** all public header files are searched for `signature_compare` as a class/struct name
- **THEN** zero results are found
- **AND** `layout_signatures_match<T1,T2>()` continues to work as the canonical comparison API

## MODIFIED Requirements

### Requirement: Unified "transfer-safe" terminology
All public API symbols, method names, file names, and documentation SHALL use the term
"transfer-safe" (or `transfer_safe` / `is_transfer_safe` / `are_transfer_safe`) instead
of "serialization-free" (or `serialization_free` / `is_serialization_free` / `are_serialization_free`).

#### Scenario: No "serialization_free" in public API
- **WHEN** all public header files are searched for "serialization_free" as an identifier
- **THEN** zero results are found in function names, class names, or variable names
- **NOTE** Comments may still reference the old term for migration guidance, but identifiers must not

#### Scenario: Method renames in SignatureRegistry
- **WHEN** `SignatureRegistry::is_transfer_safe(key)` is called
- **THEN** it returns true if and only if local and remote signatures match for the given key
- **AND** the old method name `is_serialization_free` does not exist

#### Scenario: Method renames in CompatReporter
- **WHEN** `CompatReporter::are_transfer_safe(types, platforms)` is called
- **THEN** it returns true if and only if all specified types have matching signatures
  across all specified platforms and none contain pointers
- **AND** the old method name `are_serialization_free` does not exist

### Requirement: File rename
The header file `<boost/typelayout/tools/serialization_free.hpp>` SHALL be renamed to
`<boost/typelayout/tools/transfer.hpp>`. The old include path SHALL NOT exist.

#### Scenario: Include path
- **WHEN** a user includes `<boost/typelayout/tools/transfer.hpp>`
- **THEN** `is_transfer_safe<T>(sig)` and `SignatureRegistry` are available
- **AND** `<boost/typelayout/tools/serialization_free.hpp>` does not exist

### Requirement: CompatReporter report output
CompatReporter report output SHALL use "Transfer-safe" instead of "Serialization-free"
and "Layout mismatch" instead of "Needs serialization".

#### Scenario: Report verdict text
- **WHEN** a type has matching signatures and SafetyLevel::TrivialSafe
- **THEN** the verdict is "Transfer-safe" (not "Serialization-free")

#### Scenario: Report mismatch text
- **WHEN** a type has non-matching signatures across platforms
- **THEN** the verdict is "Layout mismatch" (not "Needs serialization")

### Requirement: SigExporter admission unchanged
`SigExporter::add<T>()` SHALL retain `static_assert(std::is_trivially_copyable_v<T>)`.
The export step collects signatures for cross-platform comparison; types with pointers
have valid signatures and must be exportable so that CompatReporter can report their
safety level. Safety classification happens at the report layer, not the export layer.
Non-trivially-copyable relocatable opaque types SHALL use `add_relocatable<T>()`.

#### Scenario: Pointer-containing type accepted by add()
- **WHEN** a trivially copyable type containing pointers is passed to
  `SigExporter::add<T>(name)`
- **THEN** compilation succeeds (the static_assert passes)
- **AND** CompatReporter classifies it as PointerRisk in the report

#### Scenario: Non-trivially-copyable type uses add_relocatable()
- **WHEN** a non-trivially-copyable relocatable opaque type needs to be exported
- **THEN** the caller uses `SigExporter::add_relocatable<T>(name)` instead of `add<T>()`
