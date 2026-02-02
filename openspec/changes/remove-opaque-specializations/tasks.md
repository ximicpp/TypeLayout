## 1. Remove Non-Core Specializations

- [x] 1.1 Delete TypeDiagnostic struct and related code (lines 33-121)
- [x] 1.2 Delete std::unique_ptr specialization
- [x] 1.3 Delete std::shared_ptr specialization
- [x] 1.4 Delete std::weak_ptr specialization
- [x] 1.5 Delete std::array specialization
- [x] 1.6 Delete std::pair specialization
- [x] 1.7 Delete std::span specialization
- [x] 1.8 Delete std::atomic specialization
- [x] 1.9 Delete boost::interprocess::offset_ptr specialization
- [x] 1.10 Remove unused includes (<memory>, <array>, <utility>, <span>, <atomic>)

## 2. Update Tests

- [x] 2.1 Remove or update tests that depend on removed specializations
  - Removed std::atomic tests from test_signature_extended.cpp (due to _Atomic keyword incompatibility)
  - Removed smart pointer tests from test_signature_comprehensive.cpp
- [x] 2.2 Add tests verifying generic reflection handles these types
  - std::string_view and std::span now generate signatures via transparent reflection

## 3. Verification

- [x] 3.1 Build and run all tests
- [x] 3.2 Verify signatures are still generated (via reflection engine)
  - std::string_view: [64-le]struct[s:16,a:8]{@0[__data_]:ptr[s:8,a:8],@8[__size_]:u64[s:8,a:8]}
  - std::span<int>: [64-le]struct[s:16,a:8]{@0[__data_]:ptr[s:8,a:8],@8[__size_]:u64[s:8,a:8]}

## 4. Additional Fixes

- [x] 4.1 Added macOS unsigned long specialization (LP64: 8 bytes)
- [x] 4.2 Restored LayoutSupported concept in concepts.hpp (simple version without diagnostics)