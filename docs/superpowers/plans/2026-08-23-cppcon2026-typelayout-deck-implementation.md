# CppCon 2026 TypeLayout Deck Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Produce a fully editable, source-template-faithful 61-slide CppCon 2026 deck that implements the approved signature-centered argument and passes slide-by-slide visual, structural, and technical review.

**Architecture:** Treat the user-provided PPTX as the only visual design source. Inspect all 55 source slides, create a validated 61-slide clone map, edit inherited elements through `@oai/artifact-tool`, and build the deck in five independently rendered content batches before a final evidence and fidelity gate. Keep every intermediate in an isolated build directory and export one new PPTX without overwriting the source attachment.

**Tech Stack:** PowerShell, JavaScript ES modules, `@oai/artifact-tool`, presentation template-following helpers, presentation render/overflow helpers, Git for this plan only.

**Spec:** `docs/superpowers/specs/2026-08-23-cppcon2026-typelayout-deck-design.md`

## Global Constraints

- Source attachment: `C:\Users\gzsufanchen\.codex\codex-remote-attachments\01a02c80-5faa-7020-a872-e6628f1ff70b\206C9360-0FCC-47D3-BB2C-F376B31225AE\1-cppcon2026_typelayout_36slide_main_argument_map_editorial_no_audio_optimized.pptx`.
- Source-deck inventory expectation: 55 slides; inspect every source slide before authoring.
- Build directory: `E:\workspace\TypeLayout\.codex_tmp\cppcon2026_typelayout_final`.
- Final deliverable: `E:\workspace\TypeLayout\cppcon2026_typelayout_signature_centered.pptx`.
- Never overwrite the source attachment or modify the existing user-authored PPTX files in the repository.
- Use the source deck's master, layouts, theme, typography, footer, page numbering, palette, and technical editorial style.
- Every output slide must duplicate one mapped source slide and edit inherited elements in place; do not rebuild slides from screenshots, palette samples, or a different template.
- Preserve the master → layout → slide hierarchy. Edit masters or layouts only for an intentional repeated change whose complete descendant set has been inspected.
- Use `@oai/artifact-tool` from JavaScript ES modules for all PPTX import, editing, inspection, rendering, and export. Do not use `python-pptx` or the legacy Python artifact API.
- Run `mark_artifact_operation_started.mjs` exactly once, with operation kind `edit`, immediately before the first authoring command.
- Load workspace dependencies first and use exactly the returned `RUNTIME_NODE`, `RUNTIME_NODE_MODULES`, and `RUNTIME_BIN_DIR` values. Do not install or discover alternate runtimes.
- Keep generated plans, source notes, audit notes, QA ledgers, JSON maps, renderings, and `.mjs` builders under the build directory. Only the final PPTX may be written outside it.
- Main deck: slides 1–45. Appendix: slides 46–61.
- Main profile: ordinary object copy, zero fixup, source-address-independent bytes, finite declared type and build sets, trusted producer objects.
- Relocation remains appendix-only and never shares an unqualified permit with ordinary copy.
- Use the terms Evidence, Admission, Agreement, EdgePass, and Permit exactly as defined in the spec.
- Slides 1–45 must read as seven cumulative stages: why evidence is needed → how one build produces evidence → how the two gates decide one edge → how CI closes the finite contract → how the model authorizes one useful raw-byte type set and rejects two nearby alternatives → how the resulting Permit is bounded → a final recap of the problem, method, and actionable takeaway. Slides 35–39 form Stage 5, slides 40–42 form the brief Stage 6, and slides 43–45 form the explicit Stage 7 conclusion.
- Reserve `Permit` for the per-key result closed over the complete declared build graph. Slide 28 may establish `EdgePass`, but it must not present one edge as the final Permit.
- Treat CI provenance as evidence-input validation, not as a third compatibility gate beside Admission and Agreement.
- Reconcile the Sched phrase `architecture and endianness` explicitly: the current signature prefix records pointer width and endianness, leaf encodings carry further representation facts, and exact compiler/target identity belongs to provenance. Never call the prefix a complete CPU/ISA identity.
- Keep only details that close the current proof gap, prevent a consequential misunderstanding, or enable the next inference; route inventories, variants, and optimizations to notes or the appendix.
- Ordinary copy requires both `std::is_trivially_copyable_v<T>` and `is_byte_copy_safe_v<T>`.
- Do not display `TYPELAYOUT_ASSERT_TRANSFER_SAFE`; it does not exist at repository baseline `201f06f`.
- Do not imply that `TYPELAYOUT_ASSERT_COMPAT` joins entries by contract key; the current compile-time helper compares array positions.
- Treat a skipped or absent required build as a missing fact, never as a passing node.
- Treat the portable-capture C++/CMake/CI design in spec section 11.1 as deferred work. This deck-authoring plan may describe it and reserve source mappings for it, but must not create or modify the planned implementation files.
- Visible content must be audience-facing. Production notes, timing scaffolds, source-selection explanations, and QA language belong outside the slide canvas.
- Preserve source font family, size, weight, line spacing, paragraph spacing, insets, alignment, and vertical anchor. If copy does not fit, shorten it or remap the slide; do not shrink inherited typography.
- Color meaning must also be written as `PASS`, `FAIL`, `MATCH`, `DIFFER`, `EDGE PASS`, `PERMIT`, or `REJECT`.
- Every externally sourced non-trivial claim must have a `[Sources]` block in speaker notes.
- Required primary sources include P2996R12 and P3687R1 for C++26 reflection, N5032 or the current draft `[basic.types]` wording for ordinary byte copying, P4197R0 for C++29 relocation status, Apple's ARM64 ABI documentation for Apple `long double`, the x86-64 psABI for Linux `long double`, and the repository artifacts/API files named in Task 9.
- Render every final slide, inspect every slide individually at full size, and fix unintended overlap, clipping, wrapping, broken connectors, empty structural placeholders, inconsistent footers/page markers, and content/evidence mismatches.
- Existing unrelated untracked files remain untouched. Intermediate artifact files are not committed; the final PPTX is not committed unless the user explicitly asks.

## File Structure

| Path | Responsibility |
|---|---|
| `docs/superpowers/specs/2026-08-23-cppcon2026-typelayout-deck-design.md` | Approved narrative and slide-level design contract |
| `docs/superpowers/plans/2026-08-23-cppcon2026-typelayout-deck-implementation.md` | This execution plan |
| `.codex_tmp/cppcon2026_typelayout_final/source.pptx` | Read-only build copy of the user attachment |
| `.codex_tmp/cppcon2026_typelayout_final/source-notes.txt` | Source/provenance ledger for the attachment, repository artifacts, papers, and ABI references |
| `.codex_tmp/cppcon2026_typelayout_final/template-inspect/` | Complete source renders, layout exports, media, fonts, manifest, and NDJSON inventory |
| `.codex_tmp/cppcon2026_typelayout_final/template-audit.txt` | Reusable source patterns, inherited structure, typography, placeholder, and insertion rules |
| `.codex_tmp/cppcon2026_typelayout_final/template-frame-map.json` | Validated mapping from all 61 output slides to source slides and source elements |
| `.codex_tmp/cppcon2026_typelayout_final/deviation-log.txt` | Intentional template deviations with affected output slides and reasons |
| `.codex_tmp/cppcon2026_typelayout_final/template-starter.pptx` | 61-slide deck created only by duplicating mapped source slides |
| `.codex_tmp/cppcon2026_typelayout_final/template-starter-preview/` | Starter renders used to confirm clone order and template fidelity |
| `.codex_tmp/cppcon2026_typelayout_final/template-starter-layout/` | Starter layout JSON used by the final fidelity comparison |
| `.codex_tmp/cppcon2026_typelayout_final/edit-plan.json` | Output-slide roles mapped to exact starter-deck element anchors and actions |
| `.codex_tmp/cppcon2026_typelayout_final/slide-content.mjs` | Audience-facing copy, source-slide mapping, edit operations, and note sources for slides 1–61 |
| `.codex_tmp/cppcon2026_typelayout_final/deck-edit-schema.mjs` | Validation and application interfaces shared by the content and builder |
| `.codex_tmp/cppcon2026_typelayout_final/build-deck.mjs` | Imports the starter, applies content through a requested slide, renders evidence, and exports a draft |
| `.codex_tmp/cppcon2026_typelayout_final/validate-content.mjs` | Checks slide numbering, mapping, titles, edit targets, vocabulary, and source-note coverage |
| `.codex_tmp/cppcon2026_typelayout_final/qa-content.mjs` | Inspects an exported PPTX and checks the final visible/notes contract |
| `.codex_tmp/cppcon2026_typelayout_final/qa-ledger.txt` | One line per output slide with content, visual, overflow, placeholder, and source status |
| `.codex_tmp/cppcon2026_typelayout_final/final-candidate.pptx` | Fully authored candidate kept inside the build directory until all gates pass |
| `cppcon2026_typelayout_signature_centered.pptx` | Delivered editable PPTX |

