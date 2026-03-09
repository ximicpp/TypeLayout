Review the design and implementation quality of TypeLayout codebase changes.

Use this when: before committing, after implementing a feature/fix, or for periodic code quality audits.

## Review Scope

Determine what to review:
- If `$ARGUMENTS` specifies files or a scope, review only those
- If empty, review all uncommitted changes (`git diff` + `git diff --staged` + untracked files)

## Steps

1. **Gather changes**: Run `git diff`, `git diff --staged`, and `git status` to identify what changed.

2. **Design Review** -- for each changed/added file, check:

   - **Layer correctness**: Core layer files use P2996 reflection; tools layer files marked C++17 must NOT include P2996 headers
   - **API design**: Public functions are `consteval` (not `constexpr`) for signature generation; return `FixedString<N>` with exact length
   - **Flattening invariant**: Signatures must erase field names and inheritance -- only byte-level identity preserved
   - **Dual-path consistency**: If `compute_has_padding` (bitmap) was changed, `sig_has_padding` (string parser) must also be updated, and vice versa
   - **Opaque registration order**: Opaque types must be registered before any signature generation encounters them
   - **Virtual inheritance**: Must be rejected, never silently handled
   - **Header-only**: No new .cpp files in the library (only in test/)

3. **Implementation Review** -- for each changed/added file, check:

   - **Correctness**: Offset calculations account for alignment; padding bytes are correctly detected
   - **Platform safety**: Type sizes that vary across platforms (e.g., `long`) have proper guards in `type_map.hpp`
   - **No duplication**: Logic is not repeated between files; shared types live in `sig_types.hpp` or `fwd.hpp`
   - **Constexpr limits**: Deep reflection may hit step limits -- check if `BOOST_TYPELAYOUT_CONSTEXPR_STEPS` needs bumping
   - **FixedString sizing**: N must be exactly the content length -- verify no off-by-one in string concatenation
   - **Array recursion**: `type_has_opaque` and `compute_has_padding` must recurse into array elements via `std::remove_all_extents_t`

4. **Test Coverage Review**:

   - New functionality has corresponding `static_assert` tests
   - Changed signature format is reflected in ALL test files that contain hardcoded signatures
   - P2996 tests use correct compile flags and 120s timeout
   - C++17 tests do not accidentally include P2996 headers

5. **Output a structured report**:

```
=== Code Review ===

--- Design ---
[OK] Layer correctness: tools/new_tool.hpp is C++17-only, no P2996 includes
[ISSUE] Dual-path desync: compute_has_padding updated but sig_has_padding not updated

--- Implementation ---
[OK] Offset calculations correct in signature_impl.hpp
[ISSUE] FixedString size off-by-one in new concatenation at signature_impl.hpp:142

--- Test Coverage ---
[OK] New type has static_assert in test_layout_traits.cpp
[ISSUE] Missing test for edge case: empty struct with [[no_unique_address]]

=== Summary: X OK, Y ISSUE ===
```

## Key Files to Cross-Check

| When this changes... | Also verify... |
|---------------------|----------------|
| `detail/signature_impl.hpp` | `layout_traits.hpp`, all `test_*.cpp` with signatures |
| `detail/type_map.hpp` | `detail/reflect.hpp`, `test_layout_traits.cpp` |
| `layout_traits.hpp` (bitmap) | `tools/safety_level.hpp` (parser), `test_padding_precision.cpp` |
| `tools/safety_level.hpp` | `test_rt_padding.cpp`, `test_compat_check.cpp` |
| `opaque.hpp` | `test_opaque.cpp`, any file using the registered type |
| `CMakeLists.txt` | AGENTS.md test table |

## Important Notes

- Always read the actual changed code, not just the diff summary
- For signature format changes, check `/modify-signature-format` for the full impact list
- Design issues are higher priority than implementation issues -- flag them first
- If unsure whether a change is correct, use `/sig-check <Type>` to verify actual output
