# Unit-Granularity Relocatable Handoff Demo Implementation Plan

**Goal:** Add the smallest credible per-unit shard-handoff demo that transfers
one independently owned region without per-field serialization or pointer
fixups, preserves the existing world demo, and produces authoritative Unit
evidence on all six locked 64-bit little-endian nodes.

**Architecture:** Generalize only the example-level region root finalization,
validation access, and checkpoint envelope code currently bound to
`WorldSnapshot`. Keep World and Unit schemas, validators, root accessors, and
business operations explicit. One unit region contains all owning dynamic data
and internal `relative_ptr` links; cross-unit relationships are stable
`UnitId` values resolved after validation through a destination registry. The
existing evidence graph gains exactly one scenario dimension and retains strict
per-scenario counts and provenance.

**Tech stack:** C++26 P2996 reflection, Boost.TypeLayout, CMake, CTest, Python
`unittest`, GCC 16.2 P2996, Bloomberg Clang P2996, GitHub Actions, locked Linux
OCI images, and locked macOS toolchain archives.

**Spec:**
`docs/superpowers/specs/2026-08-31-unit-handoff-demo-design.md`

## Global Constraints

- Do not modify PPTX files or the dedicated `CppCon2026TypeLayout` talk
  repository.
- Do not include, link, initialize, or copy code from XOffsetDatastructure.
- Do not add `opaque` registration for `relative_ptr`, a region container, or a
  unit type.
- Do not add a cross-buffer relative pointer. A non-null construction handle
  must still belong to the destination builder and generation.
- Do not change a public Boost.TypeLayout header unless a focused failing test
  proves a real missing library capability. The expected work is example-local.
- Preserve the existing C++ namespace and type identity of the World region
  representations. A file move or forwarding header must not create unrelated
  World signature churn.
- Preserve the World checkpoint's current 40-byte envelope and byte format.
- Never expose a typed view of an unvalidated copied buffer.
- TypeLayout Admission and Agreement gate loading; checksum, region geometry,
  and graph validation still validate actual bytes.
- Keep exact scenario identities `world` and `unit_handoff` throughout evidence
  JSON, generated headers, C++ models, audits, and closure.
- Keep exactly four contract keys per scenario. Do not flatten the results in a
  way that prevents independent World and Unit counts.
- Unit artifacts are generated natively by each declared producer and loaded by
  every other native consumer. Six isolated local smoke tests are not a
  substitute.
- Make path-scoped commits with imperative messages. Do not use `git add -A`.
- Keep build outputs and downloaded evidence outside tracked source paths.

## Intended Build Targets

- `relocatable_region_support`: shared example-level envelope implementation;
  the representation and storage headers remain header-only.
- `relocatable_world_support`: existing World runtime and World checkpoint
  wrappers, linked to `relocatable_region_support`.
- `relocatable_unit_handoff_support`: Unit runtime, validator, registry, and
  Unit checkpoint wrappers.
- `relocatable_unit_handoff_demo`: the talk-sized local demonstration.
- `relocatable_unit_export_ok`: normal four-key Unit signature fixture.
- `relocatable_unit_export_packed`: packed-`Effect` fixture.
- `test_relocatable_unit`.
- `test_relocatable_unit_checkpoint`.
- `test_relocatable_unit_agreement`.
- Existing producer, consumer, Agreement, and matrix targets extended to both
  scenarios without weakening their fixture or production modes.

---

### Task 1: Establish the Protected Baseline and Shared Region Seam

**Files:**

- Create: `example/relocatable_region_support/region.hpp`
- Create: `example/relocatable_region_support/region_storage.hpp`
- Create: `example/relocatable_region_support/checkpoint_envelope.hpp`
- Create: `example/relocatable_region_support/checkpoint_envelope.cpp`
- Modify: `example/relocatable_world_demo/region.hpp`
- Modify: `example/relocatable_world_demo/region_storage.hpp`
- Modify: `example/relocatable_world_demo/checkpoint.hpp`
- Modify: `example/relocatable_world_demo/checkpoint.cpp`
- Modify: `example/relocatable_world_demo/world_runtime.cpp`
- Modify: `CMakeLists.txt`
- Modify: `test/test_relocatable_region.cpp`
- Modify: `test/test_relocatable_checkpoint.cpp`