## Deferred Code-Demo Work Package — Record Only

This work package records the implementation required to turn Stage 5 into live three-build repository evidence. It is deliberately outside the executable scope of the current deck-authoring plan. Do not modify C++, CMake, tests, signature fixtures, or workflows until the user separately authorizes code implementation.

When authorized, implement in this order:

1. Add `example/portable_capture_types.hpp` with `MeasurementSample { uint64_t id; int64_t value_microunits; }`, `CaptureTrailer`, and `CaptureBlock`, reusing the existing fixed-width `PacketHeader`. Add `UnsafeWithPointer` as the working sample plus a cached `std::byte*`, and retain the opening `Measurement { uint64_t id; long double value; }`. Keep `R_capture` limited to the four positive types. Add build-local assertions for `CHAR_BIT == 8`, trivial copyability, ordinary-copy Admission, every expected member offset, every expected size, and absence of internal/tail padding at each recursive level; an optional `has_unique_object_representations` assertion is fixture validation rather than the compatibility gate.
2. Add `example/portable_capture_io.cpp` to construct one completely initialized deterministic block, write and read its complete object representation without field-by-field encoding, and verify every logical value after the round trip.
3. Add `example/portable_capture_export.cpp` and `example/portable_capture_check.cpp`. Export the same positive keys independently on Linux x86-64/GCC 16, Linux x86-64/Clang P2996, and Apple ARM64/Clang P2996; require all three artifacts; and establish Admission on every node plus Agreement on every required pairwise edge for every positive key.
4. Add `example/portable_capture_negative_export.cpp` and `example/portable_capture_negative_check.cpp` with a separate test-evidence registry. Require complete evidence before checking verdicts. Verify `UnsafeWithPointer` has Admission FAIL on every node and Agreement MATCH on every edge. Verify `Measurement` has Admission PASS on every node, Linux GCC ↔ Linux Clang MATCH, and both Linux ↔ Apple edges DIFFER. Never add either candidate to the production `R_capture` allowlist, and never accept missing evidence or an unintended extra failure as a successful negative test.
5. Extend `CMakeLists.txt` with isolated export, raw-I/O, aggregate, and expected-rejection tests. Keep positive and negative generated headers in distinct output/include directories and separate translation units because both exporters use the same platform basename, include guard, and namespace shape. Reuse current exporter, generated `TypeEntry`, Admission primitives, and `CompatReporter`; do not invent a public transfer-permit API.
6. Extend `.github/workflows/compat-pipeline.yml` so every producer uploads distinct positive/negative `.sig.hpp` artifacts, deterministic `capture.bin`, and provenance binding. Pin reflection toolchain images by immutable digest. An always-running closure/status job must reject a missing node, compare all required keys and edges explicitly, compare the three raw fixtures, and report `INCOMPLETE` rather than `PERMIT` when the Apple ARM64 job does not run.
7. Retain one successful complete-run artifact set for the deck's speaker-note provenance when the code work is implemented. The Stage 5 content below is already written against that completed-demo state.

The future code-demo success condition is exact rather than merely “the workflow is green”:

```text
R_capture: four per-key Permits over three nodes and every required edge
UnsafeWithPointer: complete evidence; Admission FAIL everywhere; Agreement MATCH everywhere
Measurement: complete evidence; Admission PASS everywhere; Linux↔Linux MATCH; both Linux↔Apple edges DIFFER
capture.bin: identical across all three fully attributed producers
```

## External Prerequisite for the Completed-Demo Slides

The current plan records but does not execute the code-demo work package. A separately authorized code task must create the six `portable_capture_*` sources, CMake/CTest integration, CI producers/checkers, and one retained complete three-build artifact set before Tasks 7–10 execute. Tasks 1–6 may proceed independently; Task 8 is blocked specifically for appendix slide 59, while Tasks 9–10 require the complete demo sources and artifacts for final accuracy review and delivery. This checkpoint stays in the implementation plan and does not appear in the audience-facing Stage 5 chain.

---

## Deck Authoring Tasks

### Task 1: Establish the isolated artifact workspace and audit all 55 source slides

**Files:**

- Create: `.codex_tmp/cppcon2026_typelayout_final/source.pptx`
- Create: `.codex_tmp/cppcon2026_typelayout_final/source-notes.txt`
- Create: `.codex_tmp/cppcon2026_typelayout_final/template-inspect/`
- Create: `.codex_tmp/cppcon2026_typelayout_final/template-audit.txt`
- Read: `docs/superpowers/specs/2026-08-23-cppcon2026-typelayout-deck-design.md`

**Interfaces:**

- Consumes: the exact source attachment, approved spec, and dependency paths returned by `load_workspace_dependencies`.
- Produces: a complete 55-slide structural/visual inventory and the only permitted source-slide pattern catalog for later tasks.

- [ ] **Step 1: Verify the fixed build directory is absent or empty**

Run:

```powershell
$tmpDir = 'E:\workspace\TypeLayout\.codex_tmp\cppcon2026_typelayout_final'
if (Test-Path -LiteralPath $tmpDir) {
    $items = @(Get-ChildItem -LiteralPath $tmpDir -Force)
    if ($items.Count -ne 0) { throw "Build directory is not empty: $tmpDir" }
}
```

Expected: exit 0. If non-empty, stop and inspect ownership; do not delete or reuse unknown files.

- [ ] **Step 2: Load the presentation workspace dependencies**

Call `load_workspace_dependencies`, then bind the three returned absolute paths as command-scoped `RUNTIME_NODE`, `RUNTIME_NODE_MODULES`, and `RUNTIME_BIN_DIR`. Verify all three paths exist.

Expected: one Node executable path, one module directory, and one binary directory; no package installation.

- [ ] **Step 3: Create the build directory and module junction**

Run with the values from Step 2:

```powershell
$tmpDir = 'E:\workspace\TypeLayout\.codex_tmp\cppcon2026_typelayout_final'
New-Item -ItemType Directory -Path $tmpDir -Force | Out-Null
New-Item -ItemType Junction -Path "$tmpDir\node_modules" -Target $env:RUNTIME_NODE_MODULES | Out-Null
```

Expected: `$tmpDir\node_modules` resolves exactly to `RUNTIME_NODE_MODULES`.

- [ ] **Step 4: Copy the attachment into the isolated workspace**

Run:

```powershell
$sourceAttachment = 'C:\Users\gzsufanchen\.codex\codex-remote-attachments\01a02c80-5faa-7020-a872-e6628f1ff70b\206C9360-0FCC-47D3-BB2C-F376B31225AE\1-cppcon2026_typelayout_36slide_main_argument_map_editorial_no_audio_optimized.pptx'
$sourceCopy = 'E:\workspace\TypeLayout\.codex_tmp\cppcon2026_typelayout_final\source.pptx'
Copy-Item -LiteralPath $sourceAttachment -Destination $sourceCopy
Get-FileHash -Algorithm SHA256 -LiteralPath $sourceAttachment,$sourceCopy
```

Expected: both SHA-256 values are identical.

- [ ] **Step 5: Record source provenance**

Create `source-notes.txt` with the source attachment path, SHA-256, repository baseline `201f06f8a9dd20323ffd8af836c545ef2380e82d`, approved spec path, and the official/local sources enumerated in Task 9.

- [ ] **Step 6: Inspect the full template deck**

Run:

```powershell
& $env:RUNTIME_NODE 'C:\Users\gzsufanchen\.codex\plugins\cache\openai-primary-runtime\presentations\26.819.11345\skills\presentations\template_following_scripts\inspect_template_deck.mjs' `
  --workspace 'E:\workspace\TypeLayout\.codex_tmp\cppcon2026_typelayout_final' `
  --pptx 'E:\workspace\TypeLayout\.codex_tmp\cppcon2026_typelayout_final\source.pptx'
```

Expected: exit 0 with `template-inspect.ndjson`, `template-manifest.json`, 55 source-slide PNGs, 55 layout JSON files, font evidence, and extracted media.

- [ ] **Step 7: Verify the source inventory count**

Run:

