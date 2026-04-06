Modify the TypeLayout signature format or encoding grammar.

(!) HIGH IMPACT -- This is a breaking change that affects nearly every file in the project.

Use this when: changing how signatures are encoded (e.g., new field format, different delimiters, new arch prefix, new opaque encoding).

## Impact Assessment

Signature format changes affect:
- **Generation**: `detail/signature_impl.hpp` (produces signatures)
- **Pointer detection**: `detail/sig_parser.hpp` + `fixed_string.hpp` -> `sig_has_pointer` (reads signatures)
- **Runtime classification**: `tools/safety_level.hpp` -> `classify_signature` (reads signatures)
- **All tests**: Every test that checks signature strings or parsing results
- **Export files**: Any existing `.sig.hpp` files become incompatible

The `sig_has_pointer` function exists in two independent implementations (compile-time in `fixed_string.hpp`, runtime in `sig_parser.hpp`) -- both MUST stay in sync.

## Steps

1. **Document the grammar change first**:
   - Write the new BNF/format rule before implementing
   - Identify all tokens/delimiters that change

2. **Update signature generation** in `detail/signature_impl.hpp`:
   - Modify `TypeSignature<T>::calculate()` to produce the new format
   - If adding new type encodings, also update `detail/type_map.hpp`

3. **Update pointer detection** if pointer-like tokens changed:
   - `detail/sig_parser.hpp` -> `sig_has_pointer()` (runtime, `string_view`)
   - `fixed_string.hpp` -> `sig_has_pointer()` (compile-time, `FixedString`)
   - Both must recognize the same set of tokens

4. **Update runtime classification** in `tools/safety_level.hpp`:
   - `classify_signature()` -- safety level from signature strings
   - This is C++17 code; test with `test_compat_check.cpp`

5. **Update ALL tests** -- every test that contains hardcoded signature strings:
   - `test_layout_traits.cpp` -- signature correctness
   - `test_empty_member_probe.cpp` -- EBO/NUA signatures
   - `test_opaque.cpp` -- opaque encoding
   - `test_compat_check.cpp` -- compatibility checking
   - `test_sig_export.cpp` -- export format

6. **Update any documentation or specs** that describe the format.

7. **Build and test**: Run `/build-test` and verify all tests pass.

## Key Files (in update order)

1. `include/boost/typelayout/detail/signature_impl.hpp` -- generation
2. `include/boost/typelayout/detail/type_map.hpp` -- type names (if changed)
3. `include/boost/typelayout/detail/sig_parser.hpp` -- runtime pointer detection
4. `include/boost/typelayout/fixed_string.hpp` -- compile-time pointer detection
5. `include/boost/typelayout/tools/safety_level.hpp` -- runtime classification
6. `test/test_*.cpp` -- all test files with signature strings

## Important Notes

- NEVER update generation without updating pointer detection in BOTH paths
- The compile-time and runtime `sig_has_pointer` are independent implementations -- changes to one do not automatically propagate to the other
- Existing `.sig.hpp` export files from other platforms will be incompatible after format changes
- Consider whether the change can be made backwards-compatible (detect old vs new format)
- Run `/sig-check` on representative types after changes to visually verify output