**Produces:** A root-type-independent builder and byte-envelope seam while the
World schema, signatures, checkpoint bytes, executable output, and tests remain
unchanged.

- [ ] Record the starting commit, clean/dirty status, checked World fixture
  hashes, and existing local demo output in an ignored build directory. Stop if
  unrelated user changes overlap a file in this task.
- [ ] Add a focused test root unrelated to `WorldSnapshot` and require
  `RegionBuilder::finish(region_handle<TestRoot>)` to compile, reject null or
  foreign roots, produce the same state transition, and expose no unvalidated
  typed root.
- [ ] Run the focused target first and capture the expected failure caused by
  the current hard-coded `finish(region_handle<WorldSnapshot>)`.
- [ ] Make `finish` a constrained root-type template while retaining standard
  layout, trivial-copyability, implicit-lifetime, owner, generation, non-null,
  offset, and active-builder checks.
- [ ] Extract the physical representation/storage headers while preserving the
  current C++ type namespace and identity. Replace old paths with forwarding
  headers so current includes continue to compile.
- [ ] Add one internal `RegionValidationAccess` capability for byte span, used
  size, root offset, copied/validated state, and final state transition. Do not
  expose a public arbitrary typed root.
- [ ] Extract the field-by-field envelope codec behind a descriptor containing
  magic, format, and schema. Keep World wrapper constants and encoded bytes
  exactly unchanged.
- [ ] Convert World load/save and `WorldRegionValidator` to the shared seam.
  Keep the World rejection layers and messages stable where tests depend on
  them.
- [ ] Run World exporter targets and prove both checked `.sig.hpp` fixtures are
  byte-for-byte unchanged.
- [ ] Run the existing region, checkpoint, World, demo, and core tests.

**Focused verification:**

```bash
cmake --build build-unit-handoff --target \
  test_relocatable_region test_relocatable_checkpoint \
  test_relocatable_world relocatable_world_demo -j2
ctest --test-dir build-unit-handoff \
  -R 'test_relocatable_(region|checkpoint|world)|relocatable_world_demo' \
  --output-on-failure
```

**Commit:** `refactor: share relocatable region support`

---

### Task 2: Add the Closed Unit Schema and Admission Contract

**Files:**

- Create: `example/relocatable_unit_handoff_demo/unit.hpp`
- Create: `test/test_relocatable_unit.cpp`
- Modify: `CMakeLists.txt`

**Produces:** The four stored Unit contract types and compile-time whole-region
Admission without runtime behavior yet.

- [ ] Register `test_relocatable_unit` before adding the schema and confirm the
  missing-header/contract failure.
- [ ] Define `UnitId`, `UnitPosition`, `EffectKind`, `Effect`,
  `EffectRelativePtr`, `AttributeEntry`, and `UnitSnapshot` exactly as approved.
- [ ] Add representation assertions for standard layout, trivial copyability,
  implicit lifetime, and absence of native pointer fields.
- [ ] Add closed `region_relocation_traits` specializations for `Effect` and
  `UnitSnapshot`; every result must be the conjunction of the admitted stored
  member representations.
- [ ] Add `for_each_unit_contract_type` with the exact stable keys
  `UnitSnapshot`, `Effect`, `EffectRelativePtr`, and `AttributeEntry`.
- [ ] Add `unit_contract_admitted_v` and require all four types under
  `whole_region_relocation`.
- [ ] Compile `get_layout_signature<EffectRelativePtr>()` and
  `get_layout_signature<Effect>()` as direct evidence that recursive pointee
  relationships terminate without opaque registration.
- [ ] Add a native-pointer alternative containing `Effect*` and prove whole
  region Admission fails.