```powershell
$renderCount = @(Get-ChildItem -LiteralPath 'E:\workspace\TypeLayout\.codex_tmp\cppcon2026_typelayout_final\template-inspect\source-slides' -Filter '*.png').Count
$layoutCount = @(Get-ChildItem -LiteralPath 'E:\workspace\TypeLayout\.codex_tmp\cppcon2026_typelayout_final\template-inspect\layouts' -Filter '*.json').Count
if ($renderCount -ne 55 -or $layoutCount -ne 55) { throw "Expected 55 renders and layouts; got $renderCount and $layoutCount" }
```

Expected: exit 0.

- [ ] **Step 8: Inspect every source slide at full size**

Open all 55 source PNGs individually. Record the source slide's narrative role, title/body/code element anchors, reusable visual form, master/layout identity, footer/page-marker behavior, and any inherited empty structural placeholders.

- [ ] **Step 9: Write the template audit**

In `template-audit.txt`, record:

1. every master and child layout used by the 55 slides;
2. exact title/body/code/signature font properties and spacing rules;
3. footer, page number, section marker, CppCon brand, and byte-strip furniture that must remain;
4. reusable slide families for title, single claim, two-column comparison, code/signature, matrix, build graph, appendix, and closing;
5. the insertion contract: rewrite inherited elements, delete only classified elements, remap when a copied slide lacks usable slots.

- [ ] **Step 10: Verify the audit against the manifest and NDJSON**

Expected: every one of the 55 source slides has one audit record, and every element proposed for later rewriting has a real ID from the inspection output.

### Task 2: Create and validate the 61-slide clone map and starter deck

**Files:**

- Create: `.codex_tmp/cppcon2026_typelayout_final/template-frame-map.json`
- Create: `.codex_tmp/cppcon2026_typelayout_final/deviation-log.txt`
- Create: `.codex_tmp/cppcon2026_typelayout_final/template-starter.pptx`
- Create: `.codex_tmp/cppcon2026_typelayout_final/template-starter-preview/`
- Create: `.codex_tmp/cppcon2026_typelayout_final/template-starter-layout/`
- Create: `.codex_tmp/cppcon2026_typelayout_final/edit-plan.json`

**Interfaces:**

- Consumes: `template-inspect.ndjson`, layout JSON, `template-audit.txt`, and the primary source-slide mapping in spec section 7.
- Produces: a validated 61-slide starter and exact source/starter element targets for every allowed edit.

- [ ] **Step 1: Encode the approved output-to-source mapping**

Use this exact mapping:

```text
1:1, 2:4, 3:2, 4:2, 5:3, 6:2, 7:2, 8:3, 9:6, 10:5, 11:14,
12:8, 13:9, 14:10, 15:48, 16:12, 17:52, 18:45, 19:13, 20:13, 21:13, 22:52,
23:17, 24:15, 25:18, 26:50, 27:19, 28:21, 29:23, 30:24, 31:51, 32:25, 33:26, 34:27,
35:28, 36:29, 37:30, 38:31, 39:32, 40:34, 41:40, 42:35,
43:6, 44:26, 45:36,
46:37, 47:38, 48:39, 49:40, 50:41, 51:42, 52:44, 53:43, 54:44, 55:48, 56:45,
57:46, 58:51, 59:53, 60:54, 61:55
```

Expected: 61 unique output numbers, all source numbers in the range 1–55.

- [ ] **Step 2: Classify every inherited source element**

For every mapped output slide, add `editTargets` entries for each inherited element with one of `keep`, `rewrite`, `replace`, or `delete`. Each rewritten/deleted object must reference an exact inspected `shapeId`, `shapeIds`, `sourceElementId`, or `sourceElementIds`. Do not use broad text matching.

- [ ] **Step 3: Record omitted source slides**

Populate `omittedSourceSlides` for every source slide not selected anywhere. Use one concrete reason per source slide: superseded duplicate, promoted content, appendix-only material not retained, or redundant transition.

- [ ] **Step 4: Validate the frame map before authoring**

Run:

```powershell
& $env:RUNTIME_NODE 'C:\Users\gzsufanchen\.codex\plugins\cache\openai-primary-runtime\presentations\26.819.11345\skills\presentations\template_following_scripts\validate_template_plan.mjs' `
  --workspace 'E:\workspace\TypeLayout\.codex_tmp\cppcon2026_typelayout_final' `
  --map 'E:\workspace\TypeLayout\.codex_tmp\cppcon2026_typelayout_final\template-frame-map.json' `
  --source-slide-count 55
```

Expected: exit 0; no output slide lacks a source slide; no edit target is unresolved.

- [ ] **Step 5: Mark the artifact edit operation exactly once**

Run once and only once for the entire implementation:

```powershell
& $env:RUNTIME_NODE 'C:\Users\gzsufanchen\.codex\plugins\cache\openai-primary-runtime\presentations\26.819.11345\skills\presentations\container_tools\mark_artifact_operation_started.mjs' `
  --operation-kind edit `
  --expected-output-count 1 `
  --output-format pptx
```

Expected: exit 0. Do not rerun this command in later tasks.

- [ ] **Step 6: Build the starter deck by duplication**

Run:

```powershell
& $env:RUNTIME_NODE 'C:\Users\gzsufanchen\.codex\plugins\cache\openai-primary-runtime\presentations\26.819.11345\skills\presentations\template_following_scripts\prepare_template_starter_deck.mjs' `
  --workspace 'E:\workspace\TypeLayout\.codex_tmp\cppcon2026_typelayout_final' `
  --pptx 'E:\workspace\TypeLayout\.codex_tmp\cppcon2026_typelayout_final\source.pptx' `
  --map 'E:\workspace\TypeLayout\.codex_tmp\cppcon2026_typelayout_final\template-frame-map.json' `
  --out 'E:\workspace\TypeLayout\.codex_tmp\cppcon2026_typelayout_final\template-starter.pptx' `
  --preview-dir 'E:\workspace\TypeLayout\.codex_tmp\cppcon2026_typelayout_final\template-starter-preview' `
  --layout-dir 'E:\workspace\TypeLayout\.codex_tmp\cppcon2026_typelayout_final\template-starter-layout' `
  --contact-sheet 'E:\workspace\TypeLayout\.codex_tmp\cppcon2026_typelayout_final\template-starter-contact-sheet.png'
```

Expected: 61-slide starter, 61 previews, 61 layout JSON files, and a contact sheet.

- [ ] **Step 7: Inspect the starter and create the edit plan**

Import and inspect the starter with artifact-tool. In `edit-plan.json`, map semantic roles such as `title`, `body`, `code`, `signature`, `leftEvidence`, `rightEvidence`, `verdict`, `footer`, and `pageMarker` to exact starter anchor IDs and allowed actions for each output slide.

- [ ] **Step 8: Verify starter fidelity before content changes**

Inspect all 61 starter renders. Confirm order, source-slide correspondence, masters/layouts, CppCon brand marks, typography, footer, page marker, and inherited placeholder state. Record only intentional deviations in `deviation-log.txt`.

### Task 3: Implement the edit schema, content validator, and incremental builder

**Files:**

- Create: `.codex_tmp/cppcon2026_typelayout_final/deck-edit-schema.mjs`
- Create: `.codex_tmp/cppcon2026_typelayout_final/slide-content.mjs`
- Create: `.codex_tmp/cppcon2026_typelayout_final/validate-content.mjs`
- Create: `.codex_tmp/cppcon2026_typelayout_final/build-deck.mjs`
- Create: `.codex_tmp/cppcon2026_typelayout_final/schema-test.mjs`

**Interfaces:**

- Consumes: `template-starter.pptx`, `edit-plan.json`, `template-frame-map.json`.
- Produces: `validateSlideDefinitions(slides, throughSlide, editPlan)`, `applySlideDefinition(presentation, definition, editPlan)`, and a CLI builder accepting `--through-slide`, `--out`, `--render-dir`, and `--layout-dir`.

- [ ] **Step 1: Write a failing schema test**

Create `schema-test.mjs` with these cases:

```js
import assert from "node:assert/strict";
import { validateSlideDefinitions } from "./deck-edit-schema.mjs";

const editPlan = {
  1: {
    title: { anchorId: "sh/test-title", action: "rewrite" },
  },
};

const valid = [{
  slideNumber: 1,
  sourceSlide: 1,
  title: "Can I memcpy this type across a boundary?",
  narrativeRole: "opening question",
  operations: [{ kind: "setText", targetRole: "title", text: "Can I memcpy this type across a boundary?" }],
  sources: [],
}];

