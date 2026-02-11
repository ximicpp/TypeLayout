## 1. Implementation
- [x] 1.1 Add `static constexpr bool is_opaque = true` to TYPELAYOUT_OPAQUE_TYPE macro
- [x] 1.2 Add `static constexpr bool is_opaque = true` to TYPELAYOUT_OPAQUE_CONTAINER macro
- [x] 1.3 Add `static constexpr bool is_opaque = true` to TYPELAYOUT_OPAQUE_MAP macro
- [x] 1.4 Add `has_opaque_signature<T, Mode>` concept in signature_detail.hpp
- [x] 1.5 Guard `layout_field_with_comma` flattening branch with `!has_opaque_signature` check
- [x] 1.6 Enable integration Tests 12-13 in test_opaque.cpp
- [x] 1.7 Verify all tests pass (clean build + test_opaque + test_two_layer)
