Modify the TypeLayout signature format or encoding grammar.

(!) HIGH IMPACT -- This is a breaking change that affects nearly every file in the project.

Use this when: changing how signatures are encoded (e.g., new field format, different delimiters, new arch prefix, new opaque encoding).

## Impact Assessment

Signature format changes affect:
- **Generation**: `detail/signature_impl.hpp` (produces signatures)
- **Compile-time parser**: `layout_traits.hpp` -> `compute_has_padding` (reads signatures)
- **Runtime parser**: `tools/safety_level.hpp` -> `sig_has_padding`, `classify_signature` (reads signatures)
- **All tests**: Every test that checks signature strings or parsing results
- **Export files**: Any existing `.sig.hpp` files become incompatible

Two independent parsers MUST stay in sync: the compile-time bitmap path and the runtime string parser.

## Steps

1. **Document the grammar change first**:
   - Write the new BNF/format rule before implementing
   - Identify all tokens/delimiters that change

2. **Update signature generation** in `detail/signature_impl.hpp`:
   - Modify `TypeSignature<T>::calculate()` to produce the new format
   - If adding new type encodings, also update `detail/type_map.hpp` and `detail/reflect.hpp`

3. **Update the compile-time parser** in `layout_traits.hpp`:
   - `compute_has_padding<T>()` may parse signature fragments
   - Ensure it handles the new format correctly

4. **Update the runtime parser** in `tools/safety_level.hpp`:
   - `sig_has_padding()` -- padding detection from strings
   - `classify_signature()` -- safety level from strings
   - These are C++17 code; test independently with `test_rt_padding.cpp` and `test_compat_check.cpp`

5. **Update ALL tests** -- every test that contains hardcoded signature strings:
   - `test_layout_traits.cpp` -- signature correctness
   - `test_padding_precision.cpp` -- bitmap vs parser agreement
   - `test_classify.cpp` -- safety classification
   - `test_empty_member_probe.cpp` -- EBO/NUA signatures
   - `test_opaque.cpp` -- opaque encoding
   - `test_rt_padding.cpp` -- runtime padding parsing
   - `test_compat_check.cpp` -- compatibility checking
   - `test_sig_export.cpp` -- export format
   - `test_serialization_free.cpp` -- serialization checks

6. **Update any documentation or specs** that describe the format.

7. **Build and test**: Run `/build-test` and verify ALL 10 tests pass.

## Key Files (in update order)

1. `include/boost/typelayout/detail/signature_impl.hpp` -- generation
2. `include/boost/typelayout/detail/type_map.hpp` -- type names (if changed)
3. `include/boost/typelayout/layout_traits.hpp` -- compile-time parser
4. `include/boost/typelayout/tools/safety_level.hpp` -- runtime parser
5. `test/test_*.cpp` -- all test files with signature strings

## Important Notes

- NEVER update generation without updating BOTH parsers
- The compile-time and runtime parsers are independent implementations -- changes to one do not automatically propagate to the other
- Existing `.sig.hpp` export files from other platforms will be incompatible after format changes
- Consider whether the change can be made backwards-compatible (detect old vs new format)
- Run `/sig-check` on representative types after changes to visually verify output
