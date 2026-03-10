Add a new C++ type category to TypeLayout's signature generation engine.

Use this when: adding support for a new fundamental type, pointer variant, aggregate form, or any new type classification.

## Decision Tree

1. **Leaf/fundamental type** (e.g., `__int128`, `_Float16`) -> add mapping in `detail/type_map.hpp`
2. **New aggregate form** (e.g., variant, tuple-like) -> add handling in `detail/signature_impl.hpp`
3. **Unanalyzable/opaque type** (e.g., `std::string`) -> register via macros in `opaque.hpp`
4. **New type classification** (e.g., new reflect category) -> update `detail/reflect.hpp`

## Steps

1. **Classify the type**: Determine which decision tree branch applies.

2. **For leaf types** -- modify `detail/type_map.hpp`:
   - Add a new `if constexpr` branch mapping the C++ type to its canonical name (e.g., `i128`, `f16`)
   - Follow the existing naming convention: `i`/`u` for integers, `f` for floats, with bit width suffix
   - Handle platform-specific aliasing if needed (see `long` example with Linux/Windows guards)

3. **For aggregate forms** -- modify `detail/signature_impl.hpp`:
   - Add a new branch in `TypeSignature<T>::calculate()`
   - Use P2996 reflection (`[:..:]` syntax) to enumerate members
   - Ensure recursive flattening: call `TypeSignature<FieldT>::calculate()` for each field
   - Compute correct offsets and padding

4. **For opaque types** -- use `TYPELAYOUT_REGISTER_OPAQUE(Type, Tag, HasPointer)` in `opaque.hpp`

5. **Update safety classification** in `tools/safety_level.hpp`:
   - If the new type needs a new safety level or classification rule, update `classify_signature()`

6. **Update dual-path padding detection** if the new type can have padding:
   - `layout_traits.hpp`: update `compute_has_padding<T>()` bitmap logic
   - `tools/safety_level.hpp`: update `sig_has_padding()` string parser
   - Both paths MUST agree -- the `static_assert` in `layout_traits.hpp` enforces this

7. **Add tests**:
   - `test_layout_traits.cpp`: signature correctness, trait flags
   - `test_padding_precision.cpp`: padding bitmap vs parser agreement
   - `test_classify.cpp`: safety classification for the new type

8. **Build and test**: Run `/build-test` to verify all 10 tests pass.

## Key Files

- `include/boost/typelayout/detail/type_map.hpp` -- type name mappings
- `include/boost/typelayout/detail/signature_impl.hpp` -- signature generation engine
- `include/boost/typelayout/detail/reflect.hpp` -- type classification helpers
- `include/boost/typelayout/opaque.hpp` -- opaque registration macros
- `include/boost/typelayout/layout_traits.hpp` -- padding bitmap + traits
- `include/boost/typelayout/tools/safety_level.hpp` -- runtime parser + classification

## Important Notes

- Always run `/sig-check <YourType>` after implementation to verify the generated signature
- Platform-specific types need guards in `type_map.hpp` (check `sizeof` and `#ifdef`)
- The dual-path padding cross-validation is non-negotiable -- both paths must stay in sync
- Array types: `type_has_opaque` and `compute_has_padding` recurse via `std::remove_all_extents_t`
