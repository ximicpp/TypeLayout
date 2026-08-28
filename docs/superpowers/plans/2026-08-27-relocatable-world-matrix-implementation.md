# Relocatable World Native Matrix Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Turn the completed standalone relocatable-world demo into reproducible six-node native evidence with 15 four-key Agreement records, 30 directed validated loads, immutable toolchain provenance, and a one-command ARM64 Mac 5/6 workflow.

**Architecture:** Each native node runs one shared capability probe, exports four TypeLayout signatures, produces one canonical `.region`, and seals those facts plus artifact hashes into strict JSON provenance. A schema-specific Python evidence tool validates JSON and SHA256 inputs and generates constexpr headers; a C++ verification consumer runs inside the build/CI evidence workflow, compares the current build with each producer, and exercises cross-loads only after the representation decision passes. It is not the deployed application loader. A C++20-only matrix checker remains runnable without P2996 and closes the fixed node/edge graph. Toolchains use a source lock followed by separately reviewed output locks; the authoritative workflow never consumes mutable tags.

**Tech Stack:** C++26 P2996 for producers/consumers, standalone C++20 for closure, Python 3 standard library, CMake/CTest, Docker Buildx, GHCR OCI images, native GitHub macOS archives, GitHub Actions, GCC 16.2, and Bloomberg Clang P2996.

**Spec:** `docs/superpowers/specs/2026-08-27-relocatable-world-demo-design.md`

**Prerequisite plan:** `docs/superpowers/plans/2026-08-27-relocatable-world-demo-implementation.md` must pass its Core Completion Gate first.

## Global Constraints

- The fixed authoritative node set is exactly:
  - `x86_64_linux_gcc`
  - `x86_64_linux_clang`
  - `arm64_linux_gcc`
  - `arm64_linux_clang`
  - `arm64_macos_clang`
  - `x86_64_macos_clang`
- The runner labels are `ubuntu-24.04`, `ubuntu-24.04-arm`, `macos-15`, and `macos-15-intel`; revalidate their availability before enabling a required check. GitHub's observed `ImageOS` and `ImageVersion` are recorded for diagnostics only: hosted images are mutable, so those two strings are never a source/output lock, an Admission condition, or an authoritative-closure gate.
- GCC is exactly release 16.2.0. Bloomberg Clang starts from the reviewed `p2996` commit `060be17654102019e14810c3f948ef85a490755f`, and hosted-runner inventory starts from reviewed `actions/runner-images` commit `564e58dbe650c507ccba1171f6159c12f26820c8`; changing either requires a new source-lock review and complete evidence regeneration.
- Every node must prove `CHAR_BIT == 8`, 64-bit pointers, little-endian native order, working P2996 reflection, and optimized distinct-source `std::memcpy` implicit creation for both one object and one runtime-bound array.
- Linux images must contain native `linux/amd64` and `linux/arm64` manifests built on matching native runners. QEMU may run local demo builds but may not produce authoritative toolchains or native evidence.
- macOS ARM64 and x86-64 archives must be built on matching native GitHub runners and bundle the compiler with its matching libc++, libc++abi, and libunwind. The immutable Apple environment contract is the exact Xcode version and build, macOS SDK version and build, explicit deployment target, and checksum-sealed archive; authoritative jobs select and verify all of them and compile with explicit `DEVELOPER_DIR`/`-isysroot`/deployment flags. Local use records its actual identities and whether those hard-locked values match.
- `toolchain-sources.lock` contains only exact sources and build inputs, including Docker client/server versions, Docker Buildx version, the digest-qualified BuildKit image, and the repository-normalized LF SHA256 of `toolchain-images.yml`. `toolchains.lock` seals output digests/URLs/checksums, their producing source/workflow identity, verified compiler and Apple identities, and observed diagnostic metadata; it is committed in a separate review after candidate publication.
- Both GHCR packages remain private. Workflows authenticate with the minimum package permission required by each job; the local launcher requires either an authenticated `gh` token or an explicit PAT with `read:packages` and passes it to `docker login --password-stdin`. No workflow or launcher changes package visibility.
- The matrix and local launcher fail before building if `toolchains.lock` is absent, unsealed, inconsistent with the source lock, contains an empty checksum, branch-only source, mutable tag, or `latest`.
- Existing `ci.yml` and `compat-pipeline.yml` may retain legacy moving-image tags only as non-authoritative regression jobs. They must never be cited as six-node evidence.
- READY producer evidence is a three-file bundle: `<node>.sig.hpp`, `<node>.region`, and `<node>.provenance.json`, with provenance binding the other two by SHA256. REJECT and INCOMPLETE bundles contain only their provenance file.
- Provenance records the four profile Admission decisions and four signature strings directly. `TypeEntry::byte_copy_safe` is not whole-region Admission.
- Every Agreement record joins exactly four unique keys by name. `PERMIT` means Admission on both nodes plus equal signatures for that key; it never includes loading or application validation.
- Every authoritative consumer emits exactly five directed records; every local 5/6 consumer emits exactly four. Neither profile permits a self-edge. Loader status is separate: `PASS`, `SKIPPED_TYPELAYOUT_REJECT`, `REJECT_ENVELOPE`, `REJECT_REGION`, `REJECT_GRAPH`, or `INCOMPLETE`.
- Admission and Agreement are pre-deployment evidence decisions produced by compile/build and verification-build/CI work. The CI consumer executable may run those comparisons and cross-load tests in one job, but a deployed server or native client starts only from an already permitted path and performs runtime envelope, range, and graph validation on the actual region.
- Every non-fallback provenance record participating in one closure has the same `source_sha`, `workflow_run`, `sources_sha256`, and `outputs_sha256`. Authoritative values must equal that workflow's `${{ github.sha }}`, `${{ github.run_id }}.${{ github.run_attempt }}`, and the hashes of the committed lock files; local values must equal the clean committed `HEAD`, one launcher invocation ID, and those same committed lock-file hashes. The attempt suffix prevents artifacts from a rerun of the same workflow run from cohering with the original attempt. Mixed-run, mixed-attempt, or mixed-commit bundles are `INCOMPLETE` even when every individual file is otherwise valid.
- Overall closure precedence is `INCOMPLETE`, then `REJECT`, then `PASS`. Missing jobs or artifacts never shrink the declared graph.
- Agreement, consumer, and closure jobs use always-running paths, upload their result artifact before failing, and preserve all expected identity slots.
- No matrix code includes or initializes XOffsetDatastructure. The vendor gitlink remains a reference only.
- Do not modify the main CppCon deck. Preserve the pre-existing user-owned working-tree bytes of `docs/talk/cppcon2026-main-deck-content-and-script.md` and never stage that file as part of this work. After real evidence exists, write only the separate Chinese appendix-notes Markdown named in Task 8.
- `.gitattributes` pins LF for toolchain recipes, scripts, and workflows; source-lock recipe digests use those repository-normalized LF bytes so Windows `core.autocrlf` cannot change the identity. New `.sh` files also use Git executable mode `100755`.

## Evidence Schemas

`<node>.provenance.json` is strict JSON. For `READY` and evaluated `REJECT`, its top-level object has exactly `schema`, `node`, `status`, `probe`, `admission`, `signatures`, `compiler`, `build`, `locks`, and `artifacts`; the validator rejects unknown or missing fields. The nested contracts are exact:

| Path | Required value |
|---|---|
| `schema` | integer `1` |
| `node` | one member of the fixed six-node tuple |
| `status` | `READY`, `REJECT`, or `INCOMPLETE` |
| `probe` | exactly `char_bit`, `pointer_bits`, `endian`, `reflection`, `memcpy_object_lifetime`, `memcpy_array_lifetime`; a READY node requires `8`, `64`, `little`, and three `true` feature gates |
| `admission` | exactly the four stable keys mapped to JSON booleans |
| `signatures` | exactly the four stable keys mapped to non-empty strings copied byte-for-byte from `get_layout_signature<T>()` |
| `compiler` | exactly `family`, `revision`, `version`, `target`, `stdlib`, `xcode_version`, `xcode_build`, `sdk_version`, `sdk_build`, `deployment_target`, and `sdk_locked`; string values are non-empty, the revision matches the node's source lock, Linux uses literal `none` for the five Apple identity fields and `sdk_locked: true`, authoritative macOS values exactly match the hard lock with `sdk_locked: true`, and local macOS records actual values with a truthful boolean |
| `build` | exactly `profile`, `execution`, `runner`, `runner_image`, `source_sha`, `flags`, `workflow_run`, and `toolchain_artifact_sha256`; `profile` is `authoritative` or `local-arm64-macos`, `execution` is `native` or `emulated`, `source_sha` is 40 lowercase hex, and the other values are non-empty; `workflow_run` is `<run_id>.<run_attempt>` for GitHub Actions and the launcher invocation ID locally; `toolchain_artifact_sha256` is the selected Linux per-platform manifest digest without its `sha256:` prefix or the verified macOS archive SHA256; `runner_image` records observed `ImageOS`/`ImageVersion` on hosted runners or `personal-macos` locally but is never compared with either lock |
| `locks` | exactly `sources_sha256` and `outputs_sha256`, each 64 lowercase hex |
| `artifacts` | for `READY`, exactly `signature` and `region`; each holds the exact node filename and its 64-lowercase-hex SHA256; for `REJECT`, an empty object |

For an evaluated Admission rejection, `status` is `REJECT`, all four Admission/signature facts remain present, and payload artifacts are absent. For an unevaluable probe/build, the fallback schema has exactly `schema: 1`, the fixed `node`, `status: INCOMPLETE`, and a non-empty `error`; it must not invent Admission, signatures, compiler facts, lock hashes, or artifact hashes. Profile preparation and closure additionally require every complete record's four run-coherence fields to match the caller's expected source/run identity and the hashes of the supplied committed lock files.

`<consumer>.results.json` has exactly `schema`, `profile`, `consumer`, `consumer_provenance_sha256`, `build`, and `transfers`. `schema` is integer `1`; `profile` is one of the two declared profiles; and `consumer` belongs to that profile. `build` is null only in fallback results; otherwise it has exactly `source_sha`, `workflow_run`, `sources_sha256`, `outputs_sha256`, `execution`, `runner`, `runner_image`, `toolchain_artifact_sha256`, `compiler_family`, `compiler_revision`, `compiler_version`, `target`, `stdlib`, `flags`, `xcode_version`, `xcode_build`, `sdk_version`, `sdk_build`, `deployment_target`, and `sdk_locked` under the same value and node/profile rules as producer provenance. `prepare-consumer` obtains the compiler fields from that consumer job's fresh probe, requires its explicit toolchain artifact digest to equal the node's output-lock manifest/archive digest, derives the canonical locked flag identity from the validated node/toolchain mapping used to configure that build, and rejects any mismatch before generating the input header. Thus the independently compiled consumer cannot omit its full compiler configuration or borrow its producer job's environment identity. The authoritative profile contains the other five fixed nodes once each; the local profile contains the other four local nodes once each. Every transfer has exactly `producer`, `status`, `reason`, `producer_provenance_sha256`, and `region_sha256`; `reason` is non-empty and `status` is one of the six declared transfer statuses. Digest fields are either 64 lowercase hex or JSON null under these exact rules: `PASS` and all three loader rejections require both producer digests; `SKIPPED_TYPELAYOUT_REJECT` requires producer provenance but permits a null region digest; `INCOMPLETE` permits either missing digest. The top-level consumer provenance digest is required when that file exists and validated, otherwise it is null only in fallback `INCOMPLETE` results.

`agreements.json` has exactly `schema`, `profile`, `producer_provenance_sha256`, and `pairs`. The provenance map has every node in the selected profile exactly once; each value is the validated provenance-file SHA256 or JSON null only for an explicit `INCOMPLETE` slot. `pairs` contains every profile pair (15 authoritative or 10 local) and exactly four named decisions per pair; missing evidence creates an explicit `INCOMPLETE` decision rather than removing a pair.

