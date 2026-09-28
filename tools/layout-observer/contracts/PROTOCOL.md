# Layout Compare wire protocol 0.1

Implementation contract for independent collectors and comparison. JSON property names are case-sensitive. Snapshot schemaVersion is `0.1`. Objects below use camelCase. No absolute addresses or instance values are required. All collectors emit the same shape; JSON Schema and the Core semantic validator enforce it. Strict snapshot/pair/project/run input parsing rejects duplicate keys, files over 64 MiB and nesting beyond 96 levels; export serializers allow 512 levels to accommodate signature trees and comparison/context wrappers without truncating accepted metadata. Signature input and its selection manifest have a separate 512-level bound; observation inline graphs and signature mappings remain limited to 64 levels.

Language extension uses this contract, not a language switch in the comparison engine. Independently exported, mapped layout signatures and their canonical encoding are specified in [SIGNATURE.md](SIGNATURE.md). Snapshot 0.1 remains the collector input; signature versioning is separate, and the original C++ TypeLayout signature string is not a wire-compatible substitute.

## Facts

Numeric and representation facts use this shape (JSON numbers are exact signed 64-bit integers for sizes/offsets):

```json
{"state":"known","value":12,"evidence":{"kind":"runtime","method":"Unsafe.SizeOf","version":"10.0","inputs":[]}}
```

Alternatively `{"state":"unknown","reason":"not-reported"}` or `{"state":"not-applicable","reason":"value-not-reference"}`. No value/evidence is allowed for unknown/not-applicable. Evidence kind is `compiler`, `runtime`, `derived`, or `fixture`. A range fact's value is an array of `{"startBit":0,"lengthBits":8}`; negative starts are permitted for runtime headers. Evidence inputs lists fact paths used for derivation. Unknown is never zero.

## Snapshot

Required top-level properties:

- `schemaVersion`, `snapshotId`: nonempty strings.
- `producer`: `{id,version,capabilities:[]}` strings.
- `build`: `{buildId,runId,configuration,sourceRevision,sourceDirty,sourceDigest,artifactDigest,compiler,runtime,target,flags:[],dependencies:{}}`. compiler/runtime/target are descriptive objects, not strings. target has `os`, `architecture`, `abi`, `pointerBits`, `bitsPerByte` (=8), `endian` (`little`/`big`). compiler/runtime each have `name`, `version`; not applicable uses `{name:"none",version:"none"}`. source/artifact digest may initially be `unknown` with a diagnostic in `limitations`; orchestration enriches them before accepting a reproducible baseline. Never forge a digest.
- `typeDescriptors`: array of type descriptors.
- `observations`: array of observations (including embedded children).
- `diagnostics`: array of `{code,message,observationId?}`.
- `limitations`: array of strings.

SourceDirty is boolean when observed, or null when unknown; unknown must not be labeled a clean checkout. BuildId/runId distinguish files; do not enter content equality. Producer version and compiler/runtime identities always describe the actual execution. Arrays may be empty where appropriate, but root IDs and requested cases must not silently disappear.

Additive optional build metadata: `languages` is a nonempty array of distinct strings (e.g. `cpp`, `csharp`); absent means unknown, never inferred from native/managed view. `collectorProvenance` retains the complete original collector build object. `captureProvenance` is `{sourceBinding:"profile"|"run-default",sourceDigestScope:string,artifactVerifiedStable:boolean,sourceRelation:"built-in-run"|"unverified"}`. It describes orchestration observations, not layout facts or cryptographic proof of a reproducible build. Old 0.1 snapshots without these fields remain accepted.

Known artifact digests emitted by current collectors use `sha256:` followed by 64 lowercase hexadecimal digits. Run also accepts older bare 64-digit SHA-256 and case variants for comparison, preserves the original claim, and rejects malformed or conflicting known digests. `unknown` remains explicitly unavailable.

## Types

A type descriptor has `id` (unique), `kind`, and `displayName`. Kind is `scalar`, `enum`, `record`, `union`, `array`, `reference`, or `opaque`.

