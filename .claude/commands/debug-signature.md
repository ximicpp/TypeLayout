Debug a type's signature generation or dual-path validation failure.

Use this when: a signature is wrong, `static_assert` fires on padding cross-validation, or signature comparison gives unexpected results.

## Steps

1. **See the actual output**: Run `/sig-check <Type>` to get the current signature, traits, and safety level.

2. **Trace the generation path** through `detail/signature_impl.hpp`:
   - Find `TypeSignature<T>::calculate()` -- this is the entry point
   - Identify which branch handles the type (fundamental, record, array, enum, opaque)
   - Check offset calculations, padding insertion, and recursive field handling

3. **Identify the issue** using this lookup table:

   | Symptom | Likely cause | File to check |
   |---------|-------------|---------------|
   | Wrong type name (e.g., `i32` instead of `i64`) | Platform-specific alias | `detail/type_map.hpp` |
   | Missing padding bytes | Offset calculation error | `detail/signature_impl.hpp` |
   | Wrong field offset | Alignment not accounted for | `detail/signature_impl.hpp` |
   | Opaque type not recognized | Registration missing or after use | `opaque.hpp`, check include order |
   | `has_padding` disagrees with signature | Dual-path desync | `layout_traits.hpp` + `tools/safety_level.hpp` |
   | Wrong safety level | Classification rule mismatch | `tools/safety_level.hpp` -> `classify_signature()` |
   | Signature mismatch between platforms | Type size/alignment differs | `detail/type_map.hpp`, check `sizeof`/`alignof` |

4. **For dual-path padding failures** (`static_assert` in `layout_traits.hpp`):
   - `compute_has_padding<T>()` uses a byte-coverage bitmap via P2996 reflection
   - `sig_has_padding()` parses the signature string looking for `pad:` tokens
   - Compare both results. Common causes:
     - New type added to signature engine but not to bitmap logic
     - Padding at end of struct (tail padding) handled differently
     - Array element padding not recursed into

5. **For flattening issues**:
   - Signatures erase field names and inheritance -- only byte layout matters
   - Check that `get_layout_signature<A>() == get_layout_signature<B>()` compares the full byte sequence
   - Empty base classes should be optimized away (EBO) -- see `test_empty_member_probe.cpp`

6. **Verify the fix**: Run `/build-test` to ensure all 10 tests pass.

## Key Files

- `include/boost/typelayout/detail/signature_impl.hpp` -- signature generation engine
- `include/boost/typelayout/detail/type_map.hpp` -- type name mappings
- `include/boost/typelayout/detail/reflect.hpp` -- type classification
- `include/boost/typelayout/layout_traits.hpp` -- padding bitmap + cross-validation
- `include/boost/typelayout/tools/safety_level.hpp` -- runtime signature parser

## Important Notes

- The dual-path cross-validation is intentional and must never be disabled
- When fixing one path, always check if the other path needs a matching update
- Use `/sig-check` liberally -- it is the fastest way to see what the engine actually produces
