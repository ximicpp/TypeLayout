# TypeLayout Examples

This directory contains runnable demo sources for the TypeLayout
cross-platform compatibility pipeline.

For the full walkthrough (two-phase pipeline, Docker, CMake integration,
output format, platform naming), see **[docs/quickstart.md](../docs/quickstart.md#6-cross-platform-verification)**.

## File Reference

| File | Phase | Description |
|------|-------|-------------|
| `cross_platform_check.cpp` | 1 -- Export | Export type signatures via `TYPELAYOUT_EXPORT_TYPES(...)` |
| `compat_check.cpp` | 2 -- Check | Compare signatures via `TYPELAYOUT_CHECK_COMPAT(...)` |
| `sigs/*.sig.hpp` | -- | Pre-generated signature files for 3 platforms |

## Quick Commands

```bash
# Phase 1: Build and run the exporter (requires P2996 Clang)
clang++ -std=c++26 -freflection -freflection-latest -stdlib=libc++ \
    -I./include -o sig_export example/cross_platform_check.cpp
./sig_export sigs/

# Phase 2: Build and run the checker (C++17 is sufficient)
clang++ -std=c++17 -stdlib=libc++ -I./include -I./example \
    -o compat_check example/compat_check.cpp
./compat_check
```

## See Also

- [Quickstart Guide](../docs/quickstart.md) -- end-to-end tutorial
- [Best Practices](../docs/best-practices.md) -- field ordering, naming, CI tips
- [API Reference](../docs/api-reference.md) -- all public symbols