## 1. Header files
- [x] 1.1 `compat_check.hpp` — rewrite comments to terse style
- [x] 1.2 `sig_export.hpp` — trim file header and doxygen
- [x] 1.3 `compat_auto.hpp` — trim file header and section markers
- [x] 1.4 `platform_detect.hpp` — replace decorative separators, trim headers
- [x] 1.5 `sig_types.hpp` — trim file header
- [x] 1.6 `detail/foreach.hpp` — trim file header

## 2. Example files
- [x] 2.1 `example/compat_check.cpp` — minimal comments, drop academic jargon
- [x] 2.2 `example/README.md` — remove marketing tone, neutral style

## 3. Verification
- [x] 3.1 Run `openspec validate reduce-ai-style --strict --no-interactive`