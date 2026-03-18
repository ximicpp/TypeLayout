## Context

TypeLayout's public API has ~14 symbols across 6 headers included by the umbrella
`typelayout.hpp`. After several rounds of concept simplification (removing
`classify<T>`, `is_local_serialization_free`, etc.), the remaining symbols are all
justified -- but they lack explicit tiering. Users cannot easily distinguish the
4 essential concepts from diagnostic/convenience helpers.

`get_arch_prefix()` is currently in the public `boost::typelayout` namespace but
is only used internally by `get_layout_signature<T>()` and `SigExporter`. No test
or user code calls it directly.

## Goals / Non-Goals

**Goals:**
- Move `get_arch_prefix()` to `detail` namespace (code change).
- Restructure documentation to present a clear 4-tier API hierarchy.
- Make the "4 core concepts" the primary entry point for new users.

**Non-Goals:**
- Removing `layout_traits<T>` or `layout_signatures_match<T,U>()` from the umbrella header.
- Changing any runtime behavior or signature format.
- Splitting `transfer.hpp` (SignatureRegistry stays alongside `is_transfer_safe`).

## Decisions

### D1: Move get_arch_prefix() to detail namespace

Move the function into `namespace detail` within `signature.hpp`. Update the
sole external call site in `sig_export.hpp` to use `detail::get_arch_prefix()`.

**Alternative considered**: Delete `get_arch_prefix()` entirely and inline its
logic into `get_layout_signature()`. Rejected because `sig_export.hpp` also
needs it, so a shared `detail` function is cleaner.

### D2: API tier definitions

| Tier | Meaning | Symbols |
|------|---------|---------|
| Core | The 4 essential concepts every user needs | `get_layout_signature<T>()`, `is_byte_copy_safe_v<T>`, `is_transfer_safe<T>(sig)`, `TYPELAYOUT_REGISTER_OPAQUE` macros |
| Convenience | Useful shorthand, could be replicated from Core | `layout_signatures_match<T,U>()` |
| Diagnostic | Detailed layout inspection for advanced users | `layout_traits<T>` (signature, has_pointer, has_opaque, has_padding, field_count, total_size, alignment) |
| Tools | Separate include, cross-platform workflow | `SignatureRegistry`, `SigExporter`, `CompatReporter`, `compat::SafetyLevel`, `platform_detect` |

### D3: Documentation restructure plan

- `api-reference.md`: Reorder sections by tier (Core first, then Convenience, Diagnostic, Tools).
- `quickstart.md`: Lead with the 4 core concepts in the first example.
- `AGENTS.md` and `rules.mdc`: Add "API Tiers" section to Code Map.

## Risks / Trade-offs

- [Risk] Moving `get_arch_prefix()` to detail is a breaking change for anyone
  calling it directly. -> Mitigation: grep confirmed zero external usage.
- [Risk] Users may expect `layout_traits<T>` to be "core" since it is in the
  umbrella header. -> Mitigation: It stays in the umbrella; the tier label is
  documentation-only, not an access restriction.
