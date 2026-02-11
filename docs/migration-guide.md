# TypeLayout Migration Guide

This guide documents breaking and structural changes in TypeLayout v2.x.

## Header Path Migration

The `core/` directory has been **removed** (Phase 2 complete). All public API
headers now reside at the library root (`boost/typelayout/`). Code that still
includes `core/` paths will fail to compile.

### Header Mapping

| Old Path (deprecated) | New Path | Contents |
|---|---|---|
| `core/fwd.hpp` | `fwd.hpp` + `fixed_string.hpp` | Enums, forward declarations, FixedString |
| `core/signature_detail.hpp` | `detail/type_map.hpp` | TypeSignature specializations |
| `core/signature.hpp` | `signature.hpp` | Public API: get_layout_signature, etc. |
| `core/opaque.hpp` | `opaque.hpp` | TYPELAYOUT_OPAQUE_* macros |

### Quick Fix

Replace your includes:

```cpp
// Before (removed -- will NOT compile):
#include <boost/typelayout/core/fwd.hpp>
#include <boost/typelayout/core/signature.hpp>
#include <boost/typelayout/core/opaque.hpp>

// After:
#include <boost/typelayout/typelayout.hpp>  // umbrella (includes everything)

// Or, for fine-grained control:
#include <boost/typelayout/fwd.hpp>           // SignatureMode, forward declarations
#include <boost/typelayout/fixed_string.hpp>  // FixedString<N>, to_fixed_string()
#include <boost/typelayout/signature.hpp>     // get_layout_signature<T>(), etc.
#include <boost/typelayout/opaque.hpp>        // TYPELAYOUT_OPAQUE_* macros
```

## New Directory Structure

```
boost/typelayout/
    typelayout.hpp          -- Umbrella header (includes all public API)
    config.hpp              -- Platform macros (endianness)
    fwd.hpp                 -- SignatureMode, always_false, forward declarations
    fixed_string.hpp        -- FixedString<N>, to_fixed_string()
    signature.hpp           -- get_layout_signature<T>(), get_definition_signature<T>()
    opaque.hpp              -- TYPELAYOUT_OPAQUE_TYPE/CONTAINER/MAP macros
    detail/
        reflect.hpp         -- P2996 reflection wrappers (internal)
        signature_impl.hpp  -- Layout + Definition engines (internal)
        type_map.hpp        -- TypeSignature specializations (internal)
    tools/                  -- Export and compatibility checking tools
```

## FixedString<N> Semantic Change (P2484 Alignment)

`FixedString<N>` now uses **P2484 semantics**: `N` is the character count
(excluding the null terminator), not the buffer size.

| Aspect | Before | After |
|---|---|---|
| `FixedString{"hello"}` deduced N | 6 | 5 |
| Internal buffer | `char value[N]` | `char value[N + 1]` |
| `FixedString<N>::size` | `N - 1` | `N` |
| Concatenation result size | `N + M - 1` | `N + M` |
| `to_fixed_string()` return | `FixedString<21>` | `FixedString<20>` |

### Impact on User Code

If you use `FixedString{"literal"}` (CTAD), **no changes needed** -- the
deduction guide handles the new semantics automatically.

If you explicitly specify N (e.g., `FixedString<N>(string_view)`), subtract 1
from your old N values:

```cpp
// Before:
FixedString<name.size() + 1>(name)

// After:
FixedString<name.size()>(name)
```

## New Macro: TYPELAYOUT_REGISTER_TYPES

`TYPELAYOUT_REGISTER_TYPES` registers types on an existing `SigExporter`
without generating `main()`. Use it for custom export workflows.

```cpp
#include <boost/typelayout/tools/sig_export.hpp>

int main() {
    boost::typelayout::SigExporter exporter;

    // Register types without generating main():
    TYPELAYOUT_REGISTER_TYPES(exporter, MyStruct, MyOtherStruct, MyEnum);

    // Custom logic before writing:
    exporter.write("output/my_platform.sig.hpp");
    return 0;
}
```

The existing `TYPELAYOUT_EXPORT_TYPES` macro continues to work unchanged.