assert.doesNotThrow(() => validateSlideDefinitions(valid, 1, editPlan));
assert.throws(() => validateSlideDefinitions([{ ...valid[0], slideNumber: 2 }], 1, editPlan), /continuous/);
assert.throws(() => validateSlideDefinitions([{ ...valid[0], sourceSlide: 99 }], 1, editPlan), /source slide/);
assert.throws(() => validateSlideDefinitions([{ ...valid[0], title: "" }], 1, editPlan), /title/);
assert.throws(() => validateSlideDefinitions([{ ...valid[0], operations: [] }], 1, editPlan), /operation/);
```

- [ ] **Step 2: Run the schema test and verify it fails**

Run:

```powershell
& $env:RUNTIME_NODE 'E:\workspace\TypeLayout\.codex_tmp\cppcon2026_typelayout_final\schema-test.mjs'
```

Expected: failure because `deck-edit-schema.mjs` does not exist.

- [ ] **Step 3: Implement the schema validator**

`validateSlideDefinitions(slides, throughSlide, editPlan)` must assert:

- slide numbers are exactly `1..throughSlide`;
- each `sourceSlide` matches the fixed mapping in Task 2;
- every title and narrative role is non-empty;
- every definition has at least one operation;
- every operation kind is one of `setText`, `replaceText`, `setFill`, `setLine`, `setPosition`, `replaceImage`, `delete`, or `setNotes`;
- every operation's `targetRole` exists in `edit-plan.json` and permits that action;
- no visible string contains production language or unresolved markers;
- `Agreement`, `Admission`, `Evidence`, and `Permit` retain their exact capitalization when used as defined terms.

- [ ] **Step 4: Run the schema test and verify it passes**

Expected: exit 0 with all five assertions passing.

- [ ] **Step 5: Implement the artifact-tool builder**

Use this control flow in `build-deck.mjs`:

```js
const presentation = await PresentationFile.importPptx(
  await FileBlob.load(starterPptx),
);

validateSlideDefinitions(SLIDES, throughSlide, editPlan);

for (const definition of SLIDES.filter((s) => s.slideNumber <= throughSlide)) {
  applySlideDefinition(presentation, definition, editPlan);
}

await exportSlideEvidence(presentation, renderDir, layoutDir);
const pptx = await PresentationFile.exportPptx(presentation);
await pptx.save(outputPptx);
```

`applySlideDefinition` must resolve exact anchor IDs from `edit-plan.json`, edit inherited objects, preserve inherited typography by default, and reject an action that the map does not authorize.

- [ ] **Step 6: Implement notes generation**

For each slide with sources, append or replace a notes block in this exact form:

```text
[Sources]
- label | URI-or-repository-path | accessed 2026-08-23
```

Preserve unrelated existing notes. Never put source URLs on the visible canvas unless the source layout already uses a source footer.

- [ ] **Step 7: Implement incremental content validation**

`validate-content.mjs N` must import `SLIDES`, validate exactly slides `1..N`, confirm each definition's source mapping and edit roles, and return non-zero on missing/extra slides, unresolved targets, forbidden APIs, production language, or missing notes sources.

- [ ] **Step 8: Verify a one-slide smoke build**

Add slide 1's definition, run the builder with `--through-slide 1`, export a one-slide-edited/60-slide-preserved draft, and confirm slide 1 renders while slide 2 remains an untouched clone. This smoke build is discarded after validation.

### Task 4: Author and review slides 1–11 — boundary, profile, and insufficient local checks

**Files:**

- Modify: `.codex_tmp/cppcon2026_typelayout_final/slide-content.mjs`
- Modify: `.codex_tmp/cppcon2026_typelayout_final/edit-plan.json`
- Create: `.codex_tmp/cppcon2026_typelayout_final/draft-11.pptx`
- Create: `.codex_tmp/cppcon2026_typelayout_final/draft-11-render/`
- Create: `.codex_tmp/cppcon2026_typelayout_final/draft-11-layout/`

**Interfaces:**

- Consumes: the approved spec sections 4.1–4.2 and 5 for slides 1–11, including the main-spine/required-evidence/deferred-detail budget, plus the authoring harness.
- Produces: the complete opening question, two-axis boundary model, three scenarios, strict profile, seven-claim chain, and local-check counterargument.

- [ ] **Step 1: Run the partial validator before adding the batch**

Run `validate-content.mjs 11`.

Expected: failure naming slides 2–11 as missing.

- [ ] **Step 2: Add the exact slide titles and source mapping**

Add definitions for:

1. `Can I memcpy this type across a boundary?`
2. `Would you approve these bytes across every declared build?`
3. `Across a boundary, object representation becomes a contract`
4. `Build identity and address space are independent assumptions`
5. `A new process preserves representation—but not referents`
6. `A shared address space does not make two builds layout-compatible`
7. `Stored bytes outlive both the build and the address space`
8. `One strict profile makes the three scenarios comparable`
9. `Seven claims turn the question into a decision`
10. `Trivially copyable permits a local operation—it compares no builds`
11. `` `sizeof` can reject compatibility—but equal size cannot establish it ``

Use the source mapping from Task 2 and the visible content/visual job from spec section 5. Apply the narrative layering and detail-admission rules from spec sections 4.1–4.2; scenario detail that does not establish the two axes or strict profile belongs in notes or the appendix.

- [ ] **Step 3: Implement the opening `Measurement` tension**

Keep the title slide minimal. On slide 2 show `Measurement { uint64_t id; long double value; }`, the local trait, the four reassuring local facts, and `PERMIT?` without answering it.

- [ ] **Step 4: Implement the two-axis scenario sequence**

Slides 3–8 must visibly distinguish retained/lost build identity and retained/lost address-space identity. The strict profile on slide 8 must say ordinary copy, zero fixup, source-address-independent bytes, and finite declared build set.

- [ ] **Step 5: Implement the cumulative seven-claim chain**

Slide 9 uses the exact chain from spec section 5 as a brief orientation beat. It must not be titled or styled as an agenda, and it must not explain downstream predicates, formulas, artifact formats, or CI mechanics.

- [ ] **Step 6: Implement the two local-check counterexamples**

Slide 10 must show two disconnected local trait evaluations. Slide 11 must show that unequal size rejects while equal size remains unknown, including the equal-size/different-offset `wchar_t` example.

- [ ] **Step 7: Validate and build through slide 11**

Run:

```powershell
& $env:RUNTIME_NODE 'E:\workspace\TypeLayout\.codex_tmp\cppcon2026_typelayout_final\validate-content.mjs' 11
& $env:RUNTIME_NODE 'E:\workspace\TypeLayout\.codex_tmp\cppcon2026_typelayout_final\build-deck.mjs' `
  --through-slide 11 `
  --out 'E:\workspace\TypeLayout\.codex_tmp\cppcon2026_typelayout_final\draft-11.pptx' `
  --render-dir 'E:\workspace\TypeLayout\.codex_tmp\cppcon2026_typelayout_final\draft-11-render' `
  --layout-dir 'E:\workspace\TypeLayout\.codex_tmp\cppcon2026_typelayout_final\draft-11-layout'