- [ ] Run the test with both supported P2996 compiler families available
  locally; if only one is locally available, require the other in Task 9 CI.

**Commit:** `feat: add unit handoff schema contract`

---

### Task 3: Build and Validate Independent Unit Regions

**Files:**

- Create: `example/relocatable_unit_handoff_demo/unit_runtime.hpp`
- Create: `example/relocatable_unit_handoff_demo/unit_runtime.cpp`
- Modify: `test/test_relocatable_unit.cpp`
- Modify: `CMakeLists.txt`

**Produces:** Canonical unit construction, a staged Unit validator, destination
registry behavior, offset capture, queries, and controlled HP mutation.

- [ ] Write failing runtime tests for the canonical migrating unit and the
  independently stored destination owner unit.
- [ ] Build unit 1001 with name `Ranger`, HP 300, owner ID 9001, target ID 2001,
  two sorted attributes, two labeled effects, `A.next -> B`, `B.next -> A`,
  selected A, and pointer order `[A, B, A, null]`.
- [ ] Build owner unit 9001 in another builder and buffer. It may use empty
  optional collections, but the same schema and validator must accept it.
- [ ] Implement staged byte-first validation for the root, name, effects,
  effect labels, attribute entries, and effect-order array. Use checked
  arithmetic and owning-range non-overlap.
- [ ] Require strictly increasing unique attribute keys.
- [ ] Validate every non-null selected/order/next value as the exact start of an
  element in the validated effects array. Validate membership without following
  the cycle recursively.
- [ ] Expose `unit_root`, canonical graph queries, and a `UnitOffsets` snapshot
  only after validation.
- [ ] Add a minimal `UnitRegistry` that move-owns validated buffers, rejects a
  duplicate ID or a root/registration-ID mismatch, and resolves stable IDs.
- [ ] Prove owner 9001 resolves to another buffer and target 2001 remains
  unresolved without invalidating unit 1001.
- [ ] Add controlled HP mutation for a validated registered unit and prove the
  owner buffer is unaffected.
- [ ] Attempt to bind a handle from builder A into builder B and require the
  existing foreign-builder exception before finalization.
- [ ] Add range, alignment, overlap, map-order, pointer-middle, and pointer-out-
  of-range negative tests. These remain silent test cases rather than demo
  output.

**Commit:** `feat: add validated independent unit regions`

---

### Task 4: Add the Unit Envelope and No-Fixup Load Path

**Files:**

- Create: `example/relocatable_unit_handoff_demo/unit_checkpoint.hpp`
- Create: `example/relocatable_unit_handoff_demo/unit_checkpoint.cpp`
- Create: `test/test_relocatable_unit_checkpoint.cpp`
- Modify: `CMakeLists.txt`

**Produces:** Unit-specific save/load wrappers over the shared envelope, with a
fresh destination allocation and validation before access.

- [ ] Register `test_relocatable_unit_checkpoint` and write failing tests for
  `TLUNIT\0\0`, `64LE`, `UNITV1\0\0`, exact length, checksum, and root offset.
- [ ] Implement `save_unit_checkpoint` and `load_unit_checkpoint` using the
  shared codec and `UnitRegionValidator`. Do not expose the internal generic
  loader as an arbitrary public schema/root cast.
- [ ] Keep source region A alive, load destination region B, require different
  allocation bases, byte-for-byte payload equality before mutation, and
  equality for every sampled raw offset.
- [ ] Prove string, vector, map, pointer-vector, null, sharing, and cycle behavior
  through the validated destination view.
- [ ] Add truncated, extra-byte, wrong magic, wrong format, wrong schema,
  non-zero flags, length, root-offset, checksum, region, and graph rejections.
- [ ] For the visible corrupt-link case, mutate one encoded `Effect::next`, then
  call the normal encoder to produce a valid checksum. Require envelope success
  and graph rejection before any application resolution.
- [ ] Re-run the complete World checkpoint suite to prove codec sharing did not
  change World behavior.

