# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [2.0.0] - 2026-02-07

### Added
- **Two-Layer Signature System**: Mathematically grounded architecture
  - **Layout layer** (`get_layout_signature<T>()`): Pure byte layout, flattened inheritance, no names
  - **Definition layer** (`get_definition_signature<T>()`): Full type definition tree, with names and inheritance
- Mathematical relationship: `Layout = project(Definition)`, `definition_match ⟹ layout_match`
- New API functions:
  - `get_layout_signature<T>()` / `get_layout_hash<T>()` / `layout_signatures_match<T, U>()`
  - `get_definition_signature<T>()` / `get_definition_hash<T>()` / `definition_signatures_match<T, U>()`
  - `get_layout_verification<T>()` / `get_definition_verification<T>()` — dual-hash verification
  - `layout_verifications_match<T, U>()` / `definition_verifications_match<T, U>()`
  - `get_layout_signature_cstr<T>()` / `get_definition_signature_cstr<T>()` — C-string for runtime
- New concepts: `LayoutCompatible`, `LayoutHashCompatible`, `DefinitionCompatible`, `DefinitionHashCompatible`
- New macros: `TYPELAYOUT_ASSERT_LAYOUT_COMPATIBLE`, `TYPELAYOUT_BIND_LAYOUT`, `TYPELAYOUT_ASSERT_DEFINITION_COMPATIBLE`, `TYPELAYOUT_BIND_DEFINITION`
- Variable templates: `layout_signature_v<T>`, `layout_hash_v<T>`, `definition_signature_v<T>`, `definition_hash_v<T>`
- Comprehensive test suite: `test_two_layer.cpp` with 30+ assertions
- Tool support: `typelayout-tool generate --layer layout|definition|both`

### Changed
- **BREAKING**: Replaced three-mode system (Structural/Physical/Annotated) with two-layer system (Layout/Definition)
- **BREAKING**: `SignatureMode` enum changed from `{Structural, Physical, Annotated}` to `{Layout, Definition}`
- **BREAKING**: Removed all v1.0 API functions:
  - `signatures_match` → `layout_signatures_match`
  - `hashes_match` → `layout_hashes_match`
  - `verifications_match` → `layout_verifications_match`
  - `physical_signatures_match` → `layout_signatures_match` (Layout layer now does inheritance flattening)
  - `get_structural_signature` → `get_layout_signature`
  - `get_annotated_signature` → `get_definition_signature`
  - `LayoutMatch` concept → removed (use `TYPELAYOUT_BIND_LAYOUT` macro)
  - `LayoutHashMatch` concept → removed (use direct hash comparison)
  - `TYPELAYOUT_BIND` → `TYPELAYOUT_BIND_LAYOUT`
- All class/struct types now use unified `record` prefix (removed `struct`/`class` distinction)
- Version bumped to 2.0.0

### Removed
- `SignatureMode::Structural` (replaced by `SignatureMode::Layout`)
- `SignatureMode::Physical` (merged into `SignatureMode::Layout`)
- `SignatureMode::Annotated` (replaced by `SignatureMode::Definition`)
- All deprecated v1.0 API functions and concepts

## [Unreleased]

## [0.1.0] - 2025-01-15

### Added

#### Core Features
- **Type Signature Generation** (`get_type_signature<T>()`)
  - Compile-time type layout fingerprinting using P2996 reflection
  - Recursive structure analysis for nested types
  - Platform-independent signature format

- **Type Information** (`get_type_info<T>()`)
  - Complete type metadata including size, alignment, and field count
  - Cross-platform field offset calculation
  - Bit-field support with bit-level precision

- **Type Descriptor** (`get_type_descriptor<T>()`)
  - Human-readable type layout description
  - Field-by-field breakdown with types and offsets

#### Serialization Support
- **Binary Serialization** (`serialize()` / `deserialize()`)
  - Safe cross-platform binary serialization
  - Signature-based compatibility verification
  - Automatic endianness detection

- **Serialization Concepts** (`Serializable`, `SerializableTo`)
  - C++20 concepts for compile-time serialization validation
  - Platform-specific compatibility checks

#### Build System
- CMake configuration with `INTERFACE` library support
- B2/Jamfile support for Boost integration
- Header-only library design

### Technical Details
- Requires C++26 with P2996 reflection support
- Tested with experimental Clang P2996 implementation
- Boost Software License 1.0

[Unreleased]: https://github.com/ximicpp/TypeLayout/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/ximicpp/TypeLayout/releases/tag/v0.1.0