`closure.json` has exactly `schema`, `profile`, `authoritative`, `run`, `agreements_sha256`, `expected`, `counts`, `missing`, `duplicates`, `status`, and `error`. `run` is either null in fallback `INCOMPLETE` closure or an object with exactly `source_sha`, `workflow_run`, `sources_sha256`, and `outputs_sha256`; those values follow the 40/nonnull/64/64 rules above and are the single common tuple verified across all complete producer and consumer records. `agreements_sha256` is 64 lowercase hex after a valid artifact is consumed and may be null only in fallback `INCOMPLETE` closure. `expected` preserves the profile's fixed node, unordered-pair, key, consumer, and directed-edge identities; `counts` has exactly `nodes`, `pairs`, `named_decisions`, `named_permits`, `consumers`, `transfers`, and `passes`; `missing` and `duplicates` retain explicit identity arrays. `status` is `PASS`, `REJECT`, or `INCOMPLETE`; `error` is null for an evaluated closure and a non-empty string for fallback. The six-node profile is authoritative only when all six producer provenance and all six consumer build records belong to `profile: authoritative`, use `execution: native`, record the node's locked `toolchain_artifact_sha256`, and have `sdk_locked: true`; the macOS records must additionally match the sealed Xcode version/build, SDK version/build, deployment target, and archive checksum. Observed runner-image strings do not affect that decision. The five-node profile always records `authoritative: false` and retains its actual personal-Mac SDK boundary.

The two toolchain locks are also strict, reject unknown/missing fields, and have no mutable-reference escape hatch:

- `toolchain-sources.lock` contains `schema`, exact GCC source and the four separately checksummed prerequisites, exact Bloomberg repository/commit, native target/runtime/configure flags, immutable base-image and apt-snapshot inputs, exact build-package versions, the expected Docker client/server versions for each Linux runner class, exact Buildx version, a digest-qualified BuildKit image, the seven Action commits, the per-macOS-node Xcode version/build, SDK version/build, and deployment target, plus a `recipes` map. `recipes` binds repository-normalized LF bytes for `.gitattributes`, both Dockerfiles, `docker-bake.hcl`, both macOS scripts, and `.github/workflows/toolchain-images.yml`. The GCC flags include `--disable-nls`; neither the lock nor a recipe may invoke `contrib/download_prerequisites`. The P2996 project set is exactly `clang`, with runtimes `libcxx;libcxxabi;libunwind`; `clang-tools-extra` is absent.
- `toolchains.lock` contains `schema`, the exact `sources_sha256`, and the candidate `source_sha`/`workflow_run` where workflow identity is `<run_id>.<run_attempt>`. Each Linux toolchain entry contains its private GHCR repository, one digest-qualified OCI index, and exactly two platform records (`linux/amd64` and `linux/arm64`) with their individual manifest digests. Each macOS node contains an immutable release-asset URL, archive SHA256, compiler target/revision and libc++ identity, plus the hard-locked Xcode version/build, SDK version/build, and deployment target. Its `observed_runner` object records non-empty `ImageOS`/`ImageVersion` only for diagnostics. Validation requires the exact two Linux manifests and all hard-lock fields but never compares a current hosted runner with `observed_runner`.

---

## File Structure

- `example/relocatable_world_demo/platform_probe.cpp`: capability and exact compiler/stdlib/target facts.
- `example/relocatable_world_demo/evidence_json.hpp`: minimal deterministic JSON string/integer/boolean emission shared by the two C++ evidence writers.
- `example/relocatable_world_demo/producer.cpp`: canonical `.region` plus current-build Admission/signature fact output.
- `example/relocatable_world_demo/consumer.cpp`: current-build four-key gate, profile-sized directed loads, and result JSON.
- `example/relocatable_world_demo/agreement_check.cpp`: P2996-independent Agreement artifact writer over generated fixed producer slots.
- `example/relocatable_world_demo/matrix_check.cpp`: P2996-independent closure logic over the validated Agreement and consumer inputs.
- `example/relocatable_world_demo/matrix_model.hpp`: fixed node/key sets, decision enums, uniqueness/counting, and closure precedence.
- `tools/relocatable_world_evidence.py`: schema-specific JSON validation, SHA256 sealing, fallback evidence, and constexpr header generation.
- `test/test_relocatable_world_evidence.py`: Python unit tests for malformed evidence and hash binding.
- `.github/docker/toolchain-sources.lock`: strict source-lock JSON generated from exact public inputs.
- `.github/docker/toolchains.lock`: separately committed sealed output-lock JSON.
- `.github/docker/Dockerfile.gcc16`: reproducible native GCC 16.2 builder/runtime image.
- `.github/docker/Dockerfile.p2996`: reproducible native Bloomberg Clang plus libc++ builder/runtime image.
- `.github/docker/docker-bake.hcl`: native architecture build definitions and immutable output names.
- `.github/scripts/bootstrap-toolchain-sources.py`: resolve official checksums and image base digests into a complete source lock.
- `.github/scripts/validate-toolchain-locks.py`: reject mutable, incomplete, inconsistent, or unsealed locks.
- `.github/scripts/build-p2996-macos.sh`: native archive build from the exact source lock.
- `.github/scripts/verify-p2996-toolchain.sh`: shared compile/run probe and macOS link/include verification.
- `.github/workflows/toolchain-images.yml`: native candidate build/publish and candidate output-lock artifact.
- `.github/workflows/relocatable-world-matrix.yml`: authoritative six producer/six consumer/Agreement/closure workflow.
- `tools/run-relocatable-world.sh`: ARM64 Mac 5/6 local workflow.
- `docs/talk/cppcon2026-relocatable-world-demo-notes.zh-CN.md`: evidence-backed appendix notes created only after full execution.
- `CLAUDE.md`: current locked commands and authoritative/non-authoritative boundary.

### Task 1: Freeze the Platform Probe and Evidence Validator

**Files:**
- Create: `example/relocatable_world_demo/platform_probe.cpp`
- Create: `example/relocatable_world_demo/evidence_json.hpp`
- Create: `tools/relocatable_world_evidence.py`
- Create: `test/test_relocatable_world_evidence.py`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: completed world contract and Python 3 standard library.
- Produces:
  - `relocatable_world_platform_probe NODE OUTPUT_JSON --runner LABEL --runner-image ID --xcode-version VALUE --xcode-build VALUE --sdk-version VALUE --sdk-build VALUE --deployment-target VALUE --sdk-locked true|false`
  - `python3 tools/relocatable_world_evidence.py validate-provenance FILE`
  - `python3 tools/relocatable_world_evidence.py fallback-provenance --node NODE --reason TEXT --output FILE`
  - `python3 tools/relocatable_world_evidence.py fallback-results --profile PROFILE --consumer NODE --reason TEXT --output FILE`
  - `python3 tools/relocatable_world_evidence.py fallback-agreements --profile PROFILE --reason TEXT --output FILE`
  - `python3 tools/relocatable_world_evidence.py fallback-closure --profile PROFILE --reason TEXT --output FILE`
  - `python3 tools/relocatable_world_evidence.py seal-producer --node NODE --profile authoritative|local-arm64-macos --execution native|emulated --probe FILE --facts FILE [--signature FILE --region FILE] --sources-lock FILE --outputs-lock FILE --toolchain-artifact-sha256 SHA --runner LABEL --source-sha SHA --workflow-run ID --output FILE`

- [ ] **Step 0: Capture the matrix protected-file baseline**

Run once after the core completion gate and before editing matrix files:

```powershell
New-Item -ItemType Directory -Force build-relocatable-world-matrix-baseline | Out-Null
git rev-parse HEAD | Set-Content -NoNewline build-relocatable-world-matrix-baseline/commit.txt
if ($LASTEXITCODE -ne 0) { throw "cannot record matrix baseline commit" }
git hash-object docs/talk/cppcon2026-main-deck-content-and-script.md | Set-Content -NoNewline build-relocatable-world-matrix-baseline/main-deck.sha
if ($LASTEXITCODE -ne 0) { throw "cannot hash pre-existing main-deck file" }
```

Keep this ignored manifest through Task 8. It protects committed history as well as the working tree and independently pins the pre-existing main-deck bytes.

- [ ] **Step 1: Write failing Python schema/hash tests**

Use `unittest` and temporary directories. Cover exact six-node membership, exact four-key membership, unknown/duplicate keys, malformed SHA256, filename escape, wrong Linux manifest/macOS archive digest, READY without both artifacts, REJECT with payload artifacts, INCOMPLETE with invented signatures, and one-byte artifact mutation after sealing:

```python
class EvidenceTests(unittest.TestCase):
    def test_ready_bundle_hashes_are_bound(self):
        bundle = self.make_ready_bundle("arm64_linux_gcc")
        seal_producer(bundle)
        validate_provenance(bundle / "arm64_linux_gcc.provenance.json")
        with (bundle / "arm64_linux_gcc.region").open("ab") as stream:
            stream.write(b"x")
        with self.assertRaises(EvidenceError):
            validate_provenance(bundle / "arm64_linux_gcc.provenance.json")

    def test_unknown_node_is_rejected(self):
        with self.assertRaises(EvidenceError):
            validate_node("linux_latest")
```

- [ ] **Step 2: Run the test and verify import/command failure**

```bash
python3 -m unittest -v test/test_relocatable_world_evidence.py
```

Expected: FAIL because `tools/relocatable_world_evidence.py` does not exist.

- [ ] **Step 3: Implement strict schemas with no third-party packages**

Define fixed tuples in the Python tool:

```python
NODES = (
    "x86_64_linux_gcc",
    "x86_64_linux_clang",
    "arm64_linux_gcc",
    "arm64_linux_clang",
    "arm64_macos_clang",
    "x86_64_macos_clang",
)
KEYS = (
    "WorldSnapshot",
    "Entity",
    "EntityRelativePtr",
    "EntityIndexEntry",
)
TRANSFER_STATUSES = (
    "PASS",
    "SKIPPED_TYPELAYOUT_REJECT",
    "REJECT_ENVELOPE",
    "REJECT_REGION",
    "REJECT_GRAPH",
    "INCOMPLETE",
)
```

Use `json`, `hashlib`, `pathlib`, and `argparse` only. Reject unknown top-level keys, non-canonical node/key sets, absolute or parent-traversal artifact names, duplicate JSON object keys through `object_pairs_hook`, and any digest not matching `[0-9a-f]{64}`. Each fallback command writes every identity required by its profile with `INCOMPLETE` status and null unavailable digests, so failure cannot shrink a graph. `seal-producer` requires probe node/Admission facts to equal the producer facts, verifies compiler family/revision/flags and profile-specific execution mapping against both locks, validates the supplied 40-hex TypeLayout source SHA, and requires `--toolchain-artifact-sha256` to equal the output lock's selected Linux per-platform manifest or macOS archive hash before recording it in `build`. For macOS it also validates actual Xcode version/build, SDK version/build, and explicit deployment target: authoritative evidence must match the hard lock with `sdk_locked: true`; local evidence records the personal Mac's actual values and may set the boolean false without becoming authoritative. It records `ImageOS`/`ImageVersion` through `runner_image` but never compares those observations with a lock. When all Admission values are true it requires both optional artifact arguments, parses the deterministic generated-header format, requires its node namespace, four keys, signatures, and byte-copy flags to agree exactly with the facts, and hashes both artifacts. When any Admission value is false it rejects either artifact argument and writes an empty `artifacts` object. The authoritative profile requires the exact native GitHub runner class for every node; the local profile permits only the Task 7 ARM64 Mac mapping and records its two x86-64 Linux nodes as emulated.

- [ ] **Step 4: Add the hard P2996/memcpy-lifetime probe**

