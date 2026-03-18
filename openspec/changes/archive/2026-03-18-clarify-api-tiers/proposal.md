## Why

The public API surface has grown organically and lacks clear tiering. Users see
`get_arch_prefix()`, `layout_traits<T>`, `layout_signatures_match<T,U>()`, and
`is_byte_copy_safe_v<T>` at the same level, with no guidance on which concepts
are essential vs. diagnostic vs. convenience. `get_arch_prefix()` is an internal
implementation detail that leaked into the public namespace.

## What Changes

- Move `get_arch_prefix()` from `boost::typelayout` to `boost::typelayout::detail`.
  **BREAKING** (minor: no test or example uses it directly).
- Restructure `api-reference.md` into clear tiers: Core / Convenience / Diagnostic / Tools.
- Reorder `quickstart.md` to lead with the 4 core concepts.
- Update `AGENTS.md` and `rules.mdc` Code Map to reflect API tiers.

## Capabilities

### New Capabilities
- `api-tiers`: Documents the tiered API structure (Core / Convenience / Diagnostic / Tools) and the rationale for each tier assignment.

### Modified Capabilities

(none -- no existing spec requirements change)

## Impact

- `include/boost/typelayout/signature.hpp`: `get_arch_prefix()` moves into `detail` namespace.
- `include/boost/typelayout/tools/sig_export.hpp`: Update call site to `detail::get_arch_prefix()`.
- `docs/api-reference.md`: Major restructure into tiers.
- `docs/quickstart.md`: Reorder narrative.
- `AGENTS.md`, `.codemaker/rules/rules.mdc`: Update Code Map entries.
