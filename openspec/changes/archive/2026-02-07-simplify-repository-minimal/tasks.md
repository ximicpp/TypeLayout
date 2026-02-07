## 1. Delete outdated documentation
- [x] 1.1 Remove entire `doc/` directory
- [x] 1.2 Remove `CONTRIBUTING.md`
- [x] 1.3 Remove `CHANGELOG.md`

## 2. Delete legacy tests
- [x] 2.1 Remove all test files except `test/test_two_layer.cpp`
- [x] 2.2 Remove `test/CMakeLists.txt` and `test/Jamfile`

## 3. Delete non-core code
- [x] 3.1 Remove `include/boost/typelayout/utils/` directory (hash.hpp)
- [x] 3.2 Remove `include/boost/typelayout/core/verification.hpp`
- [x] 3.3 Remove `include/boost/typelayout/core/concepts.hpp`
- [x] 3.4 Strip `signature.hpp` to core functions only (remove hash, cstr, variable templates, macros)
- [x] 3.5 Update `typelayout.hpp` to remove verification and concepts includes

## 4. Delete auxiliary artifacts
- [x] 4.1 Remove `bench/` directory
- [x] 4.2 Remove `scripts/` directory
- [x] 4.3 Remove `tools/` directory
- [x] 4.4 Remove `meta/` directory
- [x] 4.5 Remove `example/` directory
- [x] 4.6 Remove root `Jamfile`
- [x] 4.7 Remove `cmake/` directory

## 5. Update remaining files
- [x] 5.1 Simplify `CMakeLists.txt` (tests only, remove examples/tools/install)
- [x] 5.2 Update `README.md` (remove hash/verification/concepts references)
- [x] 5.3 Update `test/test_two_layer.cpp` (remove hash/verification assertions if any)
- [x] 5.4 Update `.github/workflows/ci.yml` (remove examples/tools/benchmarks steps)

## 6. Verify
- [x] 6.1 Docker build and test pass
- [x] 6.2 Clean git status (no untracked leftover files)