The probe must compile only when all required features exist. Its required environment arguments are emitted by the locked Docker wrapper or macOS verifier; it records them verbatim beside compiler-derived version/target/stdlib facts so the Python validator can compare them with the node/profile lock policy:

```cpp
#include "world.hpp"

#include <bit>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <array>
#include <cstring>
#include <span>

static_assert(CHAR_BIT == 8);
static_assert(sizeof(void*) == 8);
static_assert(std::endian::native == std::endian::little);
constexpr auto reflected_int =
    boost::typelayout::get_layout_signature<int>();
static_assert(reflected_int.length() != 0);

int main(int argc, char** argv) {
    std::uint32_t one_source = argc == 0 ? 0u : 7u;
    alignas(std::uint32_t) std::byte one_storage[sizeof(one_source)]{};
    auto* one = static_cast<std::uint32_t*>(
        std::memcpy(one_storage, &one_source, sizeof(one_source)));
    *one += 5;

    std::array<std::uint32_t, 3> array_source{11, 13, 17};
    alignas(std::uint32_t)
        std::byte array_storage[sizeof(array_source)]{};
    auto* values = static_cast<std::uint32_t*>(
        std::memcpy(array_storage, array_source.data(), sizeof(array_source)));
    std::span<std::uint32_t> view(values, array_source.size());
    view[1] += *one;

    alignas(std::uint32_t)
        std::byte relocated_storage[sizeof(array_source)]{};
    auto* relocated = static_cast<std::uint32_t*>(std::memcpy(
        relocated_storage, array_storage, sizeof(array_storage)));
    const bool object_ok = *one == 12;
    const bool array_ok = relocated[0] == 11 && relocated[1] == 25 &&
        relocated[2] == 17 && relocated + 3 - relocated == 3;
    return write_probe_json(argc, argv, object_ok, array_ok);
}
```

Build and run this probe with the locked optimized flags, including `-O3 -fstrict-aliasing`; use a no-inline copy boundary in the implementation so both calls remain genuine source-to-distinct-destination copies without relying on a same-buffer trick. P0593 has no feature-test macro, so provenance records these behavioral results and must not claim a fabricated named compiler feature. The JSON records node, compiler version/revision macro, target triple, standard-library identity, all six boolean/numeric gates, the four local Admission decisions, and the caller-supplied runner/Apple observations. `evidence_json.hpp` emits object keys in the declared order and escapes quotes, reverse solidus, control characters, and compiler-version newlines; it implements no parser. CMake injects the locked compiler revision into the probe target; the verifier rejects a runtime compiler family, target, standard-library selection, or revision that disagrees with the node's lock mapping. `seal-producer` validates rather than blindly trusting the caller-supplied observations, then adds repository source SHA, workflow identity, exact locked flags, and the verified toolchain artifact digest.

- [ ] **Step 5: Register and run probe plus evidence tests**

Add `relocatable_world_platform_probe` linked to `relocatable_world_support`, then run:

```bash
python3 -m unittest -v test/test_relocatable_world_evidence.py
cmake --build build-relocatable-world-final --target relocatable_world_platform_probe --parallel
./build-relocatable-world-final/relocatable_world_platform_probe x86_64_linux_clang build-relocatable-world-final/probe.json --runner local-wsl --runner-image local-wsl --xcode-version none --xcode-build none --sdk-version none --sdk-build none --deployment-target none --sdk-locked true
python3 -m json.tool build-relocatable-world-final/probe.json
```

Expected stable summary: `TOOLCHAIN PROBE PASS node=x86_64_linux_clang` and six passed gates in JSON.

- [ ] **Step 6: Commit the evidence boundary**

```bash
git add -- CMakeLists.txt example/relocatable_world_demo/platform_probe.cpp example/relocatable_world_demo/evidence_json.hpp tools/relocatable_world_evidence.py test/test_relocatable_world_evidence.py
git update-index --chmod=+x tools/relocatable_world_evidence.py
git commit -m "feat: add relocatable world evidence protocol"
```

### Task 2: Add the Producer Bundle Role

**Files:**
- Create: `example/relocatable_world_demo/producer.cpp`
- Modify: `example/relocatable_world_demo/export_signatures.cpp`
- Modify: `tools/relocatable_world_evidence.py`
- Modify: `test/test_relocatable_world_evidence.py`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: canonical world builder, checkpoint writer, optional exporter platform ID, and Task 1 sealer.
- Produces:
  - `relocatable_world_producer NODE OUTPUT_DIRECTORY`
  - `<node>.producer-facts.json`
  - `<node>.region` when all four Admission decisions pass
  - `<node>.sig.hpp` from the existing exporter when admitted
  - sealed `<node>.provenance.json`

- [ ] **Step 1: Add a single-node bundle test**

The Python test creates a probe and facts file, invokes `seal-producer`, then requires the resulting provenance to contain exact keys, four Admission/signature entries, and artifact hashes. Add rejection tests proving a false Admission decision produces status `REJECT` and no `.sig.hpp`/`.region` requirement.

- [ ] **Step 2: Run and verify the producer target is absent**

```bash
cmake --build build-relocatable-world-final --target relocatable_world_producer --parallel
```

Expected: FAIL with unknown target.

- [ ] **Step 3: Implement current-build facts and canonical region output**

`producer.cpp` validates its node against the fixed set, uses the shared minimal writer to record all four `is_admitted_v<T, whole_region_relocation>` values and `get_layout_signature<T>()` strings as deterministic escaped JSON, and writes it through a same-directory temporary file plus rename. Only if all four Admission values are true does it build the canonical world and write `save_checkpoint(world)` through the same atomic pattern to `<node>.region`; it removes stale payload outputs before an evaluated rejection.

Its successful stdout is:

```text
PRODUCER READY node=<node> admission=4/4 region=<node>.region
```

On an evaluated rejection it writes facts and prints `PRODUCER REJECT node=<node> payload omitted` without creating payload files.

- [ ] **Step 4: Make exporter node naming explicit**

Keep local normal/packed defaults, but allow:

```bash
relocatable_world_export_ok OUTPUT_DIRECTORY arm64_linux_gcc
```

to write `arm64_linux_gcc.sig.hpp` in namespace `boost::typelayout::platform::arm64_linux_gcc`. Reject IDs outside the fixed six-node set for matrix invocations.

- [ ] **Step 5: Exercise one local producer output**

```bash
cmake --build build-relocatable-world-final --target relocatable_world_platform_probe relocatable_world_producer relocatable_world_export_ok --parallel
mkdir -p build-relocatable-world-final/evidence/x86_64_linux_clang
./build-relocatable-world-final/relocatable_world_platform_probe x86_64_linux_clang build-relocatable-world-final/evidence/x86_64_linux_clang/probe.json --runner local-wsl --runner-image local-wsl --xcode-version none --xcode-build none --sdk-version none --sdk-build none --deployment-target none --sdk-locked true
./build-relocatable-world-final/relocatable_world_producer x86_64_linux_clang build-relocatable-world-final/evidence/x86_64_linux_clang
./build-relocatable-world-final/relocatable_world_export_ok build-relocatable-world-final/evidence/x86_64_linux_clang x86_64_linux_clang
python3 -m json.tool build-relocatable-world-final/evidence/x86_64_linux_clang/x86_64_linux_clang.producer-facts.json >/dev/null
```

The final provenance cannot be sealed until Task 4 creates the source lock and Task 5 commits the separately reviewed output lock. Until then, only Python unit tests use complete synthetic lock fixtures inside temporary directories; do not create an unsealed repository lock.

- [ ] **Step 6: Run tests and commit producer role**

```bash
python3 -m unittest -v test/test_relocatable_world_evidence.py
ctest --test-dir build-relocatable-world-final -R "^test_relocatable_" --output-on-failure
git add -- CMakeLists.txt example/relocatable_world_demo/producer.cpp example/relocatable_world_demo/export_signatures.cpp tools/relocatable_world_evidence.py test/test_relocatable_world_evidence.py
git commit -m "feat: add relocatable world producer role"
```

### Task 3: Add Consumer Transfers and P2996-Independent Matrix Closure

**Files:**
- Create: `example/relocatable_world_demo/matrix_model.hpp`
- Create: `example/relocatable_world_demo/consumer.cpp`
- Create: `example/relocatable_world_demo/agreement_check.cpp`
- Create: `example/relocatable_world_demo/matrix_check.cpp`
- Modify: `tools/relocatable_world_evidence.py`
- Modify: `test/test_relocatable_world_evidence.py`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: the selected profile's strict producer provenance slots and the world checkpoint loader.
- Produces:
  - `prepare-consumer --profile authoritative|local-arm64-macos --consumer NODE --evidence DIR (--consumer-probe FILE --toolchain-artifact-sha256 SHA --expect-source-sha SHA --expect-workflow-run ID --sources-lock FILE --outputs-lock FILE | --fixture-context) --output-header FILE`
  - `relocatable_world_consumer PROFILE NODE EVIDENCE_DIR RESULTS_JSON`
  - `prepare-agreements --profile authoritative|local-arm64-macos --evidence DIR (--expect-source-sha SHA --expect-workflow-run ID --sources-lock FILE --outputs-lock FILE | --fixture-context) --output-header FILE`
  - `relocatable_world_agreement_check AGREEMENTS_JSON`
  - `prepare-matrix --profile authoritative|local-arm64-macos --evidence DIR --results DIR --agreements FILE (--expect-source-sha SHA --expect-workflow-run ID --sources-lock FILE --outputs-lock FILE | --fixture-context) --output-header FILE`
  - `relocatable_world_matrix_check CLOSURE_JSON`
  - `relocatable_world_matrix_check --self-test`
  - `audit-run --directory DIR --expect-source-sha SHA [--expect-workflow-run ID] --sources-lock FILE --outputs-lock FILE --expect-nodes N --expect-pairs N --expect-named-permits N --expect-transfers N`

- [ ] **Step 1: Add synthetic graph and precedence tests**

In `matrix_model.hpp`, expose pure functions over fixed-size records. Add C++ self-test cases with six complete matching nodes and 30 PASS transfers; then mutate one case at a time:

```cpp
assert(close_matrix(complete_matching_fixture()).status == closure_status::pass);
assert(close_matrix(with_missing_node()).status == closure_status::incomplete);
assert(close_matrix(with_signature_difference()).status == closure_status::reject);
assert(close_matrix(with_reject_and_missing()).status == closure_status::incomplete);
assert(rejects_duplicate_node());
assert(rejects_duplicate_pair());
assert(rejects_duplicate_directed_edge());
assert(rejects_self_edge());
assert(close_matrix(complete_local_fixture()).status == closure_status::pass);
assert(!close_matrix(complete_local_fixture()).authoritative);
```

Expected self-test output:

```text
SELFTEST PASS: nodes=6 agreements=15 named=60 transfers=30
SELFTEST PASS: local_nodes=5 agreements=10 named=40 transfers=20 authoritative=false
SELFTEST PASS: incomplete/reject/pass precedence
```

- [ ] **Step 2: Add generated constexpr evidence instead of parsing C++ headers**

In production mode, `prepare-consumer` first validates a fresh platform-probe file from the consumer job itself against the node, profile, expected source/run context, committed locks, and SDK policy. It requires the probe's compiler family, revision, version, target, standard library, runner class, and hard-lock Apple fields to match that consumer node, while retaining the observed runner-image string without comparing it to a lock. It requires `--toolchain-artifact-sha256` to equal that node's output-lock Linux per-platform manifest or macOS archive digest and binds it into the consumer build record. It then derives the exact canonical compile-flag identity selected from the same validated lock mapping and validates available producer provenance and artifact hashes. It generates `relocatable_world_consumer_input.hpp` with those complete verified consumer build facts plus the profile's fixed slots (six authoritative or five local) containing `present`, node ID, provenance digest, four Admission booleans, four signature strings, and verified region path. Missing, mixed-run, or invalid bundles remain explicit `present=false` slots with an error reason; the consumer executable copies the generated build facts into its result JSON.