- scalar: `representation` object of Fact properties: `widthBits`, `category` (`integer`/`float`/`boolean`/`character`/`byte`), `signedness` (`signed`/`unsigned`/`not-applicable`), `encoding` (`binary-integer`/`ieee754`/`utf16-code-unit`/`bool-0-or-1` etc), `floatingFormat` (`binary32`/`binary64` for floats; Fact not-applicable otherwise).
- enum: `enumUnderlyingTypeRef`.
- record/union: shape is provided by observations; no invented scalar representation.
- array: `elementTypeRef`, `fixedCount` Fact (unknown for runtime-length type).
- reference: `referenceKind` (`native-pointer`/`native-reference`/`GC-reference`/`function-pointer`/`member-pointer`), `targetTypeRef` (optional when unknowable), `representation.widthBits` Fact. A referenced class descriptor is a record; the slot descriptor is separate.
- opaque: `opaqueTag` (string); internals are unknown.

Type refs point into typeDescriptors. A missing target type should be represented by an opaque descriptor, not a dangling ID. Known widths come from the backend, never from a type name. Fixture primitive IDs can be `u8`, `i16`, `i32`, `u16`, `i64`, `f32`, `f64`, `bool`, `char16`; IDs are only local identities, not comparison equivalence.

## Observations

An observation requires:

- `id`, `typeId`, `displayName` strings; `status`: `ok`/`unsupported`; `limitations`: string array.
- `view`: `native`/`managed`/`marshaled`.
- `context`: `{kind,hostObservationId?,hostMemberId?,elementIndex?}`. Kind is `complete-value`, `embedded-value`, `array-element`, `heap-object`, `boxed-value`.
- `origin`: `{kind,extentStartBit,conversionEvidence?}`. Kind is `value-start`, `object-reference`, `instance-data`, `anchor-field`; extentStartBit is an integer, not a Fact.
- `metrics`: Fact properties; applicable keys are `valueSizeBytes`, `standaloneSizeBytes`, `referenceSlotBytes`, `arrayStrideBytes`, `runtimeReportedObjectBytes`, `alignmentBytes`. Complete-value/embedded/array-element use valueSizeBytes, object contexts use runtimeReportedObjectBytes; unknown metrics stay unknown.
- `members`: array below; `runtimeRegions`: array of `{role,ranges:Fact}`.
- `coverage`: `{fieldEnumeration,extent,occupiedRanges,hiddenRegions}`. Values are `complete`/`partial`/`unknown`/`not-applicable`.
- optional `instanceShape`: `{length,dimensions:[]}` (integer length and dimensions).
- optional `marshallingProfile`: `{id,mechanism,configuration:{}}`; required for marshaled view. `mechanism` initially `runtime-marshalling`.

A member requires `id`, `displayName`, `declarationOrder` (integer), `role` (`field`/`base`), `typeRef`, Fact `offsetBits`, Fact `bitWidth`, Fact `declaredTypeSizeBits`, Fact `occupiedRanges`. Optional `overlapGroup` and `childObservationId` are strings. BitWidth is field representation width; for a bit-field it is the actual bit width, not storage type size. For complex/reused subobjects width/ranges may be unknown. Fields refer to nonstatic storage only.

Child observation relations are bidirectional: host member.childObservationId points to child; child.context.hostObservationId/hostMemberId point back. No inline cycles; reference target cycles are allowed, but inline array/enum type cycles are invalid. Inline nesting is bounded to 64 levels. Enum underlying descriptors must be integral scalars (integer/byte/boolean/character; C++ allows bool and character bases); unknown underlying representation remains incomplete. Embedded child offsets/ranges are relative to its own origin; the parent's member gives placement. Complete known plain scalar member range is `offsetBits` + `bitWidth`. A nested parent range can include that nested value's padding; reporting its children must not double count it.

Only embedded-value/array-element observations can be inline children; heap objects and boxed values cannot be followed through this relation. Array child indices must be unique, in bounds and agree with the parent's cardinality; declared fixed count, observed length and dimension product must agree when known. Runtime-length arrays with an observed instanceShape compare that instance and its elements without claiming identical fixed-count type declarations. Instance shape belongs to arrays and object contexts, not ordinary scalar/value records.

