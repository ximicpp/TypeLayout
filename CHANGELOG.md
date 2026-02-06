# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Boost 标准差距分析报告
- CTest 集成支持
- 贡献指南和 Issue 模板
- **Signature Modes**: `SignatureMode::Structural` (default) and `SignatureMode::Annotated`
- New API functions:
  - `get_structural_signature<T>()` - Layout-only signature (no member names)
  - `get_annotated_signature<T>()` - Includes member/type names for debugging
  - `get_layout_signature<T, Mode>()` - Signature with explicit mode parameter
- Test suite: `test_signature_modes.cpp` validating name-independence

### Changed
- 位域类型现在被允许序列化（基于签名驱动模型）
- **BREAKING**: Default signature mode changed to `Structural` (excludes member names)
  - **Migration**: Use `get_annotated_signature<T>()` if you need member names in output
  - **Impact**: Types with identical layouts but different field names now produce matching signatures
- `get_layout_hash<T>()` now always uses Structural mode (name-independent)
- `signatures_match<T, U>()` now always uses Structural mode
- All `LayoutCompatible` and `LayoutHashCompatible` concepts use Structural mode

### Fixed
- **Signature semantics bug**: Previously, member names were included in signatures, causing
  types with identical memory layouts but different field names to be considered incompatible.
  This violated the core guarantee "Identical signature ⟺ Identical memory layout".

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