In production mode, `prepare-agreements` validates the profile's fixed producer slots against the expected source SHA, workflow run, and lock hashes, then generates `relocatable_world_agreement_input.hpp` for `agreement_check.cpp`. `prepare-matrix` separately validates those same run-coherence invariants across the fixed producer and consumer slots plus the required `agreements.json`, rejects profile/execution mismatches, recomputes the expected Agreement decisions from producer provenance, and requires exact equality with the uploaded file before generating `relocatable_world_matrix_input.hpp` for `matrix_check.cpp`. The generated closure input retains the common run identity, Agreement-file SHA256, and all fixed pair/decision slots. This keeps both programs runnable when no reflection compiler works, prevents closure from bypassing a missing or stale Agreement job, and avoids a general JSON parser in C++.

Task 3 predates the real locks, so all three preparation commands also expose one deliberately narrow `--fixture-context` mode. It is mutually exclusive with the four production context arguments, accepts only empty producer/result directories plus fixed fallback Agreement data, and generates explicit `present=false`/`INCOMPLETE` slots for compilation and self-tests. It rejects any READY/REJECT provenance, payload, or consumer result. Production workflows and the local launcher are statically forbidden from using it; this avoids either inventing a repository lock early or weakening real evidence checks.

- [ ] **Step 3: Implement consumer gate order and five records**

For each producer other than self:

```text
missing/invalid evidence -> INCOMPLETE
local or producer Admission false, or signature differs -> SKIPPED_TYPELAYOUT_REJECT
otherwise call load_checkpoint
checkpoint_error::envelope -> REJECT_ENVELOPE
checkpoint_error::region -> REJECT_REGION
checkpoint_error::graph -> REJECT_GRAPH
canonical graph/business mismatch -> REJECT_GRAPH
otherwise -> PASS
```

The consumer writes exactly every other producer in the selected profile (five for authoritative, four for local), binds every available provenance/region hash under the conditional schema above, and never calls the loader after a TypeLayout rejection.

- [ ] **Step 4: Implement exact Agreement and closure counts**

For every sorted unordered pair, `agreement_check.cpp` writes four named decisions:

```text
INCOMPLETE if required Admission/signature evidence is absent or invalid
REJECT if either node's Admission for that key is false
REJECT if both signatures are present and unequal
PERMIT otherwise
```

`matrix_check.cpp` then validates the selected profile's exact identities and counts: authoritative requires 6 unique nodes, 15 pairs, 60 named decisions, 6 consumers, and 30 non-self directed transfers; local requires its fixed 5 nodes, 10 pairs, 40 named decisions, 5 consumers, and 20 non-self directed transfers. Closure requires the validated Agreement artifact, its complete profile-sized provenance-digest map, exact equality between its decisions and a recomputation from the producer slots, and one common source SHA/workflow-run/lock-hash tuple matching the caller's expected context across both producer provenance and consumer build records. The local result always has `authoritative == false`; the authoritative result becomes true only when all six producer and six consumer build records are native, SDK-locked evidence from `${{ github.sha }}` and `${{ github.run_id }}.${{ github.run_attempt }}` with both committed lock hashes. Final status is `INCOMPLETE` if anything is missing/invalid; otherwise `REJECT` if any named decision or transfer rejects; otherwise `PASS`.

Implement `audit-run` over the same strict parsers. It verifies producer and region hashes, every producer/consumer `toolchain_artifact_sha256` against the node's locked per-platform manifest/archive, every consumer build record including compiler family/revision/version/target/stdlib/flags and the hard-locked Apple identities, the Agreement artifact hash recorded by closure, Agreement-to-provenance digest bindings, recomputed Agreement equality, the expected source SHA, one common workflow-run identity including GitHub attempt (and the explicit expected ID when supplied), both supplied lock-file hashes, fixed identities, duplicate/missing slots, closure status, and caller-supplied counts; it returns non-zero for any mismatch. The retained flat directory must contain `source-sha.txt` plus exactly one profile-specific run-identity file: `workflow-run.txt` for authoritative evidence or `run-id.txt` for local evidence. Both files are mandatory, must contain one exact matching line, and the other profile's run-identity filename is forbidden. `runner_image` remains retained diagnostic context only. Add Python tests for one passing authoritative directory and for wrong count, duplicate identity, altered producer artifact, mixed source SHA, mixed workflow run or attempt, missing/wrong/profile-incompatible metadata, mismatched lock hash, wrong Linux manifest digest, wrong macOS archive digest, consumer compiler/version/flags/Xcode-build/SDK-build mismatch, missing/altered/stale Agreement artifact, non-PERMIT decision, non-PASS transfer, and a closure file inconsistent with its inputs.

- [ ] **Step 5: Register dual-language targets**

`relocatable_world_consumer` links `relocatable_world_support`; `relocatable_world_agreement_check` and `relocatable_world_matrix_check` are separate C++20-only targets sharing `matrix_model.hpp`. CMake cache entries select node/profile/evidence/results/agreement paths and default to `x86_64_linux_clang`, `authoritative`, and empty directories under the build tree. In the default fixture mode, one custom command runs `prepare-consumer --profile authoritative --consumer x86_64_linux_clang --evidence <empty-evidence-dir> --fixture-context` and writes `relocatable_world_consumer_input.hpp`; a second runs `prepare-agreements --fixture-context` and writes `relocatable_world_agreement_input.hpp`; a third first creates a fixed-slot fallback Agreement when no file is configured, then runs `prepare-matrix --fixture-context` and writes `relocatable_world_matrix_input.hpp`. Each executable has an explicit generated-header dependency and includes the generated directory. The `TYPELAYOUT_RELOCATABLE_WORLD_EVIDENCE_MODE` cache string is exactly `fixture` or `production`; production requires non-empty `TYPELAYOUT_RELOCATABLE_WORLD_CONSUMER_PROBE`, `TYPELAYOUT_RELOCATABLE_WORLD_TOOLCHAIN_ARTIFACT_SHA256`, `TYPELAYOUT_RELOCATABLE_WORLD_SOURCE_SHA`, `TYPELAYOUT_RELOCATABLE_WORLD_WORKFLOW_RUN`, `TYPELAYOUT_RELOCATABLE_WORLD_SOURCES_LOCK`, and `TYPELAYOUT_RELOCATABLE_WORLD_OUTPUTS_LOCK` cache paths/values, passes the artifact value as `prepare-consumer --toolchain-artifact-sha256`, and switches all three preparation commands to those real arguments. The defaults deliberately generate six explicit missing slots so all three executables remain compilable before real evidence exists. Add a build test that deletes all three generated headers, builds the three targets, and proves CMake regenerates each header before compiling. Also document a standalone self-test build:

```bash
mkdir -p build/empty-producers build/empty-results build/fallback build/generated
python3 tools/relocatable_world_evidence.py fallback-agreements --profile authoritative --reason "no evidence" --output build/fallback/agreements.json
python3 tools/relocatable_world_evidence.py prepare-matrix --profile authoritative --evidence build/empty-producers --results build/empty-results --agreements build/fallback/agreements.json --fixture-context --output-header build/generated/relocatable_world_matrix_input.hpp
c++ -std=c++20 -Iexample/relocatable_world_demo -Ibuild/generated example/relocatable_world_demo/matrix_check.cpp -o build/relocatable_world_matrix_check
./build/relocatable_world_matrix_check --self-test
```

- [ ] **Step 6: Run self-tests and malformed evidence tests**

```bash
python3 -m unittest -v test/test_relocatable_world_evidence.py
cmake --build build-relocatable-world-final --target relocatable_world_consumer relocatable_world_agreement_check relocatable_world_matrix_check --parallel
./build-relocatable-world-final/relocatable_world_matrix_check --self-test
```

Expected: all three SELFTEST lines and all Python tests PASS.

- [ ] **Step 7: Commit consumer and closure logic**

```bash
git add -- CMakeLists.txt example/relocatable_world_demo/matrix_model.hpp example/relocatable_world_demo/consumer.cpp example/relocatable_world_demo/agreement_check.cpp example/relocatable_world_demo/matrix_check.cpp tools/relocatable_world_evidence.py test/test_relocatable_world_evidence.py
git commit -m "test: add strict relocatable world matrix closure"
```

### Task 4: Define Immutable Toolchain Sources and Native Build Recipes

**Files:**
- Create: `.gitattributes`
- Create: `.github/scripts/bootstrap-toolchain-sources.py`
- Create: `.github/scripts/validate-toolchain-locks.py`
- Create: `.github/docker/toolchain-sources.lock`
- Create: `.github/docker/docker-bake.hcl`
- Create: `.github/scripts/build-p2996-macos.sh`
- Create: `.github/scripts/verify-p2996-toolchain.sh`
- Modify: `.github/docker/Dockerfile.gcc16`
- Modify: `.github/docker/Dockerfile.p2996`
- Modify: `test/test_relocatable_world_evidence.py`

**Interfaces:**
- Consumes: official GCC 16.2.0 release, Bloomberg commit `060be17654102019e14810c3f948ef85a490755f`, runner-image inventory commit `564e58dbe650c507ccba1171f6159c12f26820c8`, immutable base-image/BuildKit digests, and exact Docker client/server/Buildx versions.
- Produces:
  - a complete committed source lock
  - two native multi-platform Linux image recipes
  - two native macOS archive recipes
  - shared probe verification
  - validated `--print-image gcc16|p2996` lookup for immutable OCI index references

- [ ] **Step 1: Add lock rejection tests before creating a repository lock**

Test empty digest, `latest`, branch-only `p2996`, short commit, non-digest base or BuildKit image, missing/mismatched Docker client/server/Buildx version, mismatched source-lock hash, changed/missing build-recipe hash including the workflow LF hash, missing LF attribute, CRLF-normalization equivalence, missing/extra/duplicate platform manifest, provenance using the wrong per-platform manifest/archive digest, mutable macOS URL, missing archive hash, missing Xcode or SDK version/build, missing deployment target, accidental `clang-tools-extra`, `download_prerequisites`, and duplicate node mapping. Also prove that changing only observed `ImageOS`/`ImageVersion` does not invalidate an otherwise identical hard lock. Every invalid fixture must make `validate-toolchain-locks.py` return non-zero. Test that `--print-image gcc16` and `--print-image p2996` emit their exact digest-qualified private-index references only after both locks validate.

- [ ] **Step 2: Implement deterministic source-lock bootstrap**

Create `.gitattributes` with these exact policies before hashing recipes:

```gitattributes
.github/docker/** text eol=lf
.github/scripts/** text eol=lf
.github/workflows/** text eol=lf
tools/*.py text eol=lf
tools/*.sh text eol=lf
```

`bootstrap-toolchain-sources.py` must:

```text
fetch the official gcc-16.2.0 release checksum list over HTTPS
select gcc-16.2.0.tar.xz and record its SHA512
record exact GMP, MPFR, MPC, and ISL prerequisite tarballs and SHA512 values instead of running an unchecked prerequisite downloader
record the exact Bloomberg commit 060be17654102019e14810c3f948ef85a490755f
resolve every Docker base image to an OCI sha256 digest
record snapshot repository timestamp, exact build-package versions, configure flags (including GCC `--disable-nls`), enabled LLVM targets, the exact `clang`-only project set, libc++ runtimes, dependency versions, source URLs, `actions/runner-images` commit 564e58dbe650c507ccba1171f6159c12f26820c8 plus the selected macOS Xcode version/build, SDK version/build, and deployment target from its inventory, and the seven reviewed Action commits from Task 5
record the exact Docker client/server versions for each Linux runner class and the common exact Buildx version plus digest-qualified BuildKit image; candidate jobs must verify the reported identities before building
record SHA256 of repository-normalized LF bytes for `.gitattributes`, the final Dockerfiles, bake file, macOS build script, shared toolchain verification script, and `.github/workflows/toolchain-images.yml` so the source-lock digest changes whenever recipe, publication semantics, or normalization policy changes
write sorted/indented JSON atomically
refuse redirects to a different host, mutable tags in output, or an unresolved digest
```