```

Expected: validator and builder exit 0.

- [ ] **Step 8: Inspect slides 1–11 individually**

Check title wrapping, profile wording, axis labels, connector order, `PERMIT?` suspense, and transition into signature evidence. Confirm the stage still reads `why evidence is needed` rather than three independent boundary tutorials. Record `PASS` or a concrete correction for slides 1–11 in `qa-ledger.txt`; apply corrections before Task 5.

### Task 5: Author and review slides 12–22 — signature generation and Agreement

**Files:**

- Modify: `.codex_tmp/cppcon2026_typelayout_final/slide-content.mjs`
- Modify: `.codex_tmp/cppcon2026_typelayout_final/edit-plan.json`
- Create: `.codex_tmp/cppcon2026_typelayout_final/draft-22.pptx`
- Create: `.codex_tmp/cppcon2026_typelayout_final/draft-22-render/`
- Create: `.codex_tmp/cppcon2026_typelayout_final/draft-22-layout/`

**Interfaces:**

- Consumes: spec sections 4.1, 4.3, and the Agreement portion of 4.4, plus repository signature implementation and reflection source notes.
- Produces: the longest technical chapter, establishing trust properties, recursive normalization, canonical representation, and exact build-edge Agreement.

- [ ] **Step 1: Run the partial validator before adding the batch**

Run `validate-content.mjs 22`.

Expected: failure naming slides 12–22 as missing.

- [ ] **Step 2: Add the exact slide titles and source mapping**

Add definitions for:

12. `A useful signature must earn our trust`
13. `The declaration identifies entities; the compiler supplies the byte map`
14. `Reflection exposes facts; recursion turns them into structure`
15. `One consteval dispatcher handles every supported category`
16. `Leaf tokens describe representation—not source spelling`
17. `Absolute offsets remove irrelevant source paths`
18. `Hidden layout machinery is rejected—not guessed`
19. `A consteval walk assembles the certificate`
20. `Agreement is a registered-type × build-edge predicate`
21. `Exact equality is both a gate and a diagnostic`
22. `Agreement proves encoded representation—not identity, meaning, or independence`

- [ ] **Step 3: Establish signature trust properties before showing grammar**

Slide 12 must introduce coverage, canonicality, discrimination, and fail-closed behavior as the questions the implementation must answer. Fail-closed means signature generation rejects a type when a fact required by the proof cannot be completely encoded; it must not emit partial evidence. Slide 13 must state that the compiler supplies the byte map and TypeLayout does not infer layout from declaration order.

- [ ] **Step 4: Implement the reflection and recursion sequence**

Slide 14 shows only `nonstatic_data_members_of`, `type_of`, `offset_of`, `bases_of`, and `bit_size_of`, followed by enumerate → recover type → read position → classify → recurse. Slide 15 shows leaf, enum, array, record, union, opaque, and unsupported categories with explicit actions. Opaque emits a named trust contract; it must not be presented as fully reflected structure.

- [ ] **Step 5: Implement canonical leaf and absolute-offset normalization**

Slide 16 uses canonical tokens and size/alignment facts; names and typedef spellings stay absent. Slide 17 converges nested, base, and flat declarations into root-relative leaf offsets without claiming semantic schema equivalence.

- [ ] **Step 6: Implement fail-closed and final-certificate slides**

Slide 18 uses virtual inheritance as one concrete fail-closed path: required hidden machinery cannot be encoded completely, so signature generation rejects instead of guessing. Keep only the one-line encode / explicit-trust / reject policy; move the complete difficult-case matrix to the appendix. Slide 19 reveals one full `PacketHeader` signature and decodes its pointer-width/endianness envelope, record header, offsets, tokens, size, and alignment. It explicitly reconciles the Sched wording: `architecture` denotes the representation-relevant target envelope, the current prefix is not a CPU/ISA identifier, and exact target identity is later bound by provenance. State once that the same recursive inspection later supplies Admission inputs, but do not introduce the Admission formula yet.

- [ ] **Step 7: Define Agreement precisely**

Slide 20 first states that both artifacts refer to the same registered application contract, then uses the exact predicate from the spec and presents Agreement as a registered-contract-key × build-edge relation within the declared signature domain/version. Slide 21 shows one exact match, one divergence with readable locations, and a real `static_assert(layout_match(linux_plat::PacketHeader_layout, macos_plat::PacketHeader_layout))` using the current primitive. Slide 22 states the three limits—source identity, application meaning, and source-context independence—in one compact statement rather than three examples; slide 23 provides the pointer counterexample and slide 27 provides the semantic warning. It must say encoded certificates match rather than claim universal object-representation identity.

- [ ] **Step 8: Add reflection and signature sources to notes**

At minimum cite P2996R12, P3687R1, `include/boost/typelayout/detail/reflect.hpp`, `include/boost/typelayout/detail/signature_impl.hpp`, `include/boost/typelayout/detail/type_map.hpp`, `include/boost/typelayout/signature.hpp`, `example/compat_check.cpp`, and `test/test_core.cpp` on the slides whose claims they support.

- [ ] **Step 9: Validate, build, and inspect through slide 22**

Run the validator and builder through slide 22. Inspect slides 12–22 individually. Confirm the signature chapter reads `how one build produces evidence`, is visually varied, uses stable token/offset colors, never repeats a full signature without a new inference, defers full grammar/category variants to the appendix, and has no invented API.

### Task 6: Author and review slides 23–34 — Admission and closed CI

**Files:**

- Modify: `.codex_tmp/cppcon2026_typelayout_final/slide-content.mjs`
- Modify: `.codex_tmp/cppcon2026_typelayout_final/edit-plan.json`
- Create: `.codex_tmp/cppcon2026_typelayout_final/draft-34.pptx`
- Create: `.codex_tmp/cppcon2026_typelayout_final/draft-34-render/`
- Create: `.codex_tmp/cppcon2026_typelayout_final/draft-34-layout/`

**Interfaces:**

- Consumes: spec sections 4.1, 4.4, and 4.5, plus the Admission API, signature artifact format, compatibility reporter, and finite-contract model.
- Produces: a profile-aware node predicate, one-edge `EdgePass` rule, finite build graph, and one provenance-bound closed Permit decision per registered contract key.

- [ ] **Step 1: Run the partial validator before adding the batch**

Run `validate-content.mjs 34`.

Expected: failure naming slides 23–34 as missing.

- [ ] **Step 2: Add the exact slide titles and source mapping**

Add definitions for:

23. `Matching layouts can preserve the wrong thing`
24. `Admission applies one transfer profile to one build`
25. `Structural Admission has three independent conditions`
26. `The recursive check closes structural blind spots`
27. `Structural inspection cannot infer semantic dependence`
28. `Admission and Agreement reject independent failures`
29. `A closed claim needs a finite contract`
30. `Every actual build emits its own evidence`
31. `The emitted header contains evidence—not provenance`
32. `CI binds evidence to an exact producer`
33. `CI quantifies the same two gates over the declared graph`
34. `A closed run rejects every missing fact`

- [ ] **Step 3: Implement the pointer counterexample and profile-aware Admission**

Slide 23 ends `Agreement MATCH / Admission FAIL`. Slide 24 states that pointer rejection follows from the talk's strict source-address-independent profile, not every possible boundary profile.

- [ ] **Step 4: Implement the Admission predicate and ordinary-copy composition**

Slide 25 uses `LocalCopyLegal ∧ NoDetectedStructuralContextDependency ∧ RepresentationEvidenceComplete`. The final term means every reachable component is encoded or covered by an explicitly named trust contract; if a required component is unsupported, signature generation fails and Admission cannot pass. It must not be described as closed-contract proof. Slide 26 shows the real combined check `static_assert(std::is_trivially_copyable_v<PacketHeader> && is_byte_copy_safe_v<PacketHeader>)` plus one nested recursive walk, explicitly states that `is_byte_copy_safe_v<T>` alone is wider than the main ordinary-copy predicate, and moves the complete category algorithm to the appendix or notes.

- [ ] **Step 5: Show the structural/semantic limit and complete 2×2 gate**

Slide 27 uses one minimal integer-disguised-handle record to show the semantic limit without expanding into a handle taxonomy. Slide 28 labels the successful combination `EDGE PASS`, using `EdgePass_P(K,A,B) = Admission_P(K,A) ∧ Admission_P(K,B) ∧ Agreement(K,A,B)`. It states that valid, correctly attributed evidence is assumed and that `EdgePass` is not the final Permit. It ends by asking how CI closes the entire declared set; the closed-contract formula belongs to slides 29–34.

- [ ] **Step 6: Define the finite CI contract and per-build emission**

Slide 29 defines `C = (R,V,E,P)` with `E` explicitly named as the required transfer-edge set; use four labeled boxes rather than an extended set-theory explanation. Slide 30 shows Linux GCC, Linux Clang P2996, and macOS Clang P2996 each producing its own signature and local Admission-related evidence; no node infers another node's layout.

- [ ] **Step 7: Separate artifact fields from provenance**

Slide 31 shows only the current artifact fields needed by the decision—contract key, signature, and byte-copy-safe result—and notes that ordinary-copy registration separately enforces local trivial copyability. It separates observed evidence from producer identity and freshness. Slide 32 shows one simple declared-build + producer-attestation → accepted-evidence binding and explicitly says provenance validates inputs before the two gates; it is not a third gate. Move the complete `TypeEntry`, platform, provenance, and `.sig.hpp` versus external-attestation details to appendix slide 58.

- [ ] **Step 8: Close the graph without inventing an API**

Slide 33 uses the declared build graph and actual node-local Admission state, then generalizes slide 28's edge decision for one registered key: `ClosedPermit_C(K) = Admission_P(K,B) for every B ∈ V ∧ Agreement(K,A,B) for every (A,B) ∈ E`. State that CI repeats this closed decision for every `K ∈ R`; it must not collapse mixed per-type outcomes into one ambiguous Permit. Do not teach equality transitivity or spanning-tree comparison reduction on the main slide; those are appendix or speaker-note optimizations. Slide 34 reduces the per-key decision to two outcomes: any missing, stale, unattributable, Admission-failing, or Agreement-failing required fact means no Permit for that key; all its required facts valid and passing establishes `ClosedPermit_C(K)`. Preview the reporter's three diagnostic shapes—`byte-copy safe + layout match`, `Layout match (not byte-copy safe)`, and `Layout mismatch`—and defer the full report to appendix slide 57. The run is complete only after every declared key has a closed decision; an optional all-types-must-pass workflow policy belongs in notes or the appendix. Use current primitive composition or `CompatReporter`; do not show `TYPELAYOUT_ASSERT_TRANSFER_SAFE`.

- [ ] **Step 9: Add repository and CI sources to notes**

At minimum cite `include/boost/typelayout/admission.hpp`, `include/boost/typelayout/tools/sig_types.hpp`, `include/boost/typelayout/tools/sig_export.hpp`, `include/boost/typelayout/tools/compat_check.hpp`, `include/boost/typelayout/tools/compat_auto.hpp`, `example/compat_ci_export.cpp`, and `.github/workflows/compat-pipeline.yml`.

- [ ] **Step 10: Validate, build, and inspect through slide 34**

Confirm Agreement uses edge color/text, Admission uses node color/text, slides 20–28 read `how the two gates decide one edge`, and slides 29–34 read `how CI closes each key over the finite contract`. Confirm slide 28 says `EDGE PASS`, not `PERMIT`; provenance is visibly an input-validity condition; and no per-key Permit is inferred from a single edge, macro, or skipped CI job. Correct all slide-scoped QA findings before Task 7.

### Task 7: Author and review slides 35–45 — apply the model, bound the Permit, and close with a complete takeaway

**Files:**

- Modify: `.codex_tmp/cppcon2026_typelayout_final/slide-content.mjs`
- Modify: `.codex_tmp/cppcon2026_typelayout_final/edit-plan.json`
- Create: `.codex_tmp/cppcon2026_typelayout_final/draft-45.pptx`
- Create: `.codex_tmp/cppcon2026_typelayout_final/draft-45-render/`
- Create: `.codex_tmp/cppcon2026_typelayout_final/draft-45-layout/`

**Interfaces:**

- Consumes: spec sections 4.6–4.8, the slide-by-slide requirements for slides 35–45 in spec section 5, the closed two-gate model, the approved portable-capture implementation contract, and the completed demo's platform evidence.
- Produces: one coherent permitted native-byte type set, two independently rejected nearby alternatives, a bounded Permit, and a three-slide recap of the problem, method, and actionable takeaway.

- [ ] **Step 1: Run the partial validator before adding the batch**

Run `validate-content.mjs 45`.

Expected: failure naming slides 35–45 as missing.

- [ ] **Step 2: Add the exact slide titles and source mapping**

Add definitions for:

35. `A real contract starts with bytes every supported build must read`
36. `Four native types pass the complete three-build contract`
37. `One cached pointer removes a type from the raw-byte path`
38. `` One `long double` removes a type from the cross-ABI path ``
39. `Four permits and two rejections exercise both gates`
40. `A representation permit is deliberately narrow`
41. `Runtime obligations depend on the boundary`
42. `Serialize when the required contract is broader`
43. `` The real question is not “can I memcpy?”—it is “under which contract?” ``
44. `Reflection derives representation evidence; CI closes the decision`
45. `Permit native bytes only inside a closed contract`

- [ ] **Step 3: Instantiate the real portable-capture contract**

Slide 35 shows recorder build → capture file/persistent bytes → later analyzer build, labels the three provenance-bound builds, and states that all three pairwise Agreement edges are required because either endpoint may write or read. Introduce `R_capture = { PacketHeader, MeasurementSample, CaptureTrailer, CaptureBlock }` as the production type set and keep `P: ordinary copy · zero fixup · source-address-independent` visible. Do not show full declarations or CI mechanics.

- [ ] **Step 4: Show the permitted set and its practical consequence**

Slide 36 composes the 96-byte `CaptureBlock` from a 16-byte header, four 16-byte samples, and a 16-byte trailer. For every `K ∈ R_capture`, show Admission PASS on all three nodes, Agreement MATCH on every required edge, and one per-key Permit. Make the direct consequence precise: `ClosedPermit_C(CaptureBlock)` authorizes native object representation for the whole-block raw-I/O path with no per-field encoding, endian conversion, or fixup; lifetime, storage, synchronization, and error handling remain application obligations on slides 40–41. The other three Permits remain independent per-key results. Call the four keys the permitted set, not a new aggregate Permit predicate. Show `CaptureBlock: Admission PASS + Agreement MATCH → PERMIT` as the compact two-gate CI result; notes may map this to trivial-copy registration plus the reporter's `byte-copy safe + layout match` wording.

- [ ] **Step 5: Apply one nearby change to each gate**

Slide 37 starts from the working `MeasurementSample`, adds a cached metadata pointer, and evaluates the resulting `UnsafeWithPointer` through `C_candidate(UnsafeWithPointer)`: Agreement MATCH on every edge, Admission FAIL on every node, REJECT. Show the concrete reporter wording `Layout match (not byte-copy safe)`. Slide 38 replaces the sample's fixed-width value representation with the opening `Measurement { uint64_t id; long double value; }`: Admission PASS on every node, Linux GCC ↔ Linux Clang MATCH, while both Linux ↔ Apple edges DIFFER because Linux x86-64 uses `@16:fld80` with size/alignment 32/16 and Apple ARM64 uses `@8:fld64` with size/alignment 16/8. Keep the shared `[64-le]` prefix visible and wrap the decisive fragments in one compact `[DIFFER] Measurement layout signatures` diagnostic. Both candidates reuse the same `V`, `E`, and `P`; neither enters the production allowlist.

- [ ] **Step 6: Attach demo and ABI sources**

Cite the completed demo's retained artifacts, the x86-64 psABI, and Apple's ARM64 ABI documentation in speaker notes for slides 35–39. For slide 43, cite N5032 or `[basic.types]` for the local object-representation and trivially-copyable claims. For slide 44, cite P2996/P3687 plus the exact repository reflection, signature, Admission, export, and compatibility-check sources that support the compressed method chain. Slide 45 is a conceptual synthesis of already sourced claims and needs no new technical source beyond the repository/Q&A link. Keep provenance and full artifact detail out of the visible main-slide flow.

- [ ] **Step 7: Close Stage 5, bound its result in Stage 6, and build the three-part Stage 7 recap**

Slide 39 closes Stage 5 with three rows: every key in `R_capture` receives its own Permit, candidate `UnsafeWithPointer` is rejected only by Admission, and candidate `Measurement` is rejected only by Agreement. Keep the workflow success condition in notes. Stage 6 then stays deliberately brief: slide 40 separates what TypeLayout proves from application-owned obligations; slide 41 lists boundary-specific runtime obligations; slide 42 explains when the closed native-byte path is appropriate and when explicit conversion is required.

Stage 7 has three distinct recap jobs. Slide 43 returns to the opening `Measurement` question, distinguishes the local-copy question from the cross-boundary contract question, and states that the original unqualified question was incomplete. It must repeat the compact result `Measurement under C_candidate(Measurement) → Agreement DIFFER → REJECT`, but not the `long double` signature or ABI proof. Slide 44 compresses the method into one chain: declare `C = (R,V,E,P)` and contract key `K` → every `B ∈ V` evaluates `Admission_P(K,B)` and emits `Signature_B(K)` → CI validates the evidence inputs → Admission on every declared node plus Agreement of signatures on every required edge closes `K` over `C` → `ClosedPermit_C(K)` or `REJECT`. Evidence presence, attribution, and freshness remain evidence-input validation preconditions, not a third gate. Slide 45 gives the four-item design-review checklist, states that Permit is per-type and contract-scoped, directs broader requirements to explicit conversion, and ends with the exact operating rule from spec section 4.8. Do not replay signature grammar, ABI fragments, artifact fields, the demo matrix, or the full serialization comparison in Stage 7.

- [ ] **Step 8: Validate, build, and inspect through slide 45**

Confirm slides 35–39 read as one causal story—real boundary → declared contract → useful permitted set → Admission failure → Agreement failure → resolved matrix—without detouring into implementation mechanics. Confirm slides 40–42 form the short boundary chain—Permit obtained → proof boundary → application obligations → serialize when broader. Confirm slides 43–45 form a visibly separate conclusion—problem recap → method recap → actionable takeaway—without adding new proof obligations. Slide 45 must contain the GitHub URL and a Q&A cue to appendix slide 46 in the inherited footer, and must not become a generic thank-you slide.

### Task 8: Author and review appendix slides 46–61 and complete speaker-note sources

**Files:**

- Modify: `.codex_tmp/cppcon2026_typelayout_final/slide-content.mjs`
- Modify: `.codex_tmp/cppcon2026_typelayout_final/edit-plan.json`
- Create: `.codex_tmp/cppcon2026_typelayout_final/draft-61.pptx`
- Create: `.codex_tmp/cppcon2026_typelayout_final/draft-61-render/`
- Create: `.codex_tmp/cppcon2026_typelayout_final/draft-61-layout/`

**Interfaces:**

- Consumes: main-deck claims and the approved appendix question map.
- Produces: 16 deeper answers that extend rather than duplicate the main deck, plus complete source notes.

- [ ] **Step 1: Run the full validator before adding the appendix**

Run `validate-content.mjs 61`.

Expected: failure naming slides 46–61 as missing.

- [ ] **Step 2: Add the exact appendix titles and source mapping**

Add definitions for:

46. `Q&A map`
47. `` Why not `has_unique_object_representations`? ``
48. `Padding locations versus padding contents`
49. `Implicit lifetime, storage, alignment, overlap, and synchronization`
50. `Endianness and why byte swapping is conversion`
51. `` Why the global `[64-le]` envelope is conservative ``
52. `` `char`, `bool`, `wchar_t`, and floating-point assumptions ``
53. `Opaque type trust boundary`
54. `Full supported / assumed / rejected matrix`
55. `Full signature grammar and recursive engine pseudocode`
56. `Complete difficult-case encodings`
57. `Diagnostic report anatomy`
58. `Artifact format versus CI provenance`
59. `Portable-capture demo: types, artifacts, and exact verdicts`
60. `Ordinary copy versus relocation`
61. `C++29 relocation status and project-policy limits`

Slide 46's Q&A map must use the new appendix destinations 47–61; do not retain any pre-expansion 45–59 links or labels.

- [ ] **Step 3: Keep appendix material additive**

For every appendix definition, compare it with its main-deck dependency. Remove any repeated conclusion and retain only deeper wording, full grammar, complete encodings, complete tables, diagnostic anatomy, or standards-status detail.

Appendix slide 58 owns the complete artifact and provenance field lists and the `.sig.hpp` versus external-attestation distinction. Equality-transitivity and spanning-tree comparison reduction may remain in speaker notes or be reached through the Q&A map; they must not return to the main causal spine.

Appendix slide 59 owns the portable-capture implementation detail removed from Stage 5: complete positive/negative declarations, separate exporter registries, fixture-specific no-padding assertions, producer bundle contents, retained generated signatures, the full three-node/three-edge result table, and the exact positive-plus-negative CI success condition.

- [ ] **Step 4: Add ordinary-copy and relocation sources**

Slides 47–52 cite the current draft/N5032 clauses they interpret. Slides 60–61 cite P4197R0 and clearly state that trivial relocation was removed from C++26 and remains an open C++29 design space as of 2026-08-23.

- [ ] **Step 5: Add local implementation sources**

Slides 53–59 cite the exact repository files used: opaque registration, signature grammar, parser, safety classification, signature artifacts, and reporter implementation. Appendix slide 59 also cites the retained three-build positive/negative artifacts and their provenance manifests.

- [ ] **Step 6: Validate, build, and inspect all 61 slides**

Run the validator and builder through slide 61. Inspect slides 46–61 individually and verify appendix navigation is clear, titles do not wrap, code remains readable, and the visual treatment is recognizably appendix material without switching themes.

### Task 9: Run the technical-accuracy and evidence-chain review

**Files:**

- Create: `.codex_tmp/cppcon2026_typelayout_final/qa-content.mjs`
- Modify: `.codex_tmp/cppcon2026_typelayout_final/source-notes.txt`
- Modify: `.codex_tmp/cppcon2026_typelayout_final/qa-ledger.txt`
- Read: `include/boost/typelayout/detail/reflect.hpp`
- Read: `include/boost/typelayout/detail/signature_impl.hpp`
- Read: `include/boost/typelayout/detail/type_map.hpp`
- Read: `include/boost/typelayout/signature.hpp`
- Read: `include/boost/typelayout/admission.hpp`
- Read: `include/boost/typelayout/tools/sig_types.hpp`
- Read: `include/boost/typelayout/tools/sig_export.hpp`
- Read: `include/boost/typelayout/tools/compat_check.hpp`
- Read: `include/boost/typelayout/tools/compat_auto.hpp`
- Read: `example/sigs/x86_64_linux_clang.sig.hpp`
- Read: `example/sigs/arm64_macos_clang.sig.hpp`
- Read: `example/compat_check.cpp`
- Read: `example/portable_capture_types.hpp`
- Read: `example/portable_capture_export.cpp`
- Read: `example/portable_capture_negative_export.cpp`
- Read: `example/portable_capture_io.cpp`
- Read: `example/portable_capture_check.cpp`
- Read: `example/portable_capture_negative_check.cpp`
- Read: `test/test_core.cpp`
- Read: `.github/workflows/compat-pipeline.yml`

**Interfaces:**

- Consumes: the complete 61-slide draft and all claim-supporting implementation/standards sources.
- Produces: a machine-readable content gate plus a line-by-line evidence review that blocks technically inaccurate delivery.

- [ ] **Step 1: Implement the final content inspector**

`qa-content.mjs` must import the PPTX, inspect `slide,textbox,shape,notes,layout`, and assert:

- exactly 61 slides;
- exact expected title on every slide;
- no unresolved authoring markers, unresolved braces, authoring prompts, or production notes;
- no visible `TYPELAYOUT_ASSERT_TRANSFER_SAFE`;
- ordinary-copy slides contain the combined trivial-copyability plus structural predicate;
- relocation appears only in appendix slides 60–61 or an explicit exclusion statement;
- every required sourced slide contains a `[Sources]` notes block;
- slide 2 contains `PERMIT?`, slides 38–39 answer it for the candidate contract, slide 43 reframes the original question, and slide 45 resolves the talk with the final rule;
- slides 20, 24, 28, 33, and 40 keep Agreement, Admission, EdgePass, per-key closed-contract Permit, and application obligations distinct;
- slide 28 contains `EDGE PASS` and does not label its single-edge result `PERMIT`;
- slides 29–34 never present provenance as a third compatibility gate;
- slides 35–39 preserve the sequence real boundary → declared contract → permitted set → Admission rejection → Agreement rejection → final matrix;
- slides 40–42 preserve the brief Stage 6 sequence Permit obtained → proof boundary → application obligations → serialize when broader, without introducing a new gate, mechanism, or demo;
- slides 43–45 form the explicit Stage 7 conclusion in the order problem recap → method recap → actionable takeaway, without introducing a new proof obligation;
- slide 43 distinguishes the local-copy question from the cross-boundary contract question and contains `Measurement under C_candidate(Measurement) → Agreement DIFFER → REJECT` without replaying the `long double` proof;
- slide 44 declares `C` before evidence generation, then shows every build evaluating Admission and emitting its representation signature, CI validating those inputs, Agreement on all required edges, and the claim closing for `K` over `C`, ending in `ClosedPermit_C(K) or REJECT`; provenance remains evidence-input validation rather than a third gate;
- slide 45 contains all four design-review checklist items—declare `C = (R,V,E,P)`, check Admission and Agreement separately, keep Permit per-type and contract-scoped, and convert explicitly when the required contract is broader—ends with the exact operating rule, preserves the GitHub/Q&A footer, and contains no generic thank-you message;
- slide 36 presents four per-key Permits rather than inventing one aggregate Permit;
- slide 21 contains the real Agreement `static_assert`, and slide 26 contains the combined ordinary-copy Admission `static_assert`;
- slide 34 previews CI diagnostic shapes, slide 37 shows `Layout match (not byte-copy safe)`, and slide 38 shows a `[DIFFER]` layout diagnostic;

- [ ] **Step 2: Run the inspector against the 61-slide draft**

Run:

```powershell
& $env:RUNTIME_NODE 'E:\workspace\TypeLayout\.codex_tmp\cppcon2026_typelayout_final\qa-content.mjs' `
  'E:\workspace\TypeLayout\.codex_tmp\cppcon2026_typelayout_final\draft-61.pptx'
```