`arrayStrideBytes` means the stride of the **observed value T in T[]**. For an inline `int[3]` observed as T, this is 12; its observed int elements have stride 4. A managed heap array has no inline object stride: the parent metric is not-applicable and element children retain their measured stride. Internal element placement is represented by each member offset. `scope=array` requires the root value's array stride; other scopes do not silently add that requirement. Earlier native producers 0.1 and runtime-marshalling producer 0.1.0 incorrectly used internal element stride on array parents; recapture those baselines with producer 0.1.1 before using array scope.

`conversionEvidence`, if present, is a complete nonnull evidence object with kind/method/version/inputs, as used by Facts. Object-scope comparison requires this calibration. Runtime regions participate in every scope, including native complete values. Required aggregate field enumeration and occupied-range coverage cannot be bypassed with `not-applicable`. Unknown alignment is omitted from `value-fields-v1` equality but required by `value-alignment-v1`. All required unknown facts remain inconclusive even when identical on both sides.

Unsupported requested types remain as observations with status unsupported, unknown metrics/coverage, an opaque type descriptor if necessary, and a stable reason in limitations/diagnostics. They never count as a successful comparison.

Common shared fixture roots for initial collectors: `sample` (u8 tag, i32 count, i16 code, Pack4), `packed` (same, Pack1), `reordered` (i32 count,i16 code,u8 tag), `nested` (sample payload,i32 extra), `array` (i32[3] values), `enum` (u16 enum value). Use lowercase member IDs in these fixtures; display names may preserve source spelling. Additional native/managed-specific cases can coexist. Do not fake a physical inline C# array with an ordinary array reference.

## Compare manifest

```json
{
  "schemaVersion":"0.1",
  "mode":"representation",
  "policy":"value-fields-v1",
  "scope":"value",
  "cases":[{"id":"sample","left":"sample","right":"sample","fields":[{"id":"tag","left":"tag","right":"tag"},{"id":"count","left":"count","right":"count"},{"id":"code","left":"code","right":"code"}]}]
}
```

Modes: regression/representation/marshaled-layout. Policies: value-fields-v1/value-alignment-v1. Scopes: value/array/object. Regression requires matching views; representation accepts any native/managed pair, including native/native and managed/managed. A view does not identify a language. Marshaled layout remains a separate native/marshaled comparison requiring its marshalling profile. Fields are optional only for regression (matching member IDs); representation requires explicit maps, including a `children` field map array for nested record members when IDs differ. Every actual member must be accounted for by a map (absence with complete enumeration is addition/removal, not error). A selector absent on both complete sides is invalid. IDs map storage declarations, not values. Case references are snapshot-local observation IDs. Unknown representation cannot be made equal by mapping. For nested mapped members without children, recursively match identical member IDs; nonmatching nested IDs require explicit children. Nonempty children on a leaf without child observations are invalid; an unobserved aggregate remains incomplete.

Different origin kinds remain incomparable unless an adapter first normalizes them. Once the coordinate origin is shared and object calibration is present, differing `extentStartBit` is an observed difference, not a reason to stop field comparison.

## Comparison output

```json
{
  "schemaVersion":"0.1",
  "mode":"representation",
  "scope":"value",
  "policy":"value-fields-v1",
  "leftSnapshotId":"native-run",
  "rightSnapshotId":"managed-run",
  "exitCode":0,
  "cases":[{
    "id":"sample","left":"sample","right":"sample",
    "verdict":"same","coverage":"complete",
    "differences":[],"unknowns":[],"diagnostics":[]
  }]
}
```

Difference/unknown entries are `{path,kind,left,right,message}`. left/right may be arbitrary JSON or null. Kinds include size/offset/representation/overlap/added/removed/alignment/stride/context/coverage. Diagnostic entries are strings. Verdict is same/different/incomplete/not-comparable. Coverage is complete/partial/unknown. Known required differences dominate incomplete verdict, while coverage still records unknown required facts. Batch exit 2 for incomplete/not-comparable or partial coverage, 1 for fully observed differences, 0 for all same; invalid input/build/capture errors return 3. Empty requested case list is invalid. The CLI prints errors to stderr and never silently rewrites a baseline.