Do not emit the committed lock yet: Steps 3-6 must first finish the recipe bytes that it hashes. Unit tests use temporary synthetic recipe files until then.

- [ ] **Step 3: Rewrite GCC image for native 16.2 source builds**

Remove the unchecked rolling `.deb`. Install build packages only from the locked snapshot repository, fetch GMP/MPFR/MPC/ISL directly from the locked URLs, verify every compiler/prerequisite archive against the source lock before extraction, and never invoke `contrib/download_prerequisites`. Configure GCC with the locked flags including `--disable-nls`, compute parallelism with the same 2-GiB-per-compile-job formula used below, build GCC and matching libstdc++ on the current native architecture, and install with GCC's `install-strip` target. Make `gcc --version`, target triple, `__GLIBCXX__`, P2996 reflection, and both optimized `std::memcpy` lifetime cases part of the final image probe.

- [ ] **Step 4: Rewrite P2996 image for native X86/AArch64 builds**

Fetch the exact commit rather than shallow-cloning a branch. Install build packages only from the locked snapshot repository. Build exactly the `clang` project with `libcxx;libcxxabi;libunwind`; do not build `clang-tools-extra`. Select `LLVM_TARGETS_TO_BUILD=X86` on amd64 and `AArch64` on arm64, disable tests/examples/benchmarks, and use Release. Compute build parallelism as `max(1, min(hardware_threads, floor(available_memory_bytes / 2_GiB)))`, set `LLVM_PARALLEL_LINK_JOBS=1`, and run `cmake --install <build-dir> --strip` for only the required toolchain. Final runtime includes the matching libc++ headers/libs and rejects fallback to the system standard library.

- [ ] **Step 5: Add native Buildx definitions**

`docker-bake.hcl` exposes `gcc16-amd64`, `gcc16-arm64`, `p2996-amd64`, and `p2996-arm64` native targets. The workflow creates its builder from the locked BuildKit image and executes every bake target through that builder with `provenance=false` and `sbom=false`; it combines each toolchain's two per-architecture manifests into an OCI index containing exactly `linux/amd64` and `linux/arm64`, with no attestation or unknown extra manifest. Do not emulate compiler builds.

- [ ] **Step 6: Add native macOS build and verification scripts**

`build-p2996-macos.sh` accepts the source lock and output directory, checks host architecture against the requested node, selects and verifies the locked Xcode version/build, SDK version/build, and deployment target, and builds the exact commit with the same `clang`-only project/runtime/resource contract as Linux. It uses the same 2-GiB-per-compile-job formula, sets `LLVM_PARALLEL_LINK_JOBS=1`, installs with `cmake --install <build-dir> --strip`, rejects an archive of 2 GiB or larger, packages one `.tar.zst`, and emits both hard-lock metadata and observed GitHub `ImageOS`/`ImageVersion`.

`verify-p2996-toolchain.sh` accepts the output lock and node plus either `--require-locked-sdk` or `--allow-unlocked-sdk`; it verifies the archive checksum before extraction, records actual Xcode version/build, SDK version/build, and deployment target, sets `DEVELOPER_DIR`, and emits explicit `-isysroot` and deployment flags. It compiles/runs `platform_probe.cpp` with archive-relative libc++ headers and link paths. Verification must prove all of the following: the effective C++ include search selects the bundled `include/c++/v1` and no host libc++ headers; the target triple is correct; `otool -L` identifies the intended libc++/libc++abi install names; every executable rpath resolves inside the extracted archive; and a real probe run with `DYLD_PRINT_LIBRARIES=1` reports the bundled libc++/libc++abi paths rather than the host copies. Required mode rejects any hard-lock or actual-load mismatch. Local allowed mode emits actual values and truthful `sdk_locked`; observed runner-image metadata never affects either mode. Reproducibility claims attach to the archive SHA256 plus the verified Xcode/SDK/deployment/runtime contract, not to a hosted runner image string.

- [ ] **Step 7: Run static and lock tests**

```bash
docker buildx bake -f .github/docker/docker-bake.hcl --print
bash -n .github/scripts/build-p2996-macos.sh
bash -n .github/scripts/verify-p2996-toolchain.sh
python3 .github/scripts/bootstrap-toolchain-sources.py --gcc-version 16.2.0 --clang-commit 060be17654102019e14810c3f948ef85a490755f --recipe-root . --output .github/docker/toolchain-sources.lock
python3 .github/scripts/validate-toolchain-locks.py --sources .github/docker/toolchain-sources.lock --recipe-root .
python3 -m json.tool .github/docker/toolchain-sources.lock >/dev/null
python3 -m unittest -v test/test_relocatable_world_evidence.py
```

Expected: all pass; bake output names four native build targets and no mutable image reference; the validator verifies the Docker/Buildx/BuildKit identities and prints `SOURCE LOCK PASS gcc=16.2.0 clang=060be17654102019e14810c3f948ef85a490755f recipes=7`.

- [ ] **Step 8: Commit source locks and recipes**

```bash
git add -- .gitattributes .github/docker/Dockerfile.gcc16 .github/docker/Dockerfile.p2996 .github/docker/docker-bake.hcl .github/docker/toolchain-sources.lock .github/scripts/bootstrap-toolchain-sources.py .github/scripts/validate-toolchain-locks.py .github/scripts/build-p2996-macos.sh .github/scripts/verify-p2996-toolchain.sh test/test_relocatable_world_evidence.py
git update-index --chmod=+x .github/scripts/build-p2996-macos.sh .github/scripts/verify-p2996-toolchain.sh
git commit -m "build: define locked P2996 toolchain sources"
```

This commit's source lock is complete for the current recipe bytes. Because the publication workflow itself is a hashed recipe, Task 5 must regenerate and revalidate the lock after rewriting that workflow and before dispatching any candidate build.

### Task 5: Publish Native Candidates and Seal `toolchains.lock`

**Files:**
- Modify: `.github/workflows/toolchain-images.yml`
- Regenerate: `.github/docker/toolchain-sources.lock` after the final workflow bytes are known
- Modify: `test/test_relocatable_world_evidence.py`
- Create after successful workflow: `.github/docker/toolchains.lock`

**Interfaces:**
- Consumes: Task 4 source lock and four native GitHub runner classes.
- Produces: two OCI index digests, four per-platform image digests, two immutable macOS release assets/checksums, and one candidate output lock.

- [ ] **Step 1: Rewrite candidate workflow with native jobs**

Keep the existing workflow manually dispatchable without introducing a branch-only input contract; the dispatching step below identifies the unique newly created run for the exact commit. Candidate build/index/archive jobs run only for `workflow_dispatch`. A default-branch push runs a separate promotion path that consumes the already committed output lock and never rebuilds a compiler or archive. Set top-level permissions to `contents: read`. Linux manifest build/index and alias-promotion jobs alone override with the exact job-level map `{contents: read, packages: write}`; the release publication job alone uses `{contents: write}`; all other jobs inherit read-only contents. No job receives `id-token: write`, no candidate job changes GHCR visibility, and the two packages remain private. Add workflow-level `concurrency.group: typelayout-toolchain-publication` with `cancel-in-progress: false`; the fixed cross-ref group serializes all canonical-tag, release-asset, and legacy-alias mutations rather than permitting check-then-publish races. Use exactly six native candidate build jobs, followed by index/sealing jobs:

```text
build_gcc_amd64     ubuntu-24.04      -> GCC amd64 image
build_p2996_amd64   ubuntu-24.04      -> P2996 amd64 image
build_gcc_arm64     ubuntu-24.04-arm  -> GCC arm64 image
build_p2996_arm64   ubuntu-24.04-arm  -> P2996 arm64 image
build_p2996_macos_arm64  macos-15       -> P2996 ARM64 archive
build_p2996_macos_x86_64 macos-15-intel -> P2996 x86-64 archive
```

Use `strategy.fail-fast: false`. Pin the required Actions to these reviewed full commits:

```text
actions/checkout@11d5960a326750d5838078e36cf38b85af677262
actions/upload-artifact@ea165f8d65b6e75b540449e92b4886f43607fa02
actions/download-artifact@d3f86a106a0bac45b974a628896c90dbdf5c8093
docker/login-action@c94ce9fb468520275223c153574b00df6fe4bcc9
docker/setup-buildx-action@8d2750c68a42422c14e847fe6c8ac0403b4cbd6f
docker/build-push-action@10e90e3645eae34f1e60eeb005ba3a3d33f178e8
softprops/action-gh-release@3bb12739c298aeb8a4eeaf626c5b8d85266b0e65
```

Compute the lowercase SHA256 of the committed source-lock bytes. Before building, verify the runner's Docker client/server and installed Buildx versions against the source lock and create Buildx with the locked digest-qualified BuildKit driver image. Each native Linux architecture job uses `push-by-digest=true,name-canonical=true,push=true`, `provenance=false`, and `sbom=false`, then returns that platform's immutable manifest digest; it does not write the shared canonical tag. Only the per-toolchain index job may create `ghcr.io/ximicpp/typelayout-gcc16:source-<sources_sha256>` or `ghcr.io/ximicpp/typelayout-p2996:source-<sources_sha256>` after both architecture digests exist. The index is constructed from exactly those two digest-qualified inputs. Inspect its raw manifest list and require exactly one `linux/amd64` and one `linux/arm64` image manifest, with no attestation, SBOM, duplicate, or third entry. If the canonical tag already exists, compare its exact two-entry platform-to-manifest-digest map with the intended pair and reuse it only on byte-for-byte identity; otherwise fail instead of overwriting it. Consume and record only digest-qualified references.

Run the common probe on all six candidates. Publish macOS archives named `p2996-macos-arm64-<clang_commit>.tar.zst` and `p2996-macos-x86_64-<clang_commit>.tar.zst` under release tag `typelayout-toolchains-<sources_sha256>`. The release step sets `target_commitish: ${{ github.sha }}` and `overwrite_files: false`; it fails if the tag points elsewhere or an asset already exists, rather than replacing bytes. Each archive must be smaller than 2 GiB and pass the bundled-libc++ include/link/rpath/`DYLD_PRINT_LIBRARIES` verification before publication. Emit a workflow artifact named `candidate-toolchains-lock` containing exactly one candidate file named `toolchains.lock`. The file records `${{ github.sha }}`, `${{ github.run_id }}.${{ github.run_attempt }}`, both per-platform manifest digests, both canonical index digests, both macOS archive checksums, Xcode version/build, SDK version/build, deployment target, compiler target, and libc++ identities. It also records actual `ImageOS`/`ImageVersion` under `observed_runner`, but the validator treats those strings as non-locking diagnostics. Updating any Action pin or the workflow bytes is a reviewed source-input change.

Preserve the repository's pre-existing mutable-image compatibility path without rebuilding sealed candidates after merge. On a push to the repository default branch, the promotion job validates the committed source/output lock pair, authenticates to the private registry, obtains the existing digest-qualified indexes from `toolchains.lock`, verifies those remote digests still expose exactly the locked two per-platform manifests, and never invokes the six candidate build jobs or republishes macOS assets. Before repointing `typelayout-gcc16:latest` and `typelayout-p2996:latest`, run the exact x86-64 Linux container contracts used by `ci.yml` and `compat-pipeline.yml` against those digests: Debug and Release configure/build/full CTest under P2996 with `LD_LIBRARY_PATH=/opt/p2996-toolchain/lib/x86_64-unknown-linux-gnu`; GCC and Clang `compat_ci_export`; the combined `compat_ci_check_linux`; and `compat_check_demo_negative` with both expected diagnostics. Explicitly require the legacy P2996 library directory to exist. Only a fully passing promotion may move the aliases. These aliases exist solely for unchanged legacy regression jobs; candidate review, the authoritative matrix, the local launcher, provenance, and claims never consume them.

