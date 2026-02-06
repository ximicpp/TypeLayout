## MODIFIED Requirements

### Requirement: Minimal Repository Structure
The repository SHALL contain only the following top-level items:
- `include/` — header-only library source
- `test/` — single test file (`test_two_layer.cpp`)
- `openspec/` — spec-driven development metadata
- `.github/` — CI configuration
- `CMakeLists.txt` — minimal build config (tests only)
- `README.md` — project documentation
- `LICENSE` / `LICENSE_1_0.txt` — license files
- `AGENTS.md` — AI assistant instructions
- `.gitignore`

All other directories and files SHALL be removed.

#### Scenario: Clean repository
- **WHEN** a developer clones the repository
- **THEN** only core headers, one test, CI config, and README are present
- **THEN** no outdated documentation, examples, tools, benchmarks, or scripts exist

## REMOVED Requirements

### Requirement: Documentation Directory
**Reason**: All content references outdated v1.0 APIs. Will be recreated from scratch when needed.
**Migration**: Removed entirely.

### Requirement: Example Programs
**Reason**: Reference non-core hash APIs. Will be recreated with minimal core API.
**Migration**: Removed entirely.

### Requirement: Build Tools and Scripts
**Reason**: Tools reference non-core APIs. Scripts are outdated.
**Migration**: Removed entirely.

### Requirement: Benchmark Suite
**Reason**: Auxiliary artifact, not part of core functionality.
**Migration**: Removed entirely.

### Requirement: Boost.Build Support
**Reason**: Root Jamfile and cmake/ package config are premature.
**Migration**: Removed entirely. CMake kept for tests only.