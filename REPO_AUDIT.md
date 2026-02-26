# Repository Audit Report

> Generated for the TypeLayout cleanup/documentation-merge phase.
> Each item has a verdict: **KEEP** / **DELETE** / **MERGE** / **REFACTOR** / **ARCHIVE**

---

## 1. Source Code (`include/`) -- 1,957 lines total

All headers are actively used. No dead code at the file level.

| File | Lines | Verdict | Notes |
|------|------:|:-------:|-------|
| `typelayout.hpp` (umbrella) | 9 | KEEP | Single-include convenience header |
| `boost/typelayout/typelayout.hpp` | 14 | KEEP | Namespace-level umbrella |
| `config.hpp` | 31 | KEEP | Feature-detection macros |
| `fwd.hpp` | 35 | KEEP | Forward declarations, enums |
| `fixed_string.hpp` | 152 | KEEP | Core constexpr string type |
| `signature.hpp` | 51 | KEEP | Public API (4 functions) |
| `opaque.hpp` | 121 | KEEP | TYPELAYOUT_OPAQUE_* macros |
| `detail/reflect.hpp` | 94 | KEEP | P2996 abstraction layer |
| `detail/signature_impl.hpp` | 268 | KEEP | Recursive signature generation |
| `detail/type_map.hpp` | 310 | KEEP | Type-to-tag mapping |
| `tools/compat_auto.hpp` | 97 | **REFACTOR** [DONE] | Replaced 7-level hand-rolled recursion with `FOR_EACH_CTX`; now supports up to 32 platforms |
| `tools/compat_check.hpp` | 311 | KEEP | Minor: `sig_match` alias is redundant (same as `layout_match`) but harmless |
| `tools/detail/foreach.hpp` | 67 | KEEP | Generic FOR_EACH utility |
| `tools/platform_detect.hpp` | 98 | KEEP | Platform/arch/compiler detection |
| `tools/sig_export.hpp` | 264 | KEEP | Signature export codegen |
| `tools/sig_types.hpp` | 35 | KEEP | Type list for export |

### Action Items -- Source Code

1. **`compat_auto.hpp` REFACTOR** [DONE]: Replaced `EACH_1`..`EACH_7` with `TYPELAYOUT_DETAIL_FOR_EACH_CTX` in `foreach.hpp`. Platform limit raised from 7 to 32.

---

## 2. Tests (`test/`) -- 6 files

| File | Verdict | Notes |
|------|:-------:|-------|
| `test_sig_export.cpp` | KEEP | Tests signature export |
| `test_two_layer.cpp` | KEEP | Tests Layout vs Definition |
| `test_opaque.cpp` | KEEP | Tests opaque type macros |
| `test_util.hpp` | KEEP | Shared test utilities |
| `test_compat_check.cpp` | KEEP | Tests cross-platform checking |
| `test_fixed_string.cpp` | KEEP | Tests FixedString |

No issues. Test coverage maps to all core features.

---

## 3. Examples (`example/`) -- 3 source files + 3 sigs + 2 docs

| File | Verdict | Notes |
|------|:-------:|-------|
| `cross_platform_check.cpp` | KEEP | Main demo |
| `compat_check.cpp` | KEEP | Alternative demo |
| `sigs/*.sig.hpp` (3 files) | KEEP | Pre-generated platform signatures |
| `README.md` (260 lines) | **MERGE** [DONE] | Merged into `docs/quickstart.md`; `example/README.md` is now a short pointer |
| `BEST_PRACTICES.md` (569 lines) | **MERGE** [DONE] | Moved to `docs/best-practices.md` |

### Action Items -- Examples

2. **Merge `example/README.md`** into `docs/quickstart.md` [DONE]: Full pipeline tutorial now in quickstart; example/README.md is a short pointer.
3. **Move `example/BEST_PRACTICES.md`** to `docs/best-practices.md` [DONE].

---

## 4. Build / CI / Infrastructure

| File | Lines | Verdict | Notes |
|------|------:|:-------:|-------|
| `CMakeLists.txt` | ~90 | KEEP | Primary build system |
| `cmake/TypeLayoutCompat.cmake` | ~40 | KEEP | Compat-check CMake module |
| `.gitignore` | ~15 | KEEP | Correctly ignores build dirs |
| `LICENSE_1_0.txt` | ~25 | KEEP | Boost license |
| `AGENTS.md` | 5 | KEEP | Project rules (no-emoji, etc.) |
| `.github/PULL_REQUEST_TEMPLATE.md` | ~20 | KEEP | Standard |
| `.github/ISSUE_TEMPLATE/*.md` (2) | ~20 each | KEEP | Standard |
| `.github/docker/Dockerfile.p2996` | ~30 | KEEP | P2996 build env |

### GitHub Workflows