- [ ] **Step 2: Ensure sealing does not retrigger candidates and validate the workflow statically**

Default-branch path filters include `.gitattributes`, Dockerfiles, both locks, the bake file, build/verify scripts, and the workflow itself. The event guards ensure such a push can run only validation/promotion, never candidate builds; sealing an output lock therefore cannot rebuild a supposedly identical archive on a newer hosted runner. Extend the Python workflow tests to require the fixed non-cancelling publication concurrency group, least job-level `contents`/`packages` permissions, private authenticated pulls/pushes with no visibility mutation, all six dispatch-only native build jobs, locked Docker/Buildx/BuildKit verification, `provenance: false`, `sbom: false`, digest-only architecture pushes, one index writer per toolchain, exact two-manifest checks, per-platform digest capture, canonical-tag identity checks, archive size/stripped-install/link-job/runtime-library verification, `target_commitish: ${{ github.sha }}`, `overwrite_files: false`, all probes before candidate-lock output, a push-only promotion that consumes the committed output lock without build/release jobs, exact legacy path and CI/compat smoke commands before `latest`, and no authoritative/local evidence command consuming `latest`.

```bash
python3 .github/scripts/bootstrap-toolchain-sources.py --gcc-version 16.2.0 --clang-commit 060be17654102019e14810c3f948ef85a490755f --recipe-root . --output .github/docker/toolchain-sources.lock
python3 .github/scripts/validate-toolchain-locks.py --sources .github/docker/toolchain-sources.lock --recipe-root .
python3 -m unittest -v test/test_relocatable_world_evidence.py
```

Expected: the regenerated lock binds the final workflow LF bytes, and workflow shape plus immutable-reference tests PASS before publication. Any subsequent workflow edit requires another regeneration before commit.

- [ ] **Step 3: Commit and publish the candidate workflow definition**

The remote dispatcher can run only a committed workflow. Once the workflow bytes and regenerated source lock from Step 2 are final, validate once more, commit both, then push the current branch:

```bash
python3 .github/scripts/validate-toolchain-locks.py --sources .github/docker/toolchain-sources.lock --recipe-root .
git add -- .github/workflows/toolchain-images.yml .github/docker/toolchain-sources.lock test/test_relocatable_world_evidence.py
git commit -m "ci: build native P2996 toolchain candidates"
git push -u origin codex/cppcon2026-deck
```

- [ ] **Step 4: Run candidate publication**

```bash
head_sha="$(git rev-parse HEAD)"
run_state_dir="$(mktemp -d)"
trap 'rm -rf "$run_state_dir"' EXIT
gh run list --workflow toolchain-images.yml --branch codex/cppcon2026-deck --commit "$head_sha" --event workflow_dispatch --limit 100 --json databaseId --jq '.[].databaseId' | sort -n > "$run_state_dir/before.ids"
gh workflow run toolchain-images.yml --ref codex/cppcon2026-deck
run_id=""
attempt=0
while test "$attempt" -lt 60; do
  attempt=$((attempt + 1))
  gh run list --workflow toolchain-images.yml --branch codex/cppcon2026-deck --commit "$head_sha" --event workflow_dispatch --limit 100 --json databaseId --jq '.[].databaseId' | sort -n > "$run_state_dir/after.ids"
  comm -13 "$run_state_dir/before.ids" "$run_state_dir/after.ids" > "$run_state_dir/new.ids"
  new_count="$(wc -l < "$run_state_dir/new.ids")"
  if test "$new_count" -eq 1; then
    run_id="$(tr -d '[:space:]' < "$run_state_dir/new.ids")"
    break
  fi
  if test "$new_count" -gt 1; then
    echo "ambiguous candidate runs for $head_sha" >&2
    exit 1
  fi
  sleep 5
done
test -n "$run_id"
gh run watch "$run_id" --exit-status
gh run download "$run_id" -n candidate-toolchains-lock -D build/toolchain-candidate
run_attempt="$(gh api "repos/{owner}/{repo}/actions/runs/$run_id" --jq .run_attempt)"
workflow_run="$run_id.$run_attempt"
test "$workflow_run" = "$(python3 -c 'import json; print(json.load(open("build/toolchain-candidate/toolchains.lock"))["workflow_run"])')"
```

The loop tolerates up to five minutes of GitHub indexing delay; zero new IDs waits, one selects that exact run, and more than one aborts as ambiguous. It never chooses an arbitrary recent run, and the downloaded lock must identify that run's exact attempt. Expected: all six native probes PASS and the candidate lock identifies two private multi-platform indexes plus two macOS archives.

- [ ] **Step 5: Verify candidate outputs independently**

```bash
python3 .github/scripts/validate-toolchain-locks.py --sources .github/docker/toolchain-sources.lock --outputs build/toolchain-candidate/toolchains.lock --recipe-root .
gcc_image="$(python3 .github/scripts/validate-toolchain-locks.py --sources .github/docker/toolchain-sources.lock --outputs build/toolchain-candidate/toolchains.lock --recipe-root . --print-image gcc16)"
clang_image="$(python3 .github/scripts/validate-toolchain-locks.py --sources .github/docker/toolchain-sources.lock --outputs build/toolchain-candidate/toolchains.lock --recipe-root . --print-image p2996)"
docker buildx imagetools inspect "$gcc_image"
docker buildx imagetools inspect "$clang_image"
```

`validate-toolchain-locks.py --print-image` emits an image reference only after the complete lock pair passes validation. Authenticate before inspecting the private packages. Inspect both raw indexes and require exactly the two locked `linux/amd64` and `linux/arm64` manifest digests, with no extra descriptors. Download both macOS assets, verify SHA256 and the `<run_id>.<run_attempt>` lock identity, then on matching native runners verify the exact Xcode/SDK/deployment contract and the actually loaded bundled libc++/libc++abi paths. Confirm candidate source-lock SHA equals the committed source lock. Before copying the lock, run the full x86-64 legacy contract from Step 1 against `gcc_image` and `clang_image`, including the exact `/opt/p2996-toolchain/lib/x86_64-unknown-linux-gnu` path, Debug/Release CTest, both exporters, Linux aggregate, and negative diagnostic check. A common probe alone is insufficient compatibility evidence.

- [ ] **Step 6: Commit the exact reviewed output lock separately**

Copy the byte-for-byte candidate to `.github/docker/toolchains.lock`, rerun validation, and commit only that file:

```bash
cmake -E copy build/toolchain-candidate/toolchains.lock .github/docker/toolchains.lock
cmp build/toolchain-candidate/toolchains.lock .github/docker/toolchains.lock
python3 .github/scripts/validate-toolchain-locks.py --sources .github/docker/toolchain-sources.lock --outputs .github/docker/toolchains.lock --recipe-root .
git add -- .github/docker/toolchains.lock
git commit -m "build: seal P2996 toolchains"
```

Do not manually edit a digest, URL, checksum, or generated identity.

### Task 6: Add the Authoritative Six-Node Workflow

**Files:**
- Create: `.github/workflows/relocatable-world-matrix.yml`
- Modify: `tools/relocatable_world_evidence.py`
- Modify: `test/test_relocatable_world_evidence.py`

**Interfaces:**
- Consumes: sealed output lock, six toolchains, producer/consumer/matrix programs.
- Produces: six producer bundles, six consumer results, `agreements.json`, `closure.json`, and a workflow summary.

- [ ] **Step 1: Add workflow-schema tests**

Add a Python test that parses the workflow text/YAML-safe structure and requires all six node IDs, all four runner labels, `fail-fast: false`, `if: always()` on consumers/Agreement/closure, artifact upload before final failure, a `workflow_dispatch` trigger, and a push trigger for `codex/cppcon2026-deck` (needed because this new workflow is not yet present on the default branch). Its path set covers `.gitattributes`, `CMakeLists.txt`, `cmake/**`, `include/**`, `example/relocatable_world_demo/**`, `tools/relocatable_world_evidence.py`, `tools/run-relocatable-world.sh`, both lock files, and the workflow itself so the final post-launcher commit necessarily receives fresh authoritative evidence. Require top-level `contents: read` only. Only the Linux producer and Linux consumer job definitions override with the exact pair `contents: read` plus `packages: read`; macOS, Agreement, and closure jobs receive no package permission, and no job has write or package-visibility permission. Also require exact `${{ github.sha }}`/`${{ github.run_id }}.${{ github.run_attempt }}`, explicit node `toolchain_artifact_sha256`, and committed-lock inputs on every seal/consumer preparation path; a fresh platform probe on all six consumers; the complete consumer compiler family/revision/version/target/stdlib/flags fields derived from that job's locked configuration; and `verify-p2996-toolchain.sh --require-locked-sdk` on both producer and both consumer macOS paths. Static tests require its emitted sysroot, deployment, bundled-libc++ include/link/rpath flags to configure those builds and require `otool` plus `DYLD_PRINT_LIBRARIES` verification of the final producer and consumer executables. No path may use `--fixture-context`, `:latest`, an XOffset path, submodule initialization, or a runner-image equality gate.

- [ ] **Step 2: Implement six producer jobs from the sealed lock**

Use two least-privilege producer matrix job definitions: four Linux entries with the explicit job-level map `{contents: read, packages: read}`, and two macOS entries inheriting top-level `contents: read` with no package permission. Together they cover each fixed node exactly once and reuse the exact full Action commits pinned in Task 5. Linux jobs authenticate to the private GHCR packages with the job token, read the node's exact per-platform manifest digest from the lock, verify that digest belongs to the locked two-entry index, and invoke `docker run` by that manifest digest. macOS jobs download and verify the exact archive, select the hard-locked Xcode version/build and SDK version/build, and call `verify-p2996-toolchain.sh --require-locked-sdk`. They configure with every emitted explicit sysroot, deployment, bundled-libc++ include/link/rpath flag, then prove with `otool` and an actual `DYLD_PRINT_LIBRARIES=1` run that the final producer executable loads the bundled libc++/libc++abi; `ImageOS`/`ImageVersion` are only passed to the probe as observed metadata. Each job initializes its output with `fallback-provenance`, passes `${{ github.sha }}`, `${{ github.run_id }}.${{ github.run_attempt }}`, the node's exact manifest/archive hash through `--toolchain-artifact-sha256`, and the two committed lock files to `seal-producer`, replaces the fallback atomically only after sealing succeeds, and always uploads a directory containing either READY/REJECT provenance and permitted artifacts or the INCOMPLETE fallback.

- [ ] **Step 3: Implement six always-running consumer jobs**

Use the same four-Linux/two-macOS least-privilege split for consumers: Linux uses the explicit job-level map `{contents: read, packages: read}`, and macOS inherits only `contents: read`. Both consumer matrix job definitions depend on both producer groups and use `if: ${{ always() }}`. Download all available bundles with missing artifacts tolerated and initialize each result with `fallback-results`. Every consumer independently runs the platform probe. Linux consumers authenticate and use their exact per-platform manifest digest. Both macOS consumers download/verify the archive, select the hard-locked Xcode/SDK versions and builds, run `verify-p2996-toolchain.sh --require-locked-sdk`, and configure with the emitted `DEVELOPER_DIR`, explicit sysroot, deployment target, and bundled-libc++ include/link/rpath flags just as producers do; the final consumer executable also passes `otool` and actual `DYLD_PRINT_LIBRARIES` path verification. Generate the fixed six-slot input header from that probe while requiring `${{ github.sha }}`, `${{ github.run_id }}.${{ github.run_attempt }}`, the node's exact manifest/archive hash through `--toolchain-artifact-sha256`, and both committed lock-file hashes; require its compiler family/revision/version/target/stdlib and derived canonical flags to match the locked consumer build; compile with that same configuration, run exactly five directed transfers, embed the verified consumer build facts, replace the fallback atomically on success, and upload `<consumer>.results.json` even when upstream evidence or the consumer build is incomplete.