Expected: exit 0. Correct deck content rather than weakening an assertion.

- [ ] **Step 3: Verify reflection and normalization claims against code**

Check the visible operations/categories/grammar against the five signature/reflection files listed above. Record exact file/line evidence in `source-notes.txt`. Remove or rewrite any claim not supported by the baseline.

- [ ] **Step 4: Verify Admission and Agreement claims against code**

Check ordinary copy, opaque handling, pointer-like rejection, `TypeEntry`, key-based runtime reporting, and array-position compile-time comparison against the listed API files. Verify all four portable-capture positive keys, the exact two negative-result shapes, the whole-object I/O path, and the three pairwise checks against the completed demo sources and artifacts. Confirm the deck never upgrades current behavior into a nonexistent convenience API.

- [ ] **Step 5: Verify platform claims against primary evidence**

Record and cite:

- Apple ARM64 ABI: `https://developer.apple.com/documentation/xcode/writing-arm64-code-for-apple-platforms`;
- x86-64 psABI: `https://gitlab.com/x86-psABIs/x86-64-ABI/blob/master/x86-64-ABI/low-level-sys-info.tex`;
- retained portable-capture demo artifacts from all three declared builds;
- repository platform-metadata fixtures: `example/sigs/x86_64_linux_clang.sig.hpp` and `example/sigs/arm64_macos_clang.sig.hpp`.

