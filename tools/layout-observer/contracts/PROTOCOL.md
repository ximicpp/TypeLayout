# Layout Observer wire protocol 0.1

Implementation contract for the independent observer. JSON property names are case-sensitive. Snapshot schemaVersion is `0.1`. Objects below use camelCase. No absolute addresses or instance values are required. All collectors emit the same shape; JSON Schema and the Core semantic validator enforce it.

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

Modes: regression/representation/marshaled-layout. Policies: value-fields-v1/value-alignment-v1. Scopes: value/array/object. Fields are optional only for regression (matching member IDs); representation requires explicit maps, including a `children` field map array for nested record members when IDs differ. Every actual member must be accounted for by a map (absence with complete enumeration is addition/removal, not error). IDs map storage declarations, not values. Case references are snapshot-local observation IDs. Unknown representation cannot be made equal by mapping. For nested mapped members without children, recursively match identical member IDs; nonmatching nested IDs require explicit children.

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

## Orchestration manifest

A separate JSON run manifest has schemaVersion, optional sourceRoot, profiles[], comparisons[]. Each profile has id, executable, arguments[], workingDirectory, configuration, target{os,architecture}, artifact (file to hash), outputArgument (e.g. --output), requiredCapabilities[], timeoutSeconds. It can have buildSteps[] with executable, arguments[], workingDirectory, timeoutSeconds. Configuration/path IDs must be unique and path-safe. All path properties are relative to the manifest directory unless absolute; strings inside arguments are passed unchanged and interpreted by the program relative to its workingDirectory. A fresh run directory receives one output per profile; the launcher appends outputArgument plus the unique output path. It uses argument arrays (no shell expansion), captures stdout/stderr separately, verifies actual target/configuration and capabilities, validates every snapshot, and rejects missing/duplicate profiles. comparisons specify id, left/right profile IDs and a compareManifest relative path. Build/collector errors are errors, not expected differences.

sourceRoot selects a Git checkout (default: manifest directory); identity covers the entire repository's tracked and nonignored files. Generated build/output directories must be outside that tree or ignored. Inputs must remain stable across the run or provenance validation fails. Original collector source claims are retained under compiler.collectorSourceRevision; the enriched digest describes the checkout observed during the run. A prebuilt artifact without recorded buildSteps does not establish a source-to-binary relationship merely because its hash was calculated. Never promote an unbuilt current checkout digest into proof of what produced an old binary.

The public Draft 2020-12 schemas validate JSON structure. The Core validator additionally checks unique IDs, references, range overflow/bounds, required coverage, enum constraints and inline graphs. Both are required by the validation scripts; JSON syntax or schema validation alone is insufficient to prove a valid observation.