- [ ] **Step 4: Implement Agreement and closure jobs**

The Agreement job depends on both producer groups with `if: always()`, initializes `agreements.json` with `fallback-agreements`, downloads all producer bundles, runs `prepare-agreements` with `${{ github.sha }}`, `${{ github.run_id }}.${{ github.run_attempt }}`, and both committed locks, compiles and runs `agreement_check.cpp` with the runner's system C++20 compiler, and replaces the fallback with 15 pair/60 named decisions plus the fixed producer-provenance digest map. It always uploads the Agreement artifact before reporting failure. The closure job depends on both producer groups, both consumer groups, and Agreement with `if: always()`, initializes `closure.json` with `fallback-closure`, downloads the Agreement artifact as a required input, and runs `prepare-matrix --agreements ...` with the same expected run context followed by the separately compiled `matrix_check.cpp`; missing, fallback, malformed, stale, mixed-attempt, or producer-inconsistent Agreement data therefore leaves closure `INCOMPLETE`. It enumerates the fixed identities rather than counting files, records the common run tuple and consumed Agreement-file digest as `agreements_sha256`, replaces the fallback with its final result, uploads it, writes the GitHub summary, and only then exits non-zero unless status is `PASS`.

Successful output is exactly equivalent to:

```text
WORKFLOW PASS: nodes=6; agreement_pairs=15; named_permits=60/60; directed_loads=30/30
```

- [ ] **Step 5: Validate workflow statically**

```bash
python3 -m unittest -v test/test_relocatable_world_evidence.py
if rg -n ":latest|xoffset|submodule" .github/workflows/relocatable-world-matrix.yml; then echo "forbidden mutable/XOffset reference" >&2; exit 1; else test "$?" -eq 1; fi
```

Expected: tests PASS and the explicit negative search assertion succeeds with no matches; an `rg` I/O error remains a failure rather than being mistaken for no matches.

- [ ] **Step 6: Commit and push the authoritative workflow**

```bash
git add -- .github/workflows/relocatable-world-matrix.yml tools/relocatable_world_evidence.py test/test_relocatable_world_evidence.py
git commit -m "ci: add relocatable world native matrix"
git push
```

- [ ] **Step 7: Run the authoritative workflow and retain evidence**

```bash
head_sha="$(git rev-parse HEAD)"
run_state_dir="$(mktemp -d)"
trap 'rm -rf "$run_state_dir"' EXIT
run_id=""
attempt=0
while test "$attempt" -lt 60; do
  attempt=$((attempt + 1))
  gh run list --workflow relocatable-world-matrix.yml --branch codex/cppcon2026-deck --commit "$head_sha" --event push --limit 100 --json databaseId,headSha,event --jq ".[] | select(.headSha == \"$head_sha\" and .event == \"push\") | .databaseId" | sort -n > "$run_state_dir/matches.ids"
  run_count="$(wc -l < "$run_state_dir/matches.ids")"
  if test "$run_count" -eq 1; then
    run_id="$(tr -d '[:space:]' < "$run_state_dir/matches.ids")"
    break
  fi
  if test "$run_count" -gt 1; then
    echo "ambiguous matrix runs for $head_sha" >&2
    exit 1
  fi
  sleep 5
done
test -n "$run_id"
gh run watch "$run_id" --exit-status
gh run download "$run_id" -D build/relocatable-world-authoritative
run_attempt="$(gh api "repos/{owner}/{repo}/actions/runs/$run_id" --jq .run_attempt)"
workflow_run="$run_id.$run_attempt"
cmake -E echo "$workflow_run" > build/relocatable-world-authoritative/workflow-run.txt
cmake -E echo "$head_sha" > build/relocatable-world-authoritative/source-sha.txt
```

The push in Step 6 supplies the first run because a newly added workflow cannot be manually dispatched until it exists on the default branch. The bounded loop tolerates indexing delay but aborts if the exact commit has multiple distinct push runs. Require `closure.json.status == "PASS"`, its run tuple to equal `head_sha`/`workflow_run` and the committed lock hashes, exact 6/15/60/30 identity counts, and all 30 transfer statuses `PASS`.

If a remote-only defect is found, diagnose it from the retained job artifact, repair the same focused files, rerun static tests, commit, and push. That push is the fresh run trigger; recompute `head_sha` and select only the push run for the repair commit. Do not attempt manual dispatch before the workflow reaches the default branch, and do not edit generated evidence to make closure pass.

### Task 7: Add the ARM64 Mac 5/6 Launcher

**Files:**
- Create: `tools/run-relocatable-world.sh`
- Modify: `tools/relocatable_world_evidence.py`
- Modify: `test/test_relocatable_world_evidence.py`
- Modify: `CLAUDE.md`

**Interfaces:**
- Consumes: sealed locks, native macOS ARM64 archive, and four locked Linux image platforms.
- Produces command: `tools/run-relocatable-world.sh [--source-sha SHA] [--run-id ID]`
- Produces: one local evidence directory with five producers, five consumer results, 10 pair decisions, 20 directed loads, and an explicit non-authoritative result.

- [ ] **Step 1: Add local-profile count tests**

The evidence tests must require exactly five local node IDs, 10 unique unordered pairs, 40 named decisions, 20 unique non-self directed edges, and final status `authoritative: false`. A passing local closure requires all 40 decisions to be `PERMIT` and all 20 transfers to be `PASS`. Missing `x86_64_macos_clang` is expected; any other missing node is an error. Add launcher preflight tests proving that the no-argument form derives the exact current `HEAD` and a non-empty unique local invocation ID, explicit matching values are preserved, a well-formed but different 40-hex commit is rejected before any download/build/output-directory creation, missing private-GHCR credentials fail without printing a token, and both locked x86-64 images must pass an ARM-Mac Docker-emulation smoke before evidence work begins.

- [ ] **Step 2: Implement preflight before downloads or builds**

The script checks ARM64 macOS, Docker Desktop availability, Xcode command-line tools, Python 3, CMake/Ninja, the lock validator, the macOS toolchain verifier, sealed checksums/digests, and LF/executable script state. It accepts either an authenticated `gh` session whose token has `read:packages`, deriving the user with `gh api user --jq .login`, or the explicit pair `TYPELAYOUT_GHCR_USER`/`TYPELAYOUT_GHCR_TOKEN` where the PAT has that scope. The token is never passed as an argument or printed: with xtrace disabled, pipe it to `docker login ghcr.io --username "$user" --password-stdin`, unset the temporary shell value, and prove a private digest pull succeeds; do not call a package-visibility API. Before any toolchain download, build, or evidence output-directory creation, it resolves `git rev-parse HEAD`; an omitted `--source-sha` becomes that exact lowercase 40-hex value, while an explicit value must equal it. An omitted `--run-id` becomes `local-<12-hex-head>-<UTC timestamp>-<pid>` and an explicit value must be non-empty and safe for filenames. It calls the verifier in `--allow-unlocked-sdk` mode, uses its explicit sysroot/deployment/link/rpath flags, and records actual Xcode version/build and SDK version/build plus `sdk_locked`; a personal hard-lock mismatch is allowed only because this closure is non-authoritative. It exits before producing evidence if any precondition fails.

- [ ] **Step 3: Execute the exact five-node profile**

Use a single mounted artifact directory and run:

```text
macOS ARM64 / Bloomberg Clang natively
Linux ARM64 / GCC in native-architecture Docker
Linux ARM64 / Bloomberg Clang in native-architecture Docker
Linux x86-64 / GCC through Docker emulation
Linux x86-64 / Bloomberg Clang through Docker emulation
```

Authenticate once, then verify every pulled image by its node's per-platform manifest digest and every downloaded archive by SHA256 before use. Before creating evidence output, run an explicit `linux/amd64` smoke under Docker Desktop emulation for both locked x86-64 images; each smoke verifies the reported architecture, compiler identity, and optimized platform probe so a missing/broken emulation path fails early. Run a fresh platform probe for every producer and consumer build; both uses of the personal macOS compiler call the verifier in `--allow-unlocked-sdk` mode and use the emitted explicit environment/flags, including bundled-libc++ link/rpath selection and actual-load verification. Pass the same effective source SHA and run ID (derived or explicitly supplied), the node's selected manifest/archive digest through `--toolchain-artifact-sha256`, and the real lock files to every seal/consumer-prepare/audit operation; `--fixture-context` is forbidden. Initialize each node/result/Agreement/closure output with the corresponding fixed-slot fallback before running a compiler, then replace it atomically on success. Do not initialize a submodule.

- [ ] **Step 4: Close the local graph without claiming 6/6**

The final line is exactly:

```text
LOCAL COVERAGE 5/6: 3 native-architecture + 2 Docker-emulated; Agreement 10/10; directed loads 20/20; authoritative closure unavailable
```

- [ ] **Step 5: Run shell and local-profile static tests**

```bash
bash -n tools/run-relocatable-world.sh
python3 -m unittest -v test/test_relocatable_world_evidence.py
```

Expected: shell syntax and all local-profile schema/count/binding tests PASS before evidence is produced.

- [ ] **Step 6: Update developer commands and commit the launcher before execution**

In `CLAUDE.md`, replace mutable authoritative examples with the launcher, document the private-GHCR `gh`/PAT `read:packages` prerequisite without embedding credentials, and clearly label legacy `:latest` pipelines non-authoritative. Commit:

```bash
git add -- tools/run-relocatable-world.sh tools/relocatable_world_evidence.py test/test_relocatable_world_evidence.py CLAUDE.md
git update-index --chmod=+x tools/run-relocatable-world.sh
git commit -m "chore: add relocatable world local launcher"
```

- [ ] **Step 7: Execute and retain evidence for the exact committed source**

Before running, the launcher itself requires its effective source SHA (derived or explicitly supplied) to equal `git rev-parse HEAD`, then requires `git diff HEAD --` to be empty for `.gitattributes`, `CMakeLists.txt`, the complete `cmake/` tree, the complete `include/boost/typelayout/` tree, `include/boost/typelayout.hpp`, `example/relocatable_world_demo/`, `tools/relocatable_world_evidence.py`, `tools/run-relocatable-world.sh`, `.github/docker/toolchain-sources.lock`, `.github/docker/toolchains.lock`, `.github/scripts/validate-toolchain-locks.py`, and `.github/scripts/verify-p2996-toolchain.sh`; it also requires every file under those exact implementation paths to be tracked. It allows the separately hash-protected, pre-existing user change to `docs/talk/cppcon2026-main-deck-content-and-script.md` because that file is outside the executable-source set. Then run:

```bash
source_sha="$(git rev-parse HEAD)"
local_run_id="local-$(git rev-parse --short=12 HEAD)-$(date -u +%Y%m%dT%H%M%SZ)-$$"
./tools/run-relocatable-world.sh --source-sha "$source_sha" --run-id "$local_run_id"
python3 tools/relocatable_world_evidence.py audit-run --directory build/relocatable-world-local --expect-source-sha "$source_sha" --expect-workflow-run "$local_run_id" --sources-lock .github/docker/toolchain-sources.lock --outputs-lock .github/docker/toolchains.lock --expect-nodes 5 --expect-pairs 10 --expect-named-permits 40 --expect-transfers 20
```

