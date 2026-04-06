Debug a type's signature generation or trait computation failure.

Use this when: a signature is wrong, pointer detection gives unexpected results, or signature comparison gives unexpected results.

## Steps

1. **See the actual output**: Run `/sig-check <Type>` to get the current signature, traits, and safety level.

2. **Trace the generation path** through `detail/signature_impl.hpp`:
   - Find `TypeSignature<T>::calculate()` -- this is the entry point
   - Identify which branch handles the type (fundamental, record, array, enum, opaque)
   - Check offset calculations and recursive field handling

3. **Identify the issue** using this lookup table:

   | Symptom | Likely cause | File to check |
   |---------|-------------|---------------|
   | Wrong type name (e.g., `i32` instead of `i64`) | Platform-specific alias | `detail/type_map.hpp` |
   | Wrong field offset | Alignment not accounted for | `detail/signature_impl.hpp` |
   | Opaque type not recognized | Registration missing or after use | `opaque.hpp`, check include order |
   | `has_pointer` wrong | Pointer token detection mismatch | `detail/sig_parser.hpp` + `fixed_string.hpp` |
   | Wrong safety level | Classification rule mismatch | `tools/safety_level.hpp` -> `classify_signature()` |
   | Signature mismatch between platforms | Type size/alignment differs | `detail/type_map.hpp`, check `sizeof`/`alignof` |

4. **For pointer detection issues**:
   - `sig_has_pointer` in `sig_parser.hpp` (runtime, `string_view`) scans for `ptr[`, `fnptr[`, `memptr[`, `ref[`, `rref[`, `vptr`
   - `sig_has_pointer` in `fixed_string.hpp` (compile-time, `FixedString`) must recognize the same tokens
   - `layout_traits<T>::has_pointer` uses the compile-time version, or `TypeSignature<T>::pointer_free` for opaque types

5. **For flattening issues**:
   - Signatures erase field names and inheritance -- only byte layout matters
   - Check that `get_layout_signature<A>() == get_layout_signature<B>()` compares the full byte sequence
   - Empty base classes should be optimized away (EBO) -- see `test_empty_member_probe.cpp`

6. **Verify the fix**: Run `/build-test` to ensure all tests pass.

## Key Files

- `include/boost/typelayout/detail/signature_impl.hpp` -- signature generation engine
- `include/boost/typelayout/detail/type_map.hpp` -- type name mappings
- `include/boost/typelayout/detail/sig_parser.hpp` -- runtime pointer detection
- `include/boost/typelayout/fixed_string.hpp` -- compile-time pointer detection
- `include/boost/typelayout/layout_traits.hpp` -- signature + has_pointer traits
- `include/boost/typelayout/tools/safety_level.hpp` -- runtime safety classification

## Important Notes

- `sig_has_pointer` exists in two independent implementations -- when fixing one, always check the other
- Use `/sig-check` liberally -- it is the fastest way to see what the engine actually produces
