## Context

The core of TypeLayout lives in two files: `fwd.hpp` (176 lines) and
`signature_detail.hpp` (603 lines). The monolithic structure works but
creates unnecessary coupling and does not follow Boost conventions.

Boost libraries (e.g., Boost.MP11, Boost.PFR, Boost.Describe) use a
standard two-tier layout:
- Root-level headers for the public API
- `detail/` directory for implementation machinery

## Goals / Non-Goals

**Goals:**
- Each header has a single responsibility
- Public API at library root; implementation in `detail/`
- Backward compatibility: `#include <boost/typelayout/core/fwd.hpp>` still works
- Follow Boost naming conventions
- No public API changes

**Non-Goals:**
- Performance improvements (this is pure refactoring)
- New features
- Changing the signature format

## Decisions

### Decision 1: Promote public headers to library root

The three public API headers become top-level:
- `boost/typelayout/signature.hpp` -- `get_layout_signature<T>()`, `get_definition_signature<T>()`
- `boost/typelayout/opaque.hpp` -- `TYPELAYOUT_OPAQUE_*` macros
- `boost/typelayout/fixed_string.hpp` -- `FixedString<N>`, `to_fixed_string()`

### Decision 2: Extract config.hpp

Platform configuration (endianness detection) moves from `fwd.hpp` to
`config.hpp`. This separates preprocessor machinery from type definitions.

Include chain: `config.hpp` -> `fwd.hpp` -> `fixed_string.hpp` -> `signature.hpp`

### Decision 3: Split signature_detail.hpp into three detail headers

```
Before:
  signature_detail.hpp (603 lines)
    Part 1: P2996 Reflection Meta-Operations     (~90 lines)
    Part 2: Definition Signature Engine           (~100 lines)
    Part 3: Layout Signature Engine               (~140 lines)
    Part 4: TypeSignature Specializations         (~260 lines)

After:
  detail/reflect.hpp           (~90 lines)  -- qualified_name_for, get_member_count, etc.
  detail/signature_impl.hpp    (~280 lines) -- Layout + Definition engines + union helpers
  detail/type_map.hpp          (~260 lines) -- all TypeSignature<T,M> specializations
```

Include chain: `reflect.hpp` <- `signature_impl.hpp` <- `type_map.hpp`

The `type_map.hpp` contains the primary template `TypeSignature<T,M>` (the
"catch-all" for struct/class/enum/union) plus all explicit specializations
for fundamental types.

### Decision 4: Backward-compatibility shims in core/

Old `core/` headers become single-include forwarding headers:
```cpp
// core/fwd.hpp
#include <boost/typelayout/fwd.hpp>
#include <boost/typelayout/fixed_string.hpp>
```

This ensures existing code that includes `core/fwd.hpp` continues to compile.

### Decision 5: Add TYPELAYOUT_REGISTER_TYPES macro

```cpp
#define TYPELAYOUT_REGISTER_TYPES(exporter_var, ...)
// Expands to: exporter_var.add<Type1>(#Type1); exporter_var.add<Type2>(#Type2); ...
// Does NOT generate main()
```

`TYPELAYOUT_EXPORT_TYPES` will be reimplemented using `TYPELAYOUT_REGISTER_TYPES`.

### Decision 6: Header include guard naming

New guards follow the Boost pattern with path-derived names:
- `BOOST_TYPELAYOUT_CONFIG_HPP`
- `BOOST_TYPELAYOUT_FWD_HPP`
- `BOOST_TYPELAYOUT_FIXED_STRING_HPP`
- `BOOST_TYPELAYOUT_SIGNATURE_HPP`
- `BOOST_TYPELAYOUT_OPAQUE_HPP`
- `BOOST_TYPELAYOUT_DETAIL_REFLECT_HPP`
- `BOOST_TYPELAYOUT_DETAIL_SIGNATURE_IMPL_HPP`
- `BOOST_TYPELAYOUT_DETAIL_TYPE_MAP_HPP`

## Risks / Trade-offs

- **Risk**: Include order sensitivity -- mitigated by keeping the single
  `typelayout.hpp` umbrella header that includes everything in order.
- **Risk**: Existing users who `#include <boost/typelayout/core/signature_detail.hpp>`
  directly will still compile (backward-compat shim).
- **Risk**: `detail/type_map.hpp` contains both the primary template and
  specializations; if the primary template is needed earlier, a forward
  declaration in `fwd.hpp` resolves the dependency.

## Open Questions

None -- all decisions are straightforward refactoring with no behavioral change.