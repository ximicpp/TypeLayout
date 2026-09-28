# Layout signature protocol v1

This contract extends the [observation protocol 0.1](PROTOCOL.md). Independent language adapters emit observations; the shared normalizer produces a structured layout signature. An independent implementation may emit signatures directly only if it implements the same normalization, completeness, validation and conformance rules. A hash without its structured content is not a valid signature.

The signature format is `layout-signature-v1`; its JSON envelope uses `schemaVersion: "0.1"`. This is separate from TypeLayout's existing C++ signature string. Versions, scope and policy are explicit. Unknown required properties or unsupported versions fail validation rather than being ignored. Signature files and selection manifests retain the 64 MiB bound and duplicate-key rejection; their tree representation permits 512 JSON nesting levels while the semantic inline/type/mapping depth limit remains 64. Generation rejects an oversized envelope before writing an output file.

## Single-sided selection manifest

```json
{
  "schemaVersion": "0.1",
  "scope": "value",
  "policy": "value-fields-v1",
  "cases": [{
    "id": "order",
    "observation": "orders.native",
    "fields": [
      {"id": "status", "member": "kind"},
      {"id": "identity", "member": "id"},
      {"id": "quantity", "member": "units"}
    ]
  }]
}
```

The [manifest schema](signature-manifest.schema.json) defines the syntax. Cases map logical IDs to snapshot-local observation IDs. Fields map logical IDs to physical member IDs; optional `children` repeat this structure. IDs are case-sensitive, nonempty strings. Duplicate case/field IDs and multiple logical bindings of the same physical member at one level are invalid.

Unmapped actual members remain in an identity namespace; explicit logical bindings use a separate explicit namespace. The structured key is `{kind:"identity"|"explicit",id:string}`. Thus an automatic field cannot overwrite an explicitly mapped field with the same text ID. Two exports must use corresponding logical binding schemes. Explicit `x -> x` and automatic identity `x` are not interchangeable bindings.

Absent explicitly selected fields are retained for later comparison. Both sides selecting an absent field under complete enumeration is a configuration error. Missing under incomplete enumeration is unknown. A signature cannot decide the double-absence rule before its counterpart is selected.

## Envelope and equality domain

The [signature schema](signature.schema.json) describes the full wire shape. The top level carries `signatureFormat`, `scope`, `policy`, source snapshot/build identity, cases, and generation `exitCode` (0 complete, 2 partial). Each case carries:

- `id`: logical case ID used for pairing, outside the content digest.
- `payload`: versioned normalized layout under the selected scope/policy, including endian.
- `prerequisites`: representation view, contexts, origins, calibration, coverage, status and mapping information used to establish whether comparison is permitted.
- `state`: `complete` or `partial`, derived from required information.
- `unknowns`: missing required information and its explanation. Validation recomputes the multiset of `(path,kind)` entries. Nonempty message text and entry order may vary between producers/locales; neither controls completeness nor enters the digest.
- `digest`: `{algorithm:"sha256",value:"64 lowercase hexadecimal digits"}`, present only for complete cases.

Source names, build labels, compiler/runtime versions, snapshot-local type IDs, display names, evidence text and declaration order do not enter the layout payload. They cannot create a layout difference by themselves. Endian is a representation fact and does participate. Root value/object scope, nested context, representation view and marshalling profile are compatibility checks, not facts that may be erased to obtain equal hashes.

The case digest hashes its canonical payload, not the whole signature file, mapping file, source snapshot or producer metadata. Partial cases have no equivalence digest. Unknown reasons and identical unknown markers never establish equality. Digest equality alone cannot bypass prerequisites, logical mapping validity or exact content comparison.

## Normalized facts and comparison

Required layout facts retain their state and known value; evidence and unknown reason text are excluded from content equality. Selection follows the same rules as the observation comparer:

- Values use value size; objects use calibrated runtime-reported object size. The extent start is relative to the declared origin.
- `value-alignment-v1` includes alignment; `value-fields-v1` does not require it. Array scope includes the root value's array stride.
- Member role, placement, bit width, range union and representation are retained. Inline children remain a tree, never an implicitly traversed reference graph.
- Ranges are sorted and their overlapping/adjacent intervals merged; empty ranges disappear. Runtime regions are grouped by role before union.
- Scalar representation retains width, category, signedness, encoding and applicable floating format. References retain kind and slot width; target types are not traversed. Enums retain their underlying representation.
- Observed array length and dimensions define the compared instance shape. An unknown declared fixed count does not invalidate a fully observed instance. This is not a claim about every possible array length.
- Opaque internal structure, required unknown facts and unobserved inline aggregates remain incomplete.

Array observation/member prerequisites also retain their normalized `declaredType` with fixed-count facts. They are needed when a pair lacks corresponding child observations and the comparer must report a known declaration difference alongside incomplete placement. With both instances observed, their instance shapes determine equality, so the original fixed-count declaration does not enter the observed-layout digest. Validation checks that the declaration agrees with the projected type and any known instance count. This conditional evidence cannot be erased merely because it is outside the digest.

`compare-signatures` pairs logical case IDs and requires a specified mode (`regression`, `representation`, or `marshaled-layout`). Scope/policy mismatches and invalid input fail; incompatible observation prerequisites retain the comparison engine's `not-comparable`/`incomplete` behavior. A known difference remains `different` even when other facts are unknown, with coverage reported separately. Comparison uses validated structured contents, not digest equality.

Validation establishes internal consistency, required information and agreement with the claimed digest. It does not prove a producer measured real memory correctly or attest to its source/build claims. Backend conformance still requires an independent measurement oracle for the declared capabilities.

## Canonical byte encoding

The digest is SHA-256 of the canonical `payload` bytes only. Encoding is deterministic JSON with these rules:

1. UTF-8 without BOM or whitespace. Object property names sort by ordinal UTF-16 code units, with no locale or Unicode normalization. Arrays preserve their normalized order.
2. Strings use double quotes. Quote and backslash use `\"` and `\\`; slash is plain. Code units U+0000–U+001F and U+007F–U+FFFF use exactly `\u` plus four lowercase hex digits. There are no short control escapes. Supplementary Unicode scalars encode as their two UTF-16 surrogate escapes; isolated surrogates are rejected. All resulting bytes are ASCII, also valid UTF-8.
3. Integers are exact signed Int64 in plain decimal, without leading plus/zeros or exponent notation. No floating point numbers occur in layout payloads. Booleans and null use lowercase JSON literals.
4. Normalization precedes encoding: keys/roles/members have prescribed structure and ordering, ranges are merged, and policy-excluded information has already been removed. This encoder alone does not validate or normalize an arbitrary observation.

Members sort by binding kind then ID, both ordinal UTF-16; runtime regions sort by role. Merged ranges sort by start and use a nonnegative Int64 length; a union whose length cannot fit Int64 is rejected with a protocol error. Signature case arrays are selected by logical ID during comparison, so their file order is immaterial.

This is an Int64-preserving encoding, not RFC 8785/JCS. In particular, it never converts a layout integer through an IEEE-754 double. [signature_codec.py](../scripts/signature_codec.py) is an independent Python implementation used to check the .NET exporter's payload bytes/digest; it is not a signature validator.

## Conformance

The executable `LayoutObserver.SignatureChecks` covers language-neutral fixtures, independent expected values and mutations. `CoreChecks` checks parity with the established comparison behavior. `scripts/check-signatures.py` exercises real collector snapshots through public CLI file boundaries; no compiler or runtime introspection is needed to compare exported signatures.

[Public vectors](fixtures/signatures/README.md) contain hand-specified scalar and Unicode record payloads, complete observations/manifests/signatures, and fixed canonical bytes/digests including Int64 boundaries. Run `python scripts/check-signature-vectors.py` from the tool directory after installing `scripts/requirements-validation.txt` to check them with the independent Python encoder and public schemas.

A new adapter must pass observation schema and semantic validation, its claimed backend measurement checks, and the applicable signature vectors. Equivalent key/member ordering, interval partition and source/evidence metadata must preserve canonical content. Int64 precision, Unicode identity, logical key namespaces, unknowns and comparison gates must remain intact. New representation semantics require a versioned extension; they cannot be hidden in ignored fields.