Verify that visible `long double` size/alignment/token claims agree with the primary ABI sources and the completed demo artifacts.

- [ ] **Step 6: Verify standards-status claims against primary sources**

Record and cite:

- `https://www9.open-std.org/JTC1/SC22/WG21/docs/papers/2025/p2996r12.html`;
- `https://www.open-std.org/jtc1/SC22/wg21/docs/papers/2025/p3687r1.html`;
- `https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2025/n5032.pdf` or `https://eel.is/c++draft/basic.types`;
- `https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2026/p4197r0.html`.

Expected: slide wording reflects these sources as of 2026-08-23 and uses no broader claim than the source supports.

- [ ] **Step 7: Complete the evidence column in the QA ledger**

For slides 1–61, record `local`, `external`, `conceptual`, or `none required`, with the exact notes block checked for every `local` or `external` row.

### Task 10: Perform final artifact export, 61-slide visual QA, fidelity checks, and delivery copy

**Files:**

- Create: `.codex_tmp/cppcon2026_typelayout_final/final-candidate.pptx`
- Create: `.codex_tmp/cppcon2026_typelayout_final/final-render/`
- Create: `.codex_tmp/cppcon2026_typelayout_final/final-layout/`
- Create: `.codex_tmp/cppcon2026_typelayout_final/final-montage.webp`
- Create: `.codex_tmp/cppcon2026_typelayout_final/qa/template-fidelity-check.json`
- Create: `.codex_tmp/cppcon2026_typelayout_final/qa/template-fidelity-check.txt`
- Modify: `.codex_tmp/cppcon2026_typelayout_final/qa-ledger.txt`
- Create: `cppcon2026_typelayout_signature_centered.pptx`