| Workflow | Lines | Verdict | Notes |
|----------|------:|:-------:|-------|
| `ci.yml` | 42 | KEEP | Core CI |
| `compat-check.yml` | 328 | KEEP | Cross-platform reusable workflow |
| `docker-build.yml` | 85 | KEEP | Weekly Docker image build |
| `b2.yml` | 71 | **DELETE** [DONE] | Deleted. No B2 build files existed. |
| `docs.yml` | 77 | **DELETE** [DONE] | Deleted. Referenced non-existent `doc/` directory. |
| `static.yml` | 43 | KEEP *or* **DELETE** | Deploys entire repo as GitHub Pages (raw files); if `docs.yml` is the intended docs pipeline, `static.yml` is redundant. If `docs.yml` is deleted (as recommended), keep `static.yml` only if raw-repo deployment is intended. |

### Action Items -- CI

4. **Delete `b2.yml`** [DONE]: Deleted.
5. **Delete `docs.yml`** [DONE]: Deleted.
6. **Evaluate `static.yml`**: Decide if raw-repo GitHub Pages deployment is desired. If not, delete it too.

---

## 5. Tools (`tools/`)

| File | Verdict | Notes |
|------|:-------:|-------|
| `typelayout-compat` (shell script) | KEEP | CLI wrapper for compat checks |
| `platforms.conf` | KEEP | Platform configuration |
| `README.md` (360 lines) | KEEP | Tool usage docs (self-contained, not duplicated elsewhere) |

No issues.

---

## 6. Root-Level Documentation

| File | Lines | Verdict | Notes |
|------|------:|:-------:|-------|
| `README.md` | 207 | KEEP | Project entry point. Clean and well-structured. |
| `PROOFS.md` | 1,014 | **MERGE candidate** | Overlaps with `docs/paper/sec4-formal.md` (230 lines). See analysis below. |
| `APPLICATIONS.md` | 1,145 | **MERGE candidate** [DONE] | Moved to `docs/applications.md` |
| `CONFERENCE_PROPOSAL.md` | 180 | KEEP (or ARCHIVE) [DONE] | Moved to `docs/conference-proposal.md` |

### Documentation Overlap Analysis

**`PROOFS.md` (root) vs `docs/paper/sec4-formal.md`:**
- `PROOFS.md`: 1,014 lines. Full formal proofs (S1-S7) in Chinese+English.
- `sec4-formal.md`: 230 lines. Condensed English-only version for the paper.
- **Overlap**: ~40%. `sec4-formal.md` is a strict subset of `PROOFS.md` content.
- **Recommendation**: KEEP both. They serve different audiences (full proofs vs paper section). Add a cross-reference note in each.

**`APPLICATIONS.md` (root) vs `docs/paper/sec6-evaluation.md` + `docs/analysis/`:**
- `APPLICATIONS.md`: 1,145 lines. 6 scenarios + formal correctness audit. Chinese+English.
- `sec6-evaluation.md`: 181 lines. Condensed evaluation for the paper.
- `docs/analysis/ZERO_SERIALIZATION_TRANSFER.md`: 956 lines. Deep-dive on zero-copy.
- `docs/analysis/CROSS_PLATFORM_COLLECTION.md`: 318 lines. Per-type compatibility matrix.
- **Overlap**: `sec6-evaluation.md` summarizes `APPLICATIONS.md` Section 7-8. The `docs/analysis/` files are extensions, not duplicates.
- **Recommendation**: KEEP all. Add cross-references. Consider moving `APPLICATIONS.md` to `docs/applications.md` to reduce root clutter.

### Action Items -- Documentation

7. **Move `APPLICATIONS.md`** to `docs/applications.md` [DONE].
8. **Move `CONFERENCE_PROPOSAL.md`** to `docs/conference-proposal.md` [DONE].
9. **Keep `PROOFS.md` at root** (it is referenced by `README.md` directly).
10. Add cross-reference headers between related documents.

---

## 7. `docs/` Directory

| File | Lines | Verdict | Notes |
|------|------:|:-------:|-------|
| `quickstart.md` | ~270 | KEEP [DONE] | Merged with `example/README.md` content; now includes full pipeline tutorial |
| `api-reference.md` | 390 | KEEP | Comprehensive API docs |
| `migration-guide.md` | 106 | KEEP | Documents v2 header path changes |
| `paper/sec1-introduction.md` | 173 | KEEP | Paper section |
| `paper/sec2-background.md` | 141 | KEEP | Paper section |
| `paper/sec3-system.md` | 277 | KEEP | Paper section |
| `paper/sec4-formal.md` | 230 | KEEP | Paper section (condensed PROOFS.md) |
| `paper/sec5-toolchain.md` | 154 | KEEP | Paper section |
| `paper/sec6-evaluation.md` | 181 | KEEP | Paper section |
| `paper/sec7-related.md` | 120 | KEEP | Paper section |
| `paper/sec8-conclusion.md` | 100 | KEEP | Paper section |
| `analysis/ZERO_SERIALIZATION_TRANSFER.md` | 956 | KEEP | Deep analysis |
| `analysis/CROSS_PLATFORM_COLLECTION.md` | 318 | KEEP | Platform comparison |

Total docs: ~3,280 lines (excluding root `.md` files). Well-organized. No deletions needed.

---

## 8. `openspec/` Directory -- **PRIMARY CLEANUP TARGET**