The launcher also writes exact `source-sha.txt` and `run-id.txt` values beside the retained closure. Require every retained provenance record to contain that exact `source_sha`, `local_run_id`, and both actual lock hashes. Expected: the exact local coverage line, retained local `agreements.json` with 40/40 named `PERMIT`, five result files with 20/20 `PASS`, a matching Agreement digest in closure, and non-authoritative local closure status `PASS`. If execution exposes a defect, discard that run, fix and retest, commit the repair, recompute both identities, and rerun; Task 8 must never consume evidence produced from uncommitted launcher, evidence-tool, demo, or configuration changes.

### Task 8: Verify Final Evidence and Record Chinese Appendix Notes

**Files:**
- Create: `docs/talk/cppcon2026-relocatable-world-demo-notes.zh-CN.md`
- Verify only: all files from both plans
- Do not modify: CppCon deck source or output files

**Interfaces:**
- Consumes: one passing ARM64 Mac local run from the final implementation commit and the provisional workflow validation from Task 6.
- Produces evidence: a fresh authoritative run for that same final implementation commit.
- Produces: a concise evidence-backed Chinese presentation note, not deck slides.

- [ ] **Step 1: Run complete repository verification under both compiler families**

On at least one locked Linux GCC node and one locked Bloomberg Clang node:

```bash
cmake -S . -B build-final -G Ninja -DTYPELAYOUT_BUILD_COMPAT_CI=OFF
cmake --build build-final --parallel
ctest --test-dir build-final --output-on-failure
./build-final/relocatable_world_matrix_check --self-test
python3 -m unittest -v test/test_relocatable_world_evidence.py
```

Expected: all repository tests, matrix self-tests, and evidence tests PASS. If any implementation or evidence-tool repair is required, commit it and rerun the Task 7 local workflow before continuing; the final local and authoritative evidence must identify the same implementation commit.

- [ ] **Step 2: Run the final authoritative workflow and audit both profiles**

```bash
implementation_sha="$(git rev-parse HEAD)"
local_sha="$(tr -d '[:space:]' < build/relocatable-world-local/source-sha.txt)"
local_run_id="$(tr -d '[:space:]' < build/relocatable-world-local/run-id.txt)"
test "$local_sha" = "$implementation_sha"
git push
run_state_dir="$(mktemp -d)"
trap 'rm -rf "$run_state_dir"' EXIT
run_id=""
attempt=0
while test "$attempt" -lt 60; do
  attempt=$((attempt + 1))
  gh run list --workflow relocatable-world-matrix.yml --branch codex/cppcon2026-deck --commit "$implementation_sha" --event push --limit 100 --json databaseId,headSha,event --jq ".[] | select(.headSha == \"$implementation_sha\" and .event == \"push\") | .databaseId" | sort -n > "$run_state_dir/matches.ids"
  run_count="$(wc -l < "$run_state_dir/matches.ids")"
  if test "$run_count" -eq 1; then
    run_id="$(tr -d '[:space:]' < "$run_state_dir/matches.ids")"
    break
  fi
  if test "$run_count" -gt 1; then
    echo "ambiguous final matrix runs for $implementation_sha" >&2
    exit 1
  fi
  sleep 5
done
test -n "$run_id"
gh run watch "$run_id" --exit-status
gh run download "$run_id" -D build/relocatable-world-authoritative-final
run_attempt="$(gh api "repos/{owner}/{repo}/actions/runs/$run_id" --jq .run_attempt)"
workflow_run="$run_id.$run_attempt"
cmake -E echo "$workflow_run" > build/relocatable-world-authoritative-final/workflow-run.txt
cmake -E echo "$implementation_sha" > build/relocatable-world-authoritative-final/source-sha.txt
python3 tools/relocatable_world_evidence.py audit-run --directory build/relocatable-world-authoritative-final --expect-source-sha "$implementation_sha" --expect-workflow-run "$workflow_run" --sources-lock .github/docker/toolchain-sources.lock --outputs-lock .github/docker/toolchains.lock --expect-nodes 6 --expect-pairs 15 --expect-named-permits 60 --expect-transfers 30
python3 tools/relocatable_world_evidence.py audit-run --directory build/relocatable-world-local --expect-source-sha "$implementation_sha" --expect-workflow-run "$local_run_id" --sources-lock .github/docker/toolchain-sources.lock --outputs-lock .github/docker/toolchains.lock --expect-nodes 5 --expect-pairs 10 --expect-named-permits 40 --expect-transfers 20
if rg -n ":latest|xoffset|submodule" .github/workflows/relocatable-world-matrix.yml tools/run-relocatable-world.sh; then echo "forbidden mutable/XOffset reference" >&2; exit 1; else test "$?" -eq 1; fi
```

Then run the protected-path check from PowerShell:

```powershell
$baseline = Get-Content -Raw build-relocatable-world-matrix-baseline/commit.txt
git diff --exit-code "$baseline..HEAD" -- .gitmodules vendor/XOffsetDatastructure docs/talk/cppcon2026-sched-listing.md docs/talk/cppcon2026-main-deck-content-and-script.md docs/superpowers/specs/2026-08-23-cppcon2026-typelayout-deck-design.md docs/superpowers/plans/2026-08-23-cppcon2026-typelayout-deck-implementation.md
if ($LASTEXITCODE -ne 0) { throw "protected tracked path changed in matrix commits" }
git diff --exit-code -- .gitmodules vendor/XOffsetDatastructure docs/talk/cppcon2026-sched-listing.md docs/superpowers/specs/2026-08-23-cppcon2026-typelayout-deck-design.md docs/superpowers/plans/2026-08-23-cppcon2026-typelayout-deck-implementation.md
if ($LASTEXITCODE -ne 0) { throw "protected tracked path has working-tree changes" }
$deckHash = git hash-object docs/talk/cppcon2026-main-deck-content-and-script.md
if ($LASTEXITCODE -ne 0) { throw "pre-existing main-deck file is missing" }
$expectedDeckHash = Get-Content -Raw build-relocatable-world-matrix-baseline/main-deck.sha
if ($deckHash -ne $expectedDeckHash) { throw "pre-existing main-deck bytes changed" }
```

Expected: both evidence audits PASS with the same `implementation_sha`, the authoritative GitHub run metadata identifies `workflow_run` as the successful `<run_id>.<run_attempt>` push attempt for that SHA, no forbidden matches exist, the committed protected-path diff is empty from baseline through `HEAD`, the non-deck protected working-tree diff is empty, and the separately captured main-deck content hash is unchanged.

- [ ] **Step 3: Write only evidence-supported Chinese notes**

The note contains these sections:

```text
1. Demo 的实际问题：同一应用和 schema 合同下，一个 connected game-world region 从 producer build 进入预验证的 consumer build；覆盖 server→server checkpoint/接管/恢复，以及 server→已声明 native client 的 snapshot delivery；明确写出“这是受 offset-based arena/checkpoint 设计（包括 XOffsetDatastructure）启发的独立教学实现，不是 XOffsetDatastructure，也不实现或验证其 wire format”
2. 最小数据模型：Entity、WorldSnapshot、relative_ptr、string/vector/flat_map
3. 构建期两道门：每个 build 的 compile-time Admission、verification build/CI 的 Agreement，以及四个逐 key Permit；运行时不重新计算这两道门
4. 运行时责任：读取 checkpoint 或接收 snapshot 后执行 40-byte envelope、range/lifetime/index/graph validation；network framing/authentication 不在 Demo 范围内
5. 正向结果：A→B→C、无 fixup、共享/环/null、查询与 mutation
6. 三个负例及其准确失败层
7. 六节点证据：6 nodes、15 pairs、60 named permits、30 directed PASS loads
8. 边界：appendix relocation demo；不替代 portable_capture；不证明 XOffset compatibility 或 schema evolution
9. 可用于后续 deck 的候选素材和 retained workflow run/artifact identities
```

Do not write a statement unless its exact run/artifact supports it. Link the retained run ID, source SHA, source/output lock hashes, and closure artifact in the note.

- [ ] **Step 4: Run final diff and script checks**

```bash
git diff --check
git status --short
bash -n tools/run-relocatable-world.sh .github/scripts/build-p2996-macos.sh .github/scripts/verify-p2996-toolchain.sh
python3 .github/scripts/validate-toolchain-locks.py --sources .github/docker/toolchain-sources.lock --outputs .github/docker/toolchains.lock --recipe-root .
```

Expected: no whitespace errors, all shell syntax and locks valid, and every new task diff belongs to the explicit file lists. The pre-existing user-owned change to `docs/talk/cppcon2026-main-deck-content-and-script.md` may remain in status and must not be staged as part of this work.

- [ ] **Step 5: Commit the evidence note**

```bash
git add -- docs/talk/cppcon2026-relocatable-world-demo-notes.zh-CN.md
git commit -m "docs: record relocatable world matrix evidence"
```

## Matrix Completion Gate

The work is complete only when the retained authoritative evidence proves:

```text
six unique READY native nodes
four Admission facts and four signatures per node
six signature artifacts and six region artifacts
every producer and consumer records its exact locked Linux per-platform manifest or macOS archive digest
all provenance/artifact hashes valid
all complete records in each profile share one source SHA, workflow-run identity, and committed lock-hash pair; GitHub identity is <run_id>.<run_attempt>
authoritative and local evidence identify the same final implementation source SHA
both authoritative macOS nodes use the hard-locked Xcode version/build, SDK version/build, deployment target, and archive SHA256 with explicit sysroots
both authoritative macOS nodes prove bundled libc++ headers, link target, rpath, and actual DYLD-loaded paths
ImageOS/ImageVersion are retained diagnostic metadata only and never gate a lock or closure
local macOS records its actual Xcode/SDK identity and truthful sdk_locked status without affecting authoritative claims
15 unique unordered Agreement records
60 named TypeLayout PERMIT decisions
closure agreements_sha256 matches the required Agreement artifact and its recomputed producer decisions
six unique consumer records
six verified consumer build-environment records, including locked macOS SDK identities
30 unique non-self directed transfer records
30 PASS transfers
closure status PASS
complete freshly configured CTest suites PASS under one locked GCC node and one locked Bloomberg Clang node
Python evidence/lock/workflow tests PASS
C++20 authoritative/local matrix self-tests PASS
local ARM64 Mac coverage 5/6 with 10/10 pairs, 40/40 named PERMIT, 20/20 PASS loads, local closure PASS, and authoritative=false
no mutable reference consumed by authoritative or local workflows
private GHCR remains private; CI uses least package permissions and the local launcher authenticates with read:packages via password-stdin
source lock binds exact Docker client/server, Buildx, digest-qualified BuildKit, and toolchain-images workflow LF bytes
each Linux index contains exactly the locked amd64 and arm64 manifests, with provenance/SBOM descriptors disabled and both per-platform digests sealed
GCC uses separately verified prerequisites and --disable-nls without download_prerequisites; GCC uses install-strip and P2996 builds clang only with cmake --install --strip
source builds cap compile parallelism at one job per 2 GiB available memory, LLVM_PARALLEL_LINK_JOBS=1, and each macOS archive is smaller than 2 GiB
release publication binds target_commitish to the candidate source SHA and forbids asset overwrite; existing OCI indexes are reused only on exact two-manifest identity and never overwritten otherwise
both x86-64 images pass the ARM Mac Docker-emulation smoke before the local evidence run
locked digest images pass the existing x86-64 CI/compat command contract before default-branch legacy alias promotion
default-branch promotion consumes sealed outputs without rebuilding candidate images or macOS archives
no XOffset build/runtime dependency
vendor reference unchanged
main deck unchanged
tracked protected paths unchanged from the recorded matrix baseline commit
pre-existing main-deck working bytes match the captured hash
```

If any required node, artifact, decision, or transfer is missing, the correct result is `INCOMPLETE`; do not weaken the declared matrix or substitute emulation/self-hosted evidence for a native node.