**Interfaces:**

- Consumes: technically approved slide definitions, validated frame/edit maps, starter deck, and complete QA ledger.
- Produces: one final editable PPTX with fresh render, layout, overflow, placeholder, notes, theme, and template-fidelity evidence.

- [ ] **Step 1: Build a fresh final candidate from the starter**

Run the full builder with `--through-slide 61`, outputting `final-candidate.pptx`, `final-render`, and `final-layout`. Do not promote `draft-61.pptx` directly.

Expected: the candidate is regenerated from the starter and current definitions in one clean pass.

- [ ] **Step 2: Run the final content inspector**

Run `qa-content.mjs` against `final-candidate.pptx`.

Expected: exit 0.

- [ ] **Step 3: Run overflow detection**

Run:

```powershell
python 'C:\Users\gzsufanchen\.codex\plugins\cache\openai-primary-runtime\presentations\26.819.11345\skills\presentations\container_tools\slides_test.py' `
  'E:\workspace\TypeLayout\.codex_tmp\cppcon2026_typelayout_final\final-candidate.pptx'
```

Expected: exit 0 with no content outside the slide canvas. Investigate every warning; do not dismiss overlap or overflow without inspecting the slide.

- [ ] **Step 4: Run template-fidelity and structural-placeholder checks**

Run:

```powershell
& $env:RUNTIME_NODE 'C:\Users\gzsufanchen\.codex\plugins\cache\openai-primary-runtime\presentations\26.819.11345\skills\presentations\template_following_scripts\check_template_fidelity.mjs' `
  --workspace 'E:\workspace\TypeLayout\.codex_tmp\cppcon2026_typelayout_final' `
  --starter-pptx 'E:\workspace\TypeLayout\.codex_tmp\cppcon2026_typelayout_final\template-starter.pptx' `
  --final-pptx 'E:\workspace\TypeLayout\.codex_tmp\cppcon2026_typelayout_final\final-candidate.pptx' `
  --map 'E:\workspace\TypeLayout\.codex_tmp\cppcon2026_typelayout_final\template-frame-map.json' `
  --starter-layout-dir 'E:\workspace\TypeLayout\.codex_tmp\cppcon2026_typelayout_final\template-starter-layout' `
  --final-layout-dir 'E:\workspace\TypeLayout\.codex_tmp\cppcon2026_typelayout_final\final-layout' `
  --edit-dir 'E:\workspace\TypeLayout\.codex_tmp\cppcon2026_typelayout_final'
```

Expected: exit 0, no unfilled `sldNum`, `dt`, `ftr`, title/body, or other structural placeholders, and no unplanned deletion of brand furniture.

- [ ] **Step 5: Verify source-theme byte preservation**

Compare every `ppt/theme/theme*.xml` part between `source.pptx` and `final-candidate.pptx`. If artifact export changed a retained source theme part, restore that exact source part byte-for-byte as required by the presentation skill, then rerun content, render, overflow, and fidelity checks.

- [ ] **Step 6: Inspect all 61 final slides individually at full size**

For each `final-render` PNG, inspect:

- narrative claim and primary read;
- exact title and one-line wrapping;
- code/signature legibility;
- color plus text semantics;
- connectors behind nodes and away from labels;
- inherited footer/page marker/CppCon marks;
- no clipping, overlap, empty prompts, or accidental source content;
- consistency with adjacent-slide pacing and the approved source pattern.

Update one row per slide in `qa-ledger.txt`. Every row must end with content, visual, layout, placeholder, and source status all `PASS`.

- [ ] **Step 7: Inspect the complete montage for deck-level flow**

Use the montage only after individual review. Confirm the signature chapter receives the longest visual sequence, adjacent silhouettes vary, appendix styling is coherent, and the opening/closing `Measurement` loop is visible.

- [ ] **Step 8: Apply one final polish pass and rerun all gates**

Any change after Step 6 requires a fresh candidate export followed by `qa-content.mjs`, `slides_test.py`, template fidelity, theme comparison, and full-size inspection of every changed slide.

- [ ] **Step 9: Promote the verified candidate to the final path**

Run:

```powershell
Copy-Item -LiteralPath 'E:\workspace\TypeLayout\.codex_tmp\cppcon2026_typelayout_final\final-candidate.pptx' `
  -Destination 'E:\workspace\TypeLayout\cppcon2026_typelayout_signature_centered.pptx'
Get-FileHash -Algorithm SHA256 -LiteralPath `
  'E:\workspace\TypeLayout\.codex_tmp\cppcon2026_typelayout_final\final-candidate.pptx', `
  'E:\workspace\TypeLayout\cppcon2026_typelayout_signature_centered.pptx'
```

Expected: both SHA-256 values match.

- [ ] **Step 10: Verify the delivered copy, not only the candidate**

Run `qa-content.mjs`, `slides_test.py`, and one final artifact-tool slide-count/notes inspection against `E:\workspace\TypeLayout\cppcon2026_typelayout_signature_centered.pptx`.

Expected: all commands exit 0, 61 slides are present, and the delivered copy matches the verified candidate.

## Completion Evidence

Do not claim the deck complete until the final turn can cite fresh outputs showing:

- source SHA-256 equals isolated source-copy SHA-256;
- 55 source slides and layouts were inspected;
- frame-map validation passed for 61 mapped outputs;
- content validation passed for slides 1–61;
- final artifact contains exactly 61 slides;
- overflow detection passed;
- template-fidelity and structural-placeholder checks passed;
- all 61 QA-ledger rows are fully `PASS`;
- the retained portable-capture run reports four per-key Permits over all three declared nodes and edges;
- `UnsafeWithPointer` reports complete evidence, Admission FAIL on every node, and Agreement MATCH on every edge;
- `Measurement` reports complete evidence, Admission PASS on every node, Linux↔Linux MATCH, and both Linux↔Apple edges DIFFER;
- all three fully attributed producers emit identical deterministic `capture.bin` fixtures;
- final-candidate and delivered-copy SHA-256 values match;
- the final PPTX is cited exactly once as the output artifact.
