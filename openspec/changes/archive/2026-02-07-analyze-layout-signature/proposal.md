# Change: Analyze Layout signature correctness and completeness

## Why
The Definition signature layer has been thoroughly audited (analyze-definition-signature).
Now we need an equivalent systematic audit of the Layout signature (Layer 1) to verify
correctness, completeness, and the reliability guarantee: layout_sig(T) == layout_sig(U) implies memcmp-compatible(T, U).

## What Changes
- Analysis-only proposal: no code changes expected unless defects are found
- Audit every encoding dimension of the Layout signature engine
- Verify the V1 reliability guarantee holds for all supported type categories
- Check test coverage for Layout-specific behaviors

## Impact
- Affected specs: signature
- Affected code: `signature_detail.hpp` (Part 3 + Part 4), `signature.hpp`, `fwd.hpp`
