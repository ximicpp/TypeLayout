## 1. Delete outdated documentation
- [ ] 1.1 Remove entire `doc/` directory
- [ ] 1.2 Remove `CONTRIBUTING.md`
- [ ] 1.3 Remove `CHANGELOG.md`

## 2. Delete legacy tests
- [ ] 2.1 Remove all test files except `test/test_two_layer.cpp`
- [ ] 2.2 Remove `test/CMakeLists.txt` and `test/Jamfile`

## 3. Delete non-core code
- [ ] 3.1 Remove `include/boost/typelayout/utils/` directory (hash.hpp)
- [ ] 3.2 Remove `include/boost/typelayout/core/verification.hpp`
- [ ] 3.3 Remove `include/boost/typelayout/core/concepts.hpp`
- [ ] 3.4 Strip `signature.hpp` to core functions only (remove hash, cstr, variable templates, macros)
- [ ] 3.5 Update `typelayout.hpp` to remove verification and concepts includes

## 4. Delete auxiliary artifacts
- [ ] 4.1 Remove `bench/` directory
- [ ] 4.2 Remove `scripts/` directory
- [ ] 4.3 Remove `tools/` directory
- [ ] 4.4 Remove `meta/` directory
- [ ] 4.5 Remove `example/` directory
- [ ] 4.6 Remove root `Jamfile`
- [ ] 4.7 Remove `cmake/` directory

## 5. Update remaining files
- [ ] 5.1 Simplify `CMakeLists.txt` (tests only, remove examples/tools/install)
- [ ] 5.2 Update `README.md` (remove hash/verification/concepts references)
- [ ] 5.3 Update `test/test_two_layer.cpp` (remove hash/verification assertions if any)
- [ ] 5.4 Update `.github/workflows/ci.yml` (remove examples/tools/benchmarks steps)

## 6. Verify
- [ ] 6.1 Docker build and test pass
- [ ] 6.2 Clean git status (no untracked leftover files)