New comparisons also emit `context:{left:{snapshotId,build},right:{snapshotId,build},changes:[{path,kind,left,right}],interpretation}`. Build leaf changes are classified as identity, provenance or environment; arrays are compared as whole metadata values. Path components escape `~` and `/` as in JSON Pointer. Metadata differences do not affect layout verdict or establish causation. Old diff files without context remain structurally valid.

## Comparison project

`project-manifest.schema.json` describes reusable inputs to comparison only (no build execution). A project contains `schemaVersion`, `projectId`, `variants`, `mappings`, `comparisons`:

- variant: `{id,snapshot,mapping}`; snapshot paths are relative to the project manifest.
- mapping: `{id,cases:[{id,observation,fields:[{id,member,children?}]}]}`; children repeat the logical field structure. Logical field IDs match across implementations, physical member IDs may differ. Builds can share a mapping.
- comparison: `{id,left,right,mode,scope,policy,cases:[logicalCaseId]}`. Pairs and nonempty case subsets are explicit; no implicit all-pairs execution.

Both sides must define the selected logical case and the same logical field IDs. To compare an added/removed field, retain its expected physical member ID on both sides; observed absence is evaluated by coverage. An omitted logical mapping is a configuration error. Variant/comparison IDs must be portable filename components, unique ignoring case, and cannot be Windows reserved device names. Other IDs remain case-sensitive strings.

`project` imports all required snapshots, then expands each pair into the existing compare manifest. Missing/malformed inputs and pair errors remain in `result.json` (schema `project-result.schema.json`); unaffected comparisons still run, overall exit 3. Successfully imported snapshots, pair manifests, JSON diff and offline HTML are saved under a new output directory. A `project.json` referencing only bundle-local snapshots is emitted only after every snapshot has been copied successfully; replay needs neither the original paths nor compilers. `project.source.json` preserves the original declaration, `index.html` links the reports. No output is overwritten. The importer validates protocol facts but does not manufacture build provenance.

## Orchestration manifest

A separate JSON run manifest has schemaVersion, optional sourceRoot, profiles[], comparisons[]. Each profile has id, executable, arguments[], workingDirectory, configuration, target{os,architecture}, artifact (file to hash), outputArgument (e.g. --output), requiredCapabilities[], timeoutSeconds. It can have buildSteps[] with executable, arguments[], workingDirectory, timeoutSeconds. Configuration/path IDs must be unique and path-safe. All path properties are relative to the manifest directory unless absolute; strings inside arguments are passed unchanged and interpreted by the program relative to its workingDirectory. A fresh run directory receives one output per profile; the launcher appends outputArgument plus the unique output path. It uses argument arrays (no shell expansion), captures stdout/stderr separately, verifies actual target/configuration and capabilities, validates every snapshot, and rejects missing/duplicate profiles. comparisons specify id, left/right profile IDs and a compareManifest relative path. Build/collector errors are errors, not expected differences.

Each profile may select its own sourceRoot (relative to the manifest); otherwise it inherits the top-level sourceRoot, whose default is the manifest directory. Identity covers the Git root's HEAD, index entries/stages, tracked and nonignored worktree files, and recursively initialized submodules. Missing submodules fail rather than masquerading as deleted files. Generated build/output directories must be outside that tree or ignored. All source roots remain stable across the run, and the declared artifact digest is checked before and after capture. Git commands resolve each actual sourceRoot independently; inherited GIT_DIR/GIT_WORK_TREE redirects cannot override them. WSL must use a Git-readable checkout.

Original collector build claims are retained under collectorProvenance; the enriched digest describes the checkout observed during the run. A prebuilt artifact without recorded buildSteps has sourceRelation=unverified. Built-in-run only records execution of the declared build steps, not proof that arbitrary commands compiled those sources. External SDKs, headers and dependencies require separate version/content pinning. Source registration/capture errors are retained per profile and comparison in run.json; independent valid profiles continue. Any final source stability failure prevents successful comparisons from being accepted.

The public Draft 2020-12 schemas validate JSON structure. The Core validator additionally checks unique IDs, references, range overflow/bounds, required coverage, enum constraints and inline graphs. Both are required by the validation scripts; JSON syntax or schema validation alone is insufficient to prove a valid observation.