**Commit:** `feat: add unit checkpoint relocation`

---

### Task 5: Add Local Agreement Fixtures and the Talk-Sized Demo

**Files:**

- Create: `example/relocatable_unit_handoff_demo/agreement.hpp`
- Create: `example/relocatable_unit_handoff_demo/export_signatures.cpp`
- Create: `example/relocatable_unit_handoff_demo/demo.cpp`
- Create: `example/relocatable_unit_handoff_demo/sigs/unit_producer_ok.sig.hpp`
- Create: `example/relocatable_unit_handoff_demo/sigs/unit_producer_packed.sig.hpp`
- Create: `test/test_relocatable_unit_agreement.cpp`
- Modify: `CMakeLists.txt`

**Produces:** Strict four-key local Agreement and the approved positive/negative
story in one executable.

- [ ] Add strict key-based Agreement tests before implementation: exact four
  keys, order independence, duplicate/missing/unknown/null evidence as
  incomplete, normal MATCH, and packed DIFFER.
- [ ] Implement the Unit Agreement helper without positional joins.
- [ ] Add normal and `TYPELAYOUT_RELOCATABLE_UNIT_PACKED_EFFECT` exporters.
  Generate, inspect, and check in deterministic fixtures.
- [ ] Prove the packed fixture remains admitted and differs on the intended
  `Effect` contract entry. If any additional key differs, diagnose the actual
  signature closure rather than weakening the assertion.
- [ ] Add loader-call counters proving native-pointer Admission failure and
  packed Agreement difference both skip runtime loading.
- [ ] Implement the approved demo sequence and exact result categories:

```text
Unit contract: Admission PASS 4/4, Agreement MATCH
Transfer: source base != destination base, raw offsets unchanged
Containers: string=yes vector=yes flat_map=yes
Pointers: nullable=yes shared=yes cycle=yes pointer_vector=yes
Registry: owner 9001 RESOLVED, target 2001 UNRESOLVED
Business: unit 1001 attached, hp 300 -> 250

Negative[native pointer]: Admission FAIL
Negative[packed Effect]: Agreement DIFFER
Negative[foreign handle]: builder REJECT
Negative[corrupt Effect::next]: graph REJECT before dereference
```

- [ ] Make the executable return non-zero if any printed claim is false.
- [ ] Run the three Unit tests, Unit demo, and all World tests.

**Commit:** `feat: demonstrate unit-granularity handoff`

---

### Task 6: Generalize the Evidence Model to Two Strict Scenarios

**Files:**

- Modify: `tools/relocatable_world_evidence.py`
- Modify: `test/test_relocatable_world_evidence.py`
- Modify: `example/relocatable_world_demo/matrix_model.hpp`
- Modify: `example/relocatable_world_demo/matrix_check.cpp`
- Modify: `example/relocatable_world_demo/agreement_check.cpp`

**Produces:** Schema-2 evidence, generated inputs, Agreement, audit, and closure
that preserve World and Unit identities and counts separately.

- [ ] Add Python fixture builders and failing parser tests for exactly two
  scenarios. Reject schema 1 in a schema-2 run rather than guessing or silently
  upgrading historical evidence.
- [ ] Define canonical contract keys per scenario and reject missing, extra,
  duplicate, or cross-scenario keys.
- [ ] Change producer facts to:

```json
{
  "schema": 2,
  "node": "...",
  "contracts": {
    "world": {"admission": {}, "signatures": {}},
    "unit_handoff": {"admission": {}, "signatures": {}}
  }
}
```

- [ ] Extend sealed provenance to bind one eight-entry signature header plus
  separate `world` and `unit_handoff` region filenames and SHA-256 values.
- [ ] Extend consumer results so every directed producer slot contains separate
  scenario status, reason, and region digest. Preserve the producer provenance
  digest once per directed slot.
- [ ] Extend Agreement records to carry four named decisions under each
  scenario for every unordered pair.
