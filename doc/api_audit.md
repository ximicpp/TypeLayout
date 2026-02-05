# API Audit Report

## Overview

This document audits the public API of Boost.TypeLayout for potential breaking changes
and API stability considerations.

**Audit Date**: February 2026  
**API Version**: 1.0.0 (`BOOST_TYPELAYOUT_VERSION = 100000`)

---

## 1. Public API Inventory

### 1.1 Core Functions (`signature.hpp`)

| Function | Signature | Breaking Change Risk | Notes |
|----------|-----------|---------------------|-------|
| `get_arch_prefix()` | `consteval auto` | **Low** | Internal detail, but exposed |
| `get_layout_signature<T>()` | `consteval auto` | **Medium** | Format changes break `static_assert` |
| `signatures_match<T1, T2>()` | `consteval bool` | **Low** | Boolean result is stable |
| `get_layout_signature_cstr<T>()` | `constexpr const char*` | **Medium** | String format may evolve |
| `get_layout_hash<T>()` | `consteval uint64_t` | **High** | Hash algorithm change = major break |
| `hashes_match<T1, T2>()` | `consteval bool` | **Low** | Boolean result is stable |

### 1.2 Verification Functions (`verification.hpp`)

| Function | Signature | Breaking Change Risk | Notes |
|----------|-----------|---------------------|-------|
| `get_layout_verification<T>()` | `consteval LayoutVerification` | **Medium** | Struct layout is ABI |
| `verifications_match<T1, T2>()` | `consteval bool` | **Low** | Boolean result is stable |
| `no_hash_collision<Types...>()` | `consteval bool` | **Low** | Boolean result is stable |
| `no_verification_collision<Types...>()` | `consteval bool` | **Low** | Boolean result is stable |

### 1.3 Macros

| Macro | Breaking Change Risk | Notes |
|-------|---------------------|-------|
| `TYPELAYOUT_BIND(Type, ExpectedSig)` | **Medium** | Depends on signature format |

### 1.4 Types

| Type | Breaking Change Risk | Notes |
|------|---------------------|-------|
| `LayoutVerification` | **High** | Struct layout is public ABI |
| `CompileString<N>` | **Low** | Mostly internal implementation |

---

## 2. Breaking Change Analysis

### 2.1 HIGH RISK: Signature Format Changes

**Issue**: Users may store signature strings in:
- Configuration files
- Protocol headers
- Database records
- `static_assert` statements

**Mitigation**:
1. ✅ Document signature format is **stable within major version**
2. ✅ Use hash comparison instead of string comparison when possible
3. ⚠️ Consider adding `BOOST_TYPELAYOUT_SIGNATURE_VERSION` for format versioning

**Recommendation**: Add signature format version in v1.1:
```cpp
// Proposed: Include format version in signature
// "[v1][64-le]struct{...}"
```

### 2.2 HIGH RISK: Hash Algorithm Changes

**Issue**: `get_layout_hash<T>()` uses FNV-1a. Changing to a different algorithm
would break all persisted hashes.

**Mitigation**:
1. ✅ Document that hash algorithm is **frozen for v1.x**
2. ✅ If new algorithm needed, add `get_layout_hash_v2<T>()` instead
3. ✅ `LayoutVerification` uses dual-hash for future flexibility

**Current Status**: FNV-1a is frozen. DJB2 in `LayoutVerification` provides alternative.

### 2.3 MEDIUM RISK: LayoutVerification Struct Layout

**Issue**: `LayoutVerification` is a public struct:
```cpp
struct LayoutVerification {
    uint64_t fnv1a;     // Offset 0
    uint64_t djb2;      // Offset 8
    uint32_t length;    // Offset 16
};
```

Changing member order or adding fields would break binary compatibility.

**Mitigation**:
1. ✅ Mark struct layout as stable in documentation
2. ⚠️ Consider adding reserved fields for future extension
3. ⚠️ Use accessor functions instead of direct member access

**Recommendation for v1.1**:
```cpp
struct LayoutVerification {
    uint64_t fnv1a;
    uint64_t djb2;
    uint32_t length;
    uint32_t reserved1 = 0;  // Future: additional hash bits
    uint64_t reserved2 = 0;  // Future: algorithm version
    
    // Accessor functions (preferred over direct access)
    constexpr uint64_t hash1() const noexcept { return fnv1a; }
    constexpr uint64_t hash2() const noexcept { return djb2; }
};
```

### 2.4 LOW RISK: `consteval` vs `constexpr`

**Issue**: Some compilers may have `consteval` quirks.

**Status**: Already using `consteval` consistently. No changes needed.

### 2.5 LOW RISK: Namespace Structure

**Current**: `boost::typelayout::*`

**Status**: Stable. No plans to change namespace structure.

---

## 3. Proposed API Stability Policy

### 3.1 Semantic Versioning Guarantees

| Version Increment | What May Change |
|-------------------|-----------------|
| **Patch** (1.0.x) | Bug fixes only, no API changes |
| **Minor** (1.x.0) | New functions added, no removals |
| **Major** (x.0.0) | Breaking changes allowed |

### 3.2 Specific Guarantees

1. **Signature String Format**: Stable within major version
2. **Hash Algorithm (FNV-1a)**: Frozen for v1.x series
3. **`LayoutVerification` Layout**: Binary-stable, may add reserved fields
4. **Function Return Types**: Stable (`bool`, `uint64_t`, `const char*`)
5. **Function Names**: No renames without deprecation period

### 3.3 Deprecation Policy

- Deprecated features marked with `[[deprecated("reason")]]`
- Minimum 2 minor versions before removal
- Deprecated features documented in CHANGELOG

---

## 4. Future-Proofing Recommendations

### 4.1 Immediate (v1.0.x)

- [x] Add version macros (already done)
- [x] Add `[[nodiscard]]` to all functions (already done)
- [ ] Add reserved fields to `LayoutVerification` (recommended)

### 4.2 Short-term (v1.1.0)

- [ ] Add `get_signature_version()` function
- [ ] Add algorithm selector: `get_layout_hash<T, HashAlgorithm::SHA256>()`
- [ ] Add `LayoutVerification::version` field

### 4.3 Long-term (v2.0.0)

- [ ] Consider migrating to cryptographic hash (SHA-256) for security-critical uses
- [ ] Module-based API (`import boost.typelayout`)
- [ ] Separate "stable" vs "experimental" API namespaces

---

## 5. Audit Checklist

| Item | Status | Notes |
|------|--------|-------|
| All public functions have `[[nodiscard]]` | ✅ Done | |
| All pure functions use `consteval` | ✅ Done | |
| No `std::stringstream` in templates | ✅ Done | |
| No `std::locale` dependencies | ✅ Done | |
| Version macros defined | ✅ Done | `config.hpp` |
| Deprecation policy documented | ✅ Done | `reviewer_qa.md` |
| API stability guarantee documented | ✅ Done | README |
| Reserved fields in verification struct | ⚠️ Recommended | v1.1 |
| Signature format versioning | ⚠️ Recommended | v1.1 |

---

## 6. Conclusion

The current API is **stable and ready for Boost review**. Key decisions:

1. **Hash algorithm is frozen** for v1.x series
2. **Signature format is stable** within major versions
3. **Binary layout of `LayoutVerification`** is public ABI

Recommended enhancements for v1.1:
- Add reserved fields to `LayoutVerification` for future extension
- Add signature format version prefix
- Add accessor functions to `LayoutVerification`

**Overall Risk Assessment**: **LOW** - The API follows best practices and has
clear stability guarantees documented.
