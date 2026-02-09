# Change: Reduce AI-style verbosity in code and comments

## Why
Code comments and the example README accumulated AI-characteristic patterns
during development: over-explanatory doxygen, decorative separator lines,
marketing-style feature lists, and formal jargon that real developers
wouldn't write. This makes the codebase feel generated rather than authored.

## What Changes
1. **`compat_check.hpp`** — rewrite doxygen to terse developer style; drop
   "Semantic alias:" phrasing; trim enum member comments to bare minimum.
2. **`sig_export.hpp`** — shorten file header; drop "Phase 1 of the
   two-phase..." wording; tighten doxygen on private helpers.
3. **`compat_auto.hpp`** — shorten file header and section comments.
4. **`platform_detect.hpp`** — replace `=====` separator lines with plain
   `// ---` or nothing; drop redundant section header comments.
5. **`sig_types.hpp`** — trim the file header.
6. **`detail/foreach.hpp`** — trim the file header.
7. **`example/compat_check.cpp`** — trim to minimal; drop C1/C2/A1 jargon
   from inline comments.
8. **`example/README.md`** — remove "100% pure C++", "Why This Matters"
   marketing section; trim ZST quick-reference table; make tone neutral.

## Impact
- Affected specs: `cross-platform-compat` (comment-style only)
- Affected code: all 5 tool headers, `detail/foreach.hpp`, example `.cpp`, example `README.md`
- No API changes. No behaviour changes.