- [ ] Extend closure expected identities and counts so authoritative success
  independently requires:

```text
world:        pairs=15 named=60 permits=60 transfers=30 passes=30
unit_handoff: pairs=15 named=60 permits=60 transfers=30 passes=30
```

- [ ] Preserve explicit incomplete slots. Missing Unit evidence must not shrink
  the graph or inherit World status.
- [ ] Generalize the C++ matrix model with `scenario_id`, scenario-specific
  contract arrays, region records, decisions, and transfer status. Do not build
  a runtime plugin registry.
- [ ] Add self-tests for a complete two-scenario authoritative graph, a complete
  two-scenario local graph, one scenario rejected, one scenario incomplete,
  artifact substitution, digest mismatch, duplicate identities, and mixed run
  attempts.
- [ ] Keep existing provenance, toolchain, SDK, source-lock, output-lock, and
  runner constraints unchanged.

**Python verification:**

```bash
python -m unittest test.test_relocatable_world_evidence
```

**Commit:** `refactor: add scenario-aware relocation evidence`

---

### Task 7: Emit and Consume Both Native Payloads

**Files:**

- Modify: `example/relocatable_world_demo/platform_probe.cpp`
- Modify: `example/relocatable_world_demo/export_signatures.cpp`
- Modify: `example/relocatable_world_demo/producer.cpp`
- Modify: `example/relocatable_world_demo/consumer.cpp`
- Modify: `example/relocatable_world_demo/agreement_check.cpp`
- Modify: `example/relocatable_world_demo/matrix_check.cpp`
- Modify: `CMakeLists.txt`
- Modify: `test/test_relocatable_world_evidence.py`

**Produces:** Each native build reports both contracts, generates two real
payloads, and executes both loaders against every foreign producer.

- [ ] Extend the optimized platform probe to report all eight Admission values
  grouped by scenario while retaining the existing architecture, reflection,
  byte-order, pointer-width, and lifetime probes.
- [ ] Export one deterministic matrix signature header containing all eight
  uniquely named contract entries. Keep the four-key local World and Unit
  fixture exporters separate from this matrix export.
- [ ] Update the native producer to write schema-2 facts,
  `<node>.world.region`, and `<node>.unit.region`. It must build, save, load, and
  verify each admitted scenario before atomically publishing that scenario's
  artifact.
- [ ] If one scenario is not admitted, omit only its region and emit its exact
  Admission facts; do not claim the node is fully ready.
- [ ] Update stale-output cleanup for both region names and the combined
  signature file.
- [ ] Update generated consumer input to carry both scenario contracts and both
  verified artifact paths for every producer slot.
- [ ] Update the consumer to evaluate World and Unit independently. It may call
  a scenario loader only after all four decisions for that scenario permit.
- [ ] For Unit PASS, require unchanged copied bytes/offsets, canonical internal
  graph, owner resolved, target unresolved, and isolated HP mutation.
- [ ] Emit one schema-2 result file per consumer with exactly five directed
  producer slots and exactly two scenario outcomes in each slot.
- [ ] Update Agreement and closure executables to enforce exact separate counts
  and report a combined summary only after both scenario summaries.
- [ ] Add CMake regeneration tests for all generated headers and fallback inputs.

**Expected authoritative summary contract:**

```text
WORLD: Agreement 60/60; directed loads 30/30
UNIT: Admission 24/24; Agreement 60/60; directed handoffs 30/30
COMBINED: Agreement 120/120; directed transfers 60/60
```

**Commit:** `feat: exchange world and unit native artifacts`

---

### Task 8: Extend Workflow, Audit, and ARM Mac Launcher

**Files:**

- Modify: `.github/workflows/relocatable-world-matrix.yml`
- Modify: `.github/scripts/verify-p2996-toolchain.sh`
- Modify: `tools/run-relocatable-world.sh`
- Modify: `test/test_relocatable_world_workflow.py`
- Modify: `test/test_toolchain_workflow.py`
- Modify: `test/test_toolchain_locks.py` only if a current assertion enumerates
  the old four-contract application probe
