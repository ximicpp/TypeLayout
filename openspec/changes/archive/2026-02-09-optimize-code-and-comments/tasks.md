## 1. Code Refactoring
- [x] 1.1 Create `include/boost/typelayout/tools/detail/foreach.hpp` with the shared FOR_EACH macro machinery
- [x] 1.2 Update `sig_export.hpp` to `#include` the new `detail/foreach.hpp` instead of inline macro definitions
- [x] 1.3 Update `compat_auto.hpp` to `#include` the new `detail/foreach.hpp` and remove its own copy
- [x] 1.4 Remove dead ASSERT macro stubs from `compat_auto.hpp` (`TYPELAYOUT_DETAIL_ASSERT_FIRST`, `TYPELAYOUT_DETAIL_ASSERT_VS_FIRST`, `TYPELAYOUT_ASSERT_COMPAT_2`, `TYPELAYOUT_DETAIL_ASSERT_REF_NS`)

## 2. Comment Simplification
- [x] 2.1 Simplify `compat_check.hpp` — trim separator lines, merge adjacent comment blocks, reduce redundancy
- [x] 2.2 Simplify `example/compat_check.cpp` — condense header block and inline comments

## 3. Documentation Deduplication
- [x] 3.1 Trim `CROSS_PLATFORM_COLLECTION.md` — remove sections that duplicate ZST analysis, add cross-references to `ZERO_SERIALIZATION_TRANSFER.md`

## 4. Verification
- [x] 4.1 Verify all `.hpp` headers still compile correctly (include guard, namespace, no missing macros)
- [x] 4.2 Run `openspec validate optimize-code-and-comments --strict --no-interactive`