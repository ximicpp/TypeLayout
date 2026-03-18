# TypeLayout Examples

Runnable demo sources for the TypeLayout cross-platform compatibility pipeline.

## Files

| File | Phase | Description |
|------|-------|-------------|
| `cross_platform_check.cpp` | 1 -- Export | Registers types with `TYPELAYOUT_EXPORT_TYPES` and writes `.sig.hpp` files |
| `compat_check.cpp` | 2 -- Check | Includes pre-generated `.sig.hpp` files and runs `TYPELAYOUT_CHECK_COMPAT` |
| `sigs/*.sig.hpp` | -- | Pre-generated signature files for three reference platforms |

## Quick Commands

```bash
# Phase 1: build and run the exporter (requires P2996 Clang)
clang++ -std=c++26 -freflection -freflection-latest -stdlib=libc++ \
    -I../include -o sig_export cross_platform_check.cpp
./sig_export sigs/

# Phase 2: build and run the checker
clang++ -std=c++26 -freflection -freflection-latest -stdlib=libc++ \
    -I../include -o compat_check compat_check.cpp
./compat_check
```

## See Also

- [Quickstart Guide](../docs/quickstart.md) -- end-to-end tutorial including Docker usage and CMake integration
- [API Reference](../docs/api-reference.md) -- `SigExporter`, `CompatReporter`, and all related symbols