- Modify: `CLAUDE.md`

**Produces:** Existing locked toolchains execute both scenarios on all six
native nodes; the personal ARM Mac path executes both scenarios on its existing
non-authoritative 5/6 profile.

- [ ] Add failing static workflow tests for both region artifacts, eight
  signatures, scenario-aware seal/consumer/Agreement/closure commands, exact
  path triggers for `example/relocatable_region_support/**` and
  `example/relocatable_unit_handoff_demo/**`, and artifact upload on failure.
- [ ] Keep the six existing node IDs, runner classes, compiler locks, action
  pins, SDK locks, native-only authoritative rule, `if: always()` behavior, and
  least permissions unchanged.
- [ ] Build and run the Unit demo/tests in each producer job before sealing
  artifacts.
- [ ] Upload and download both region files under the same run-attempt identity.
- [ ] Pass both exact region paths into the sealer and generated consumer input.
- [ ] Update the application toolchain verification to require eight admitted
  matrix contract entries for a permitting candidate. Do not rebuild or reseal
  compiler images merely because the application gained four types.
- [ ] Update the ARM Mac launcher to build both support libraries and producers,
  retain both artifacts, execute both scenarios for every local directed edge,
  and audit exact per-scenario counts.
- [ ] Require the local 5/6 profile to report:

```text
WORLD local: Agreement 40/40; directed loads 20/20
UNIT local: Admission 20/20; Agreement 40/40; directed handoffs 20/20
COMBINED local: Agreement 80/80; directed transfers 40/40
authoritative=false
```

- [ ] Keep local x86-64 Linux runs explicitly Docker-emulated and keep the
  missing macOS x86-64 node explicit. Do not label local 5/6 as authoritative.
- [ ] Update `CLAUDE.md` with build, local launcher, evidence audit, and result
  interpretation commands. Do not add talk narrative.
- [ ] Run all workflow, evidence, launcher, and lock tests.

**Verification:**

```bash
python -m unittest \
  test.test_relocatable_world_workflow \
  test.test_toolchain_workflow \
  test.test_toolchain_locks
```

**Commit:** `ci: validate unit handoff across six platforms`

---

### Task 9: Run the Complete Local Regression Gate

**Files:** No intended source changes. Commit only diagnosed fixes with their
own focused tests.

- [ ] Configure a fresh P2996 build rather than reusing a pre-feature cache.
  On the current Windows/WSL workspace, use an ignored
  `build-unit-handoff` directory.
- [ ] Build every default target and run full CTest with output on failure.
- [ ] Run `relocatable_world_demo` and capture its output for comparison with
  the Task-1 baseline.
- [ ] Run `relocatable_unit_handoff_demo` and compare every line category with
  the approved design.
- [ ] Run all Python evidence, workflow, and lock suites.
- [ ] Regenerate World and Unit local fixtures; require no unexplained diff.
- [ ] Run the C++ matrix fixture self-tests and Python `audit-run` fixture tests.
- [ ] Run `git diff --check`, inspect the complete branch diff, and confirm no
  change under the talk repository, PPTX paths, vendor submodule, or public
  TypeLayout headers unless separately justified by a focused regression.

**Representative commands:**

```bash
cmake -S . -B build-unit-handoff -G Ninja \
  -DCMAKE_CXX_COMPILER=/root/clang-p2996-install/bin/clang++ \
  -DCMAKE_CXX_FLAGS='-std=c++26 -freflection -freflection-latest -stdlib=libc++' \
  -DTYPELAYOUT_BUILD_COMPAT_CI=OFF
cmake --build build-unit-handoff -j2
ctest --test-dir build-unit-handoff --output-on-failure
```

```powershell
python -m unittest discover -s test -p "test_relocatable_*.py"
python -m unittest test.test_toolchain_workflow test.test_toolchain_locks
git diff --check
git status --short
```

