# Change: Refactor Header Architecture to Boost-Style Layout

## Why

The current `core/` directory structure groups all implementation files at the
same level, mixing public API with internal detail. Boost libraries and the
C++ standard library consistently use a `detail/` (or `__detail/`) pattern to
separate public interface from implementation machinery. This restructure:

1. Promotes the three **public API headers** (`signature.hpp`, `opaque.hpp`,
   `fixed_string.hpp`) to the library root (`boost/typelayout/`).
2. Moves all implementation-only code into `detail/` -- invisible to casual
   users but still accessible when needed.
3. Extracts **platform configuration** macros from `fwd.hpp` into `config.hpp`.
4. Splits the 603-line `signature_detail.hpp` monolith into three focused
   headers by responsibility.
5. Cleans up `sig_export.hpp` include ordering and adds a non-main macro.

## What Changes

### Directory Layout (Before -> After)

```
Before:                                  After:
boost/typelayout/                        boost/typelayout/
  typelayout.hpp                           typelayout.hpp  (umbrella)
  core/                                    config.hpp      (platform macros)
    fwd.hpp                                fwd.hpp         (enums, forward decls)
    signature_detail.hpp                   fixed_string.hpp (FixedString<N>)
    signature.hpp                          signature.hpp   (public API)
    opaque.hpp                             opaque.hpp      (opaque macros)
                                           detail/
                                             reflect.hpp        (P2996 wrappers)
                                             signature_impl.hpp (Layout+Def engines)
                                             type_map.hpp       (TypeSignature<T> specializations)
                                           core/               (backward-compat shims)
                                             fwd.hpp
                                             signature_detail.hpp
                                             signature.hpp
                                             opaque.hpp
```

### Specific Changes

- **New `config.hpp`**: Endianness detection macros extracted from `fwd.hpp`
- **New `fwd.hpp` (root)**: `SignatureMode`, `always_false`, forward declarations only; includes `config.hpp`
- **New `fixed_string.hpp` (root)**: `FixedString<N>` and `to_fixed_string()`; no P2996 dependency
- **New `signature.hpp` (root)**: Public API `get_layout_signature()`, `get_definition_signature()`
- **New `opaque.hpp` (root)**: `TYPELAYOUT_OPAQUE_*` macros
- **New `detail/reflect.hpp`**: `qualified_name_for`, `get_member_count`, `get_base_count`, `is_fixed_enum`, `get_member_name`, `get_base_name`, `get_type_qualified_name`
- **New `detail/signature_impl.hpp`**: Definition engine + Layout engine + union helpers
- **New `detail/type_map.hpp`**: All 50+ `TypeSignature<T, Mode>` specializations
- **Old `core/` headers**: Become one-line backward-compatibility includes
- **`sig_export.hpp`**: Move `#include foreach.hpp` to top; add `TYPELAYOUT_REGISTER_TYPES` macro
- **`typelayout.hpp`**: Updated include list

## Impact

- Affected specs: `signature`
- **BREAKING**: None -- all public APIs and include paths backward-compatible
- Affected code: all headers under `include/boost/typelayout/`