## Why

After three rounds of simplification, TypeLayout still exposes 11 public concepts.
Analysis shows that 5 of them are either pure syntactic sugar, convenience wrappers,
or internal implementation details that leaked into the public API. Removing them
reduces the public surface to 6 concepts -- the minimum necessary to deliver the
library's core value proposition.

The guiding principle: **if a concept can be trivially replicated from other public
concepts, or is only consumed by one internal component, it should not be public.**

## What Changes

### DELETE from public API (5 concepts removed)

| Symbol | Reason | Replacement |
|--------|--------|-------------|
| `layout_signatures_match<T,U>()` | Pure sugar | `get_layout_signature<T>() == get_layout_signature<U>()` |
| `layout_traits<T>` | 6/7 fields are redundant with `sizeof`/`alignof`/signature parsing; `field_count` is niche | Users check `is_byte_copy_safe_v<T>` for safety, `sig_has_padding()` for padding |
| `SignatureRegistry` | Convenience wrapper over `std::map` + `is_transfer_safe` | Users build their own map or call `is_transfer_safe<T>(sig)` directly |
| `compat::SafetyLevel` | Only consumed by `CompatReporter` for display | Move into `CompatReporter` implementation (detail) |
| `platform_detect` macros | Only consumed by `SigExporter` for auto-naming | Move into `SigExporter` implementation (detail) |

### KEEP as public API (6 concepts remain)

| Symbol | Tier | Role |
|--------|------|------|
| `get_layout_signature<T>()` | Core | Signature generation |
| `is_byte_copy_safe_v<T>` | Core | Compile-time admission |
| `is_transfer_safe<T>(sig)` | Core | Runtime cross-endpoint verification |
| `TYPELAYOUT_REGISTER_OPAQUE` | Core | Opaque type registration |
| `SigExporter` | Tools | Phase 1: export platform signatures |
| `CompatReporter` | Tools | Phase 2: cross-platform compatibility report |

### INTERNAL (moved to detail, not deleted)

- `layout_traits<T>` -> `detail::layout_traits<T>` (still used by admission/signature engine)
- `compat::SafetyLevel` -> `detail::SafetyLevel` (still used by CompatReporter)
- `platform_detect` -> `detail::platform_detect` (still used by SigExporter)
- `layout_signatures_match` -> deleted entirely (trivial one-liner)
- `SignatureRegistry` -> deleted entirely (thin wrapper)

## Capabilities

### Modified Capabilities
- `api-tiers`: Reduced from 4 tiers (Core/Convenience/Diagnostic/Tools) to 2 tiers (Core/Tools).

## Impact

### Headers affected

| File | Change |
|------|--------|
| `signature.hpp` | Remove `layout_signatures_match` |
| `layout_traits.hpp` | Move `layout_traits<T>` into `namespace detail` |
| `tools/transfer.hpp` | Remove `SignatureRegistry` class |
| `tools/safety_level.hpp` | Move `SafetyLevel` from `compat::` to `detail::` |
| `tools/compat_check.hpp` | Update to use `detail::SafetyLevel` |
| `tools/platform_detect.hpp` | Wrap content in `namespace detail` |
| `tools/sig_export.hpp` | Update to use `detail::platform_detect` |
| `typelayout.hpp` | Remove `layout_traits.hpp` from umbrella includes |

### Tests affected

| Test | Change |
|------|--------|
| `test_layout_traits.cpp` | Use `detail::layout_traits<T>` or rewrite to use Core APIs |
| `test_transfer.cpp` | Remove `SignatureRegistry` tests; keep `is_transfer_safe` tests |
| `test_classify.cpp` | Use `detail::SafetyLevel` or rewrite |
| `test_padding_precision.cpp` | Use `detail::layout_traits` |
| `test_compat_check.cpp` | Update `SafetyLevel` references |
| `test_byte_copy_safe.cpp` | May reference `layout_traits` |
| All tests using `layout_signatures_match` | Replace with `==` comparison |

### Documentation affected

- `docs/api-reference.md`: Major rewrite -- only 6 symbols
- `docs/quickstart.md`: Remove `layout_traits` section, `layout_signatures_match` examples
- `AGENTS.md`, `rules.mdc`: Update API Tiers to 2-tier model

### Migration cost for users

- `layout_signatures_match<A,B>()` -> `get_layout_signature<A>() == get_layout_signature<B>()`
- `layout_traits<T>::has_padding` -> `sig_has_padding(get_layout_signature<T>().view())`
- `layout_traits<T>::has_pointer` -> `!is_byte_copy_safe_v<T>` (approximate) or check signature
- `SignatureRegistry` -> `std::unordered_map<std::string, std::string>` + `is_transfer_safe`
- `compat::SafetyLevel` -> not accessible (report-internal)