If a test fails, diagnose the first causal error before retrying. Do not weaken
an assertion or downgrade the six-platform claim to make the suite green.

---

### Task 10: Obtain and Audit Authoritative 6/6 Evidence

**Files:**

- Create after success:
  `docs/superpowers/evidence/2026-08-31-unit-handoff-authoritative.md`
- Modify implementation/tests only when a diagnosed CI failure requires a real
  fix.

**Produces:** A retained successful GitHub Actions run proving both scenarios
from one final implementation commit.

- [ ] Push `cppcon2026demo` normally after the local gate passes. Do not force
  push.
- [ ] Dispatch or identify the `relocatable-world-matrix.yml` run for the exact
  implementation SHA and record `<run_id>.<run_attempt>`.
- [ ] Follow the workflow until completion. On failure, inspect the failed job
  and retained artifacts, reproduce when possible, add a focused regression,
  commit the fix, push, and start a fresh authoritative run. Never combine
  artifacts from failed or older attempts.
- [ ] Download the final run artifacts to an ignored directory and run strict
  audit with exact separate expectations:

```text
nodes=6
world pairs=15 named permits=60 directed passes=30
unit_handoff pairs=15 named permits=60 directed passes=30
combined named permits=120 directed passes=60
status=PASS authoritative=true
```

- [ ] Inspect all six Unit provenance records and require each native producer
  region digest to be present and distinct from missing/fallback values.
- [ ] Inspect all six consumer records and require five foreign Unit PASS
  outcomes each. Counts retain producer/consumer identities so duplicates
  cannot hide omissions.
- [ ] Record the final source SHA, run URL, run attempt, node/toolchain table,
  exact World and Unit counts, audit command, and claim boundary in the evidence
  document. Do not copy this material into PPTX.
- [ ] Commit the evidence record, push normally, and ensure a documentation-only
  commit does not invalidate the retained implementation-SHA claim.

**Implementation completion commit, if no CI repair was needed:** No additional
source commit. The evidence record uses
`docs: record unit handoff matrix evidence`.

---

## Final Review Checklist

- [ ] `UnitSnapshot`, `Effect`, `EffectRelativePtr`, and `AttributeEntry` are the
  only Unit Agreement keys.
- [ ] The unit buffer contains dynamic string/vector/map data plus null, shared,
  cyclic, and container-stored relative pointers.
- [ ] No `relative_ptr` crosses a unit buffer; cross-unit owner/target links are
  stable IDs.
- [ ] Owner 9001 resolves and target 2001 remains unresolved after handoff.
- [ ] Source and destination buffers prove changed base and unchanged offsets
  locally; cross-platform consumers prove unchanged foreign bytes in fresh
  allocations without transmitted addresses or fixups.
- [ ] Native pointer, packed effect, foreign handle, and corrupt offset reject at
  their intended layers.
- [ ] Existing World checkpoint bytes, signatures, demo behavior, tests, and
  authoritative counts remain intact.
- [ ] Evidence schema, generated headers, consumers, Agreement, audit, closure,
  workflow, and launcher all enforce two exact scenarios.
- [ ] Unit authoritative result is 24/24 Admission, 60/60 Agreement, and 30/30
  directed handoffs.
- [ ] Final run uses the same source SHA, workflow attempt, committed locks, and
  native toolchain identity across every artifact.
- [ ] No PPTX, talk-repository, vendor, or unrelated user-owned file changed.

## Stop Conditions

Stop and request user direction only if implementation proves one of these
approved premises false:

- the existing finite non-opaque `relative_ptr<Effect>` signature cannot
  represent the recursive schema;
- one of the six locked toolchains cannot compile the approved Unit contract
  without a public TypeLayout semantic change;
- preserving current World type identity and byte format conflicts with the
  minimum shared seam;
- six-platform native execution requires replacing or republishing a locked
  compiler artifact rather than extending application targets.

Ordinary build failures, schema-parser failures, workflow defects, or test
regressions are implementation work to diagnose and fix, not reasons to reduce
scope.