| Component | Size | Files | Verdict | Notes |
|-----------|------|------:|:-------:|-------|
| `project.md` | 534 lines | 1 | KEEP | Project context and spec |
| `config.yaml` | small | 1 | KEEP | OpenSpec config |
| `specs/` (7 spec files) | ~1,282 lines | 7 | **REVIEW** | 5 of 7 have "TBD" purpose from auto-archiving |
| `changes/archive/` | **2.5 MB** | **511** | **ARCHIVE / DELETE** [DONE] | Deleted. Historical change logs recovered from git history if needed. |

### `openspec/specs/` Detail

| Spec | Lines | Status | Verdict |
|------|------:|--------|:-------:|
| `signature/spec.md` | 589 | Active, detailed | KEEP |
| `cross-platform-compat/spec.md` | 179 | Active | KEEP |
| `documentation/spec.md` | 388 | Active | KEEP |
| `ci/spec.md` | 57 | TBD stub | **DELETE or FILL** |
| `build-system/spec.md` | 22 | TBD stub | **DELETE or FILL** |
| `project-setup/spec.md` | 17 | TBD stub | **DELETE or FILL** |
| `repository-structure/spec.md` | 30 | TBD stub | **DELETE or FILL** |

### `openspec/changes/archive/` -- 511 Files, 2.5 MB

This is the single largest contributor to repository bloat. These are historical
change-tracking markdown files generated by the OpenSpec tooling. Options:

| Option | Pros | Cons |
|--------|------|------|
| **A. Keep as-is** | Full traceability | 511 files, 2.5 MB of markdown bloat; clutters `git log` |
| **B. Compress to single file** | Preserves history, reduces file count to 1 | Loses per-change file granularity |
| **C. Move to separate branch** | Keeps main branch clean; history still in repo | Extra branch maintenance |
| **D. Delete entirely** | Cleanest main branch | Loses history (recoverable from git log) |

**Recommendation**: Option **C** (move to `archive` branch) or **D** (delete; git history preserves content). The 511 files provide no value for active development.

### Action Items -- openspec

11. **Archive or delete `openspec/changes/archive/`** [DONE]: Deleted (511 files, 2.5 MB).
12. **Fill or delete TBD spec stubs** (`ci`, `build-system`, `project-setup`, `repository-structure`).

---

## 9. Summary: Action Priority Matrix

| # | Action | Impact | Effort | Priority |
|---|--------|--------|--------|:--------:|
| 11 | Delete/archive `openspec/changes/archive/` | -2.5 MB, -511 files | Low | **P0** [DONE] |
| 4 | Delete `b2.yml` workflow | Remove dead CI | Low | **P0** [DONE] |
| 5 | Delete `docs.yml` workflow | Remove dead CI | Low | **P0** [DONE] |
| 12 | Fill or delete TBD spec stubs (4 files) | Reduce noise | Low | **P1** [DONE] |
| 1 | Refactor `compat_auto.hpp` ASSERT macros | -20 lines, +25 platform limit | Medium | **P1** [DONE] |
| 3 | Move `BEST_PRACTICES.md` to `docs/` | Better organization | Low | **P2** [DONE] |
| 2 | Merge `example/README.md` into `docs/quickstart.md` | Reduce duplication | Low | **P2** [DONE] |
| 7 | Move `APPLICATIONS.md` to `docs/` | Cleaner root | Low | **P2** [DONE] |
| 8 | Move `CONFERENCE_PROPOSAL.md` to `docs/` | Cleaner root | Low | **P2** [DONE] |
| 6 | Evaluate `static.yml` workflow | Potential dead CI | Low | **P2** |
| 10 | Add cross-reference headers | Better navigation | Low | **P3** |

---

## 10. Quantitative Summary

| Category | Files | Lines | Keep | Delete | Merge/Move | Refactor |
|----------|------:|------:|-----:|-------:|-----------:|---------:|
| Source (`include/`) | 16 | 1,957 | 15 | 0 | 0 | 1 |
| Tests (`test/`) | 6 | ~600 | 6 | 0 | 0 | 0 |
| Examples (`example/`) | 8 | ~900 | 6 | 0 | 2 | 0 |
| Docs (`docs/`) | 13 | ~3,280 | 13 | 0 | 0 | 0 |
| Root docs (`.md`) | 5 | ~2,550 | 2 | 0 | 2-3 | 0 |
| CI/workflows | 6 | ~646 | 3-4 | **2-3** | 0 | 0 |
| Tools | 3 | ~400 | 3 | 0 | 0 | 0 |
| Build/infra | ~8 | ~200 | 8 | 0 | 0 | 0 |
| openspec (excl. archive) | ~10 | ~1,300 | 5-6 | **4** (stubs) | 0 | 0 |
| openspec/changes/archive | **511** | **~10,000+** | 0 | **511** | 0 | 0 |
| **Total** | **~586** | **~21,800+** | ~62 | **~518** | ~4 | 1 |

**Net result of full cleanup**: Remove ~518 files (~2.5 MB), delete 2-3 dead CI workflows, consolidate 2-4 documentation files. Source code and tests are already lean and require only one minor refactor.
