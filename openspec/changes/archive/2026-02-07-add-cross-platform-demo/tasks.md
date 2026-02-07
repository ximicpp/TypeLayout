## 1. Core Demo Program
- [x] 1.1 Create `example/cross_platform_check.cpp` — define representative types and output their layout/definition signatures in structured format (JSON)
- [x] 1.2 Types to include: network packet, shared-memory region, file header, IPC message, and a known-unsafe type (with `long`, pointer, `wchar_t`)

## 2. Comparison Script
- [x] 2.1 Create `scripts/compare_signatures.py` — reads JSON outputs from multiple platforms, diffs them, and reports compatibility matrix

## 3. Build Integration
- [x] 3.1 Add `cross_platform_check` target to root `CMakeLists.txt`
- [x] 3.2 Add `example/README.md` with usage workflow

## 4. Validation
- [x] 4.1 Build and run in Docker (x86_64), verify JSON output
- [x] 4.2 Verify comparison script with sample data (Linux vs simulated Windows)