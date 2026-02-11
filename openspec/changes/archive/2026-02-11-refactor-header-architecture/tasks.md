## 1. Create New Root-Level Headers

- [x] 1.1 Create `config.hpp` -- endianness detection macros from `fwd.hpp`
- [x] 1.2 Create `fwd.hpp` (root) -- `SignatureMode`, `always_false`, forward declarations; includes `config.hpp`
- [x] 1.3 Create `fixed_string.hpp` (root) -- `FixedString<N>` and `to_fixed_string()`; includes `fwd.hpp`

## 2. Create Detail Headers (split signature_detail.hpp)

- [x] 2.1 Create `detail/reflect.hpp` -- P2996 meta-operations (`qualified_name_for`, `get_member_count`, `get_base_count`, `is_fixed_enum`, `get_member_name`, `get_base_name`, `get_type_qualified_name`)
- [x] 2.2 Create `detail/signature_impl.hpp` -- Definition engine + Layout engine + union helpers; includes `detail/reflect.hpp`
- [x] 2.3 Create `detail/type_map.hpp` -- `TypeSignature<T,M>` primary template + all specializations; includes `detail/signature_impl.hpp`

## 3. Promote Public API Headers to Root

- [x] 3.1 Create `signature.hpp` (root) -- `get_arch_prefix()`, `get_layout_signature<T>()`, `get_definition_signature<T>()`; includes `detail/type_map.hpp`
- [x] 3.2 Create `opaque.hpp` (root) -- `TYPELAYOUT_OPAQUE_*` macros; includes `fwd.hpp` and `fixed_string.hpp`

## 4. Create Backward-Compatibility Shims in core/

- [x] 4.1 Replace `core/fwd.hpp` with shim that includes root `fwd.hpp` + `fixed_string.hpp`
- [x] 4.2 Replace `core/signature_detail.hpp` with shim that includes `detail/type_map.hpp`
- [x] 4.3 Replace `core/signature.hpp` with shim that includes root `signature.hpp`
- [x] 4.4 Replace `core/opaque.hpp` with shim that includes root `opaque.hpp`

## 5. Update Umbrella and Tool Headers

- [x] 5.1 Update `typelayout.hpp` to include new root-level headers
- [x] 5.2 Update `tools/sig_export.hpp` -- move `#include foreach.hpp` to top, add `TYPELAYOUT_REGISTER_TYPES`

## 6. Verify

- [x] 6.1 Build and run all existing tests
- [x] 6.2 Build example/cross_platform_check.cpp
- [x] 6.3 Build example/compat_check.cpp