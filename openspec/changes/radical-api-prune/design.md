## Context

TypeLayout's public API has 11 symbols after three rounds of simplification.
5 are either syntactic sugar, thin wrappers, or internal implementation details.
This change removes them, leaving 6 essential public concepts.

## Goals / Non-Goals

**Goals:**
- Remove 2 symbols entirely: `layout_signatures_match`, `SignatureRegistry`.
- Internalize 3 symbols: `layout_traits<T>`, `compat::SafetyLevel`, `platform_detect`.
- Reduce public API to 6 concepts across 2 tiers (Core + Tools).
- Update all tests and docs to use only the 6 public concepts.

**Non-Goals:**
- Deleting internal implementation code (everything is moved to `detail`, not deleted).
- Changing signature format or runtime behavior.
- Modifying `SigExporter` or `CompatReporter` public interface (except internal references).

## Decisions

### D1: Delete layout_signatures_match entirely

It is `get_layout_signature<T>() == get_layout_signature<U>()` -- a trivial one-liner.
`FixedString` already supports `operator==`, so the replacement is direct.

All test static_asserts and doc examples using `layout_signatures_match<A,B>()`
will be rewritten to use `==` comparison on `get_layout_signature`.

### D2: Delete SignatureRegistry entirely

It wraps a `std::unordered_map<std::string, std::string>` with `register_local`,
`register_remote`, and `is_transfer_safe` methods. Users can trivially build this
from `is_transfer_safe<T>(sig)` + a map.

The `test_transfer.cpp` Parts 6-8 that test `SignatureRegistry` will be removed.
The `is_transfer_safe<T>(sig)` free function tests (Part 1-5) are retained.

### D3: Move layout_traits<T> to detail namespace

`layout_traits` is used internally by `admission.hpp` (`is_byte_copy_safe`)
and `sig_export.hpp`. It stays as `detail::layout_traits<T>`.

- Remove `layout_traits.hpp` from umbrella `typelayout.hpp` includes.
- Tests that validate layout_traits behavior (`test_layout_traits.cpp`,
  `test_padding_precision.cpp`) continue to work by using `detail::layout_traits`
  directly -- these are internal validation tests, not user-facing examples.

### D4: Move SafetyLevel to detail namespace

Currently in `boost::typelayout::compat::SafetyLevel`. Move to
`boost::typelayout::detail::SafetyLevel`.

`classify_signature()` and `safety_level_name()` also move to detail.
`CompatReporter` uses them internally -- no public interface change.

`test_classify.cpp` and `test_rt_padding.cpp` use SafetyLevel directly;
they will reference `detail::SafetyLevel`.

### D5: Move platform_detect to detail namespace

`platform_detect.hpp` macros and functions move into `detail` namespace.
`SigExporter` uses them internally. `test_sig_export.cpp` may reference
platform info; update to use `detail::` prefix.

### D6: Umbrella header simplification

`typelayout.hpp` will include only:
- `config.hpp`, `fwd.hpp`, `fixed_string.hpp` (infrastructure)
- `signature.hpp` (get_layout_signature)
- `opaque.hpp` (REGISTER_OPAQUE macros)
- `admission.hpp` (is_byte_copy_safe_v)
- `tools/transfer.hpp` (is_transfer_safe)

Removed from umbrella: `layout_traits.hpp`.

### D7: Test strategy

Tests that validate internal mechanisms (padding bitmap, layout_traits fields,
SafetyLevel classification) continue to exist but use `detail::` prefixed symbols.
This is acceptable because tests are internal validation, not user-facing examples.

## Risks / Trade-offs

- [Risk] Users who depend on `layout_traits<T>` lose compile-time `has_padding`
  check without parsing the signature string.
  -> Mitigation: `sig_has_padding()` provides the same check at constexpr level.
  -> If user demand emerges, `layout_traits` can be re-promoted to public later.

- [Risk] Users who depend on `SignatureRegistry` lose a convenient batch API.
  -> Mitigation: The pattern is trivial to replicate with a map + `is_transfer_safe`.

- [Risk] `layout_signatures_match` is used in the paper and conference proposal.
  -> Mitigation: Update paper examples to use `==` comparison. The concept is
  the same; only the syntax changes slightly.
