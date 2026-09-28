"""Generate the public Draft 2020-12 contracts. No runtime dependency."""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1] / "contracts"
S = {"type": "string", "minLength": 1}
I = {"type": "integer", "minimum": -(2**63), "maximum": 2**63 - 1}
N = {**I, "minimum": 0}
P = {**N, "minimum": 1}


def arr(item):
    return {"type": "array", "items": item}


def enum(*values):
    return {"enum": list(values)}


def obj(properties, required=None):
    return {"type": "object", "properties": properties, "required": list(properties if required is None else required), "additionalProperties": False}


def ref(name):
    return {"$ref": "#/$defs/" + name}


def fact(value):
    return {"oneOf": [
        obj({"state": {"const": "known"}, "value": value, "evidence": ref("evidence")}),
        obj({"state": enum("unknown", "not-applicable"), "reason": S}),
    ]}


DEFS = {
    "evidence": obj({"kind": enum("compiler", "runtime", "derived", "fixture"), "method": S, "version": S, "inputs": arr(S)}),
    "integerFact": fact(I), "sizeFact": fact(N), "positiveFact": fact(P), "stringFact": fact(S),
    "rangeFact": fact(arr(obj({"startBit": I, "lengthBits": N}))),
}

repr_props = {k: ref("stringFact") for k in ("category", "signedness", "encoding", "floatingFormat")}
repr_props["category"] = fact(enum("integer", "float", "boolean", "character", "byte"))
repr_props["signedness"] = fact(enum("signed", "unsigned", "not-applicable"))
repr_props["widthBits"] = ref("positiveFact")
type_props = {
    "id": S, "displayName": S, "kind": enum("scalar", "enum", "record", "union", "array", "reference", "opaque"),
    "representation": obj(repr_props, ["widthBits"]),
    "enumUnderlyingTypeRef": S, "elementTypeRef": S, "fixedCount": ref("sizeFact"),
    "referenceKind": enum("native-pointer", "native-reference", "GC-reference", "function-pointer", "member-pointer"),
    "targetTypeRef": S, "opaqueTag": S,
}
type_schema = obj(type_props, ["id", "displayName", "kind"])
type_schema["allOf"] = []
for kind, required in [("scalar", ["representation"]), ("enum", ["enumUnderlyingTypeRef"]),
                       ("array", ["elementTypeRef", "fixedCount"]), ("reference", ["referenceKind", "representation"]), ("opaque", ["opaqueTag"])]:
    then = {"required": required}
    if kind == "scalar":
        then["properties"] = {"representation": {"required": list(repr_props)}}
    type_schema["allOf"].append({"if": {"properties": {"kind": {"const": kind}}}, "then": then})
DEFS["typeDescriptor"] = type_schema
member_props = {"id": S, "displayName": S, "declarationOrder": N, "role": enum("field", "base"), "typeRef": S,
                "offsetBits": ref("integerFact"), "bitWidth": ref("sizeFact"), "declaredTypeSizeBits": ref("sizeFact"),
                "occupiedRanges": ref("rangeFact"), "overlapGroup": S, "childObservationId": S}
DEFS["member"] = obj(member_props, [k for k in member_props if k not in ("overlapGroup", "childObservationId")])
metrics = obj({k: ref("sizeFact") for k in ("valueSizeBytes", "standaloneSizeBytes", "referenceSlotBytes", "arrayStrideBytes", "runtimeReportedObjectBytes", "alignmentBytes")}, [])
obs_props = {
    "id": S, "typeId": S, "displayName": S, "status": enum("ok", "unsupported"), "limitations": arr(S),
    "view": enum("native", "managed", "marshaled"),
    "context": obj({"kind": enum("complete-value", "embedded-value", "array-element", "heap-object", "boxed-value"), "hostObservationId": S, "hostMemberId": S, "elementIndex": N}, ["kind"]),
    "origin": obj({"kind": enum("value-start", "object-reference", "instance-data", "anchor-field"), "extentStartBit": I, "conversionEvidence": ref("evidence")}, ["kind", "extentStartBit"]),
    "metrics": metrics, "members": arr(ref("member")), "runtimeRegions": arr(obj({"role": S, "ranges": ref("rangeFact")})),
    "coverage": obj({k: enum("complete", "partial", "unknown", "not-applicable") for k in ("fieldEnumeration", "extent", "occupiedRanges", "hiddenRegions")}),
    "instanceShape": obj({"length": N, "dimensions": arr(N)}),
    "marshallingProfile": obj({"id": S, "mechanism": S, "configuration": {"type": "object"}}),
}
observation = obj(obs_props, [k for k in obs_props if k not in ("instanceShape", "marshallingProfile")])
observation["allOf"] = [{"if": {"properties": {"view": {"const": "marshaled"}}}, "then": {"required": ["marshallingProfile"]}}]
DEFS["observation"] = observation
identity = {"type": "object", "properties": {"name": S, "version": S}, "required": ["name", "version"]}
target = obj({"os": S, "architecture": S, "abi": S, "pointerBits": enum(32, 64), "bitsPerByte": {"const": 8}, "endian": enum("little", "big")})
build_props = {k: S for k in ("buildId", "runId", "configuration", "sourceRevision", "sourceDigest", "artifactDigest")}
build_props.update({"sourceDirty": {"type": ["boolean", "null"]}, "compiler": identity, "runtime": identity, "target": target, "flags": arr(S), "dependencies": {"type": "object"}, "requestedProfile": {"type": "object"}})
build_props.update({"languages": {**arr(S), "minItems": 1, "uniqueItems": True}, "collectorProvenance": {"type": "object"},
                    "captureProvenance": obj({"sourceBinding": enum("profile", "run-default"), "sourceDigestScope": S,
                                              "artifactVerifiedStable": {"type": "boolean"}, "sourceRelation": enum("built-in-run", "unverified")})})
snapshot = obj({
    "schemaVersion": {"const": "0.1"}, "snapshotId": S,
    "producer": obj({"id": S, "version": S, "capabilities": arr(S)}),
    "build": obj(build_props, [k for k in build_props if k not in ("requestedProfile", "languages", "collectorProvenance", "captureProvenance")]),
    "typeDescriptors": arr(ref("typeDescriptor")), "observations": {**arr(ref("observation")), "minItems": 1},
    "diagnostics": arr(obj({"code": S, "message": S, "observationId": S}, ["code", "message"])), "limitations": arr(S),
})

field_map = obj({"id": S, "left": S, "right": S, "children": arr(ref("fieldMap"))}, ["id", "left", "right"])
compare = obj({
    "schemaVersion": {"const": "0.1"}, "mode": enum("regression", "representation", "marshaled-layout"),
    "policy": enum("value-fields-v1", "value-alignment-v1"), "scope": enum("value", "array", "object"),
    "cases": {**arr(obj({"id": S, "left": S, "right": S, "fields": arr(ref("fieldMap"))}, ["id", "left", "right"])), "minItems": 1},
})
compare["allOf"] = [{"if": {"properties": {"mode": enum("representation", "marshaled-layout")}}, "then": {"properties": {"cases": {"items": {"required": ["fields"]}}}}}]
step_props = {"executable": S, "arguments": arr(S), "workingDirectory": S, "timeoutSeconds": {"type": "integer", "minimum": 1, "maximum": 3600}}
profile_props = {**step_props, "id": S, "configuration": S, "target": obj({"os": S, "architecture": S}), "outputArgument": S, "requiredCapabilities": arr(S), "artifact": S, "buildSteps": arr(obj(step_props)), "sourceRoot": S}
run = obj({"schemaVersion": {"const": "0.1"}, "sourceRoot": S,
           "profiles": {**arr(obj(profile_props, [k for k in profile_props if k not in ("buildSteps", "sourceRoot")])), "minItems": 1},
           "comparisons": {**arr(obj({"id": S, "left": S, "right": S, "compareManifest": S})), "minItems": 1}}, ["schemaVersion", "profiles", "comparisons"])
entry = obj({"path": S, "kind": S, "left": {}, "right": {}, "message": S})
comparison = obj({"schemaVersion": {"const": "0.1"}, "mode": enum("regression", "representation", "marshaled-layout"), "scope": enum("value", "array", "object"), "policy": enum("value-fields-v1", "value-alignment-v1"), "leftSnapshotId": S, "rightSnapshotId": S,
                  "exitCode": enum(0, 1, 2), "cases": {**arr(obj({"id": S, "left": S, "right": S,
                  "verdict": enum("same", "different", "incomplete", "not-comparable"), "coverage": enum("complete", "partial", "unknown"),
                  "differences": arr(entry), "unknowns": arr(entry), "diagnostics": arr(S)})), "minItems": 1}})
context_side = obj({"snapshotId": S, "build": snapshot["properties"]["build"]})
comparison["properties"]["context"] = obj({"left": context_side, "right": context_side,
    "changes": arr(obj({"path": S, "kind": enum("identity", "provenance", "environment"), "left": {}, "right": {}})), "interpretation": S})
# Additive 0.1 field: older diff files without context remain readable.
logical_field = obj({"id": S, "member": S, "children": arr(ref("logicalField"))}, ["id", "member"])
project = obj({"schemaVersion": {"const": "0.1"}, "projectId": S,
    "variants": {**arr(obj({"id": S, "snapshot": S, "mapping": S})), "minItems": 1},
    "mappings": {**arr(obj({"id": S, "cases": {**arr(obj({"id": S, "observation": S, "fields": arr(ref("logicalField"))})), "minItems": 1}})), "minItems": 1},
    "comparisons": {**arr(obj({"id": S, "left": S, "right": S, "mode": enum("regression", "representation", "marshaled-layout"),
        "scope": enum("value", "array", "object"), "policy": enum("value-fields-v1", "value-alignment-v1"), "cases": {**arr(S), "minItems": 1, "uniqueItems": True}})), "minItems": 1}})
project_variant_result = obj({"id": S, "status": enum("ok", "error"), "snapshotId": S, "snapshot": S, "error": S}, ["id", "status"])
project_pair_result = obj({"id": S, "left": S, "right": S, "status": enum("ok", "error"), "exitCode": enum(0, 1, 2), "diff": S, "html": S, "error": S}, ["id", "left", "right", "status"])
for result, ok_fields in [(project_variant_result, ["snapshotId", "snapshot"]), (project_pair_result, ["exitCode", "diff", "html"])]:
    result["allOf"] = [{"if": {"properties": {"status": {"const": "ok"}}}, "then": {"required": ok_fields}, "else": {"required": ["error"]}}]
project_result = obj({"schemaVersion": {"const": "0.1"}, "projectId": S, "exitCode": enum(0, 1, 2, 3),
    "variants": {**arr(project_variant_result), "minItems": 1}, "comparisons": {**arr(project_pair_result), "minItems": 1}})

signature_manifest = obj({"schemaVersion": {"const": "0.1"},
    "scope": enum("value", "array", "object"), "policy": enum("value-fields-v1", "value-alignment-v1"),
    "cases": {**arr(obj({"id": S, "observation": S, "fields": arr(ref("logicalField"))}, ["id", "observation"])), "minItems": 1}})

def normalized_fact(value):
    return {"oneOf": [obj({"state": {"const": "known"}, "value": value}), obj({"state": enum("unknown", "not-applicable")})]}


signature_defs = {
    "integerFact": normalized_fact(I), "sizeFact": normalized_fact(N),
    "positiveFact": normalized_fact(P), "stringFact": normalized_fact(S),
    "rangeFact": normalized_fact(arr(obj({"startBit": I, "lengthBits": N}))),
    "key": obj({"kind": enum("explicit", "identity"), "id": S}),
}
normalized_repr = {key: ref("stringFact") for key in ("category", "signedness", "encoding", "floatingFormat")}
normalized_repr["category"] = normalized_fact(enum("integer", "float", "boolean", "character", "byte"))
normalized_repr["signedness"] = normalized_fact(enum("signed", "unsigned", "not-applicable"))
normalized_repr["widthBits"] = ref("positiveFact")
normalized_type = obj({"kind": enum("scalar", "enum", "record", "union", "array", "reference", "opaque"),
    "representation": obj(normalized_repr, ["widthBits"]), "referenceKind": type_props["referenceKind"],
    "underlying": ref("type"), "element": ref("type"), "fixedCount": ref("sizeFact")}, ["kind"])
normalized_type["allOf"] = []
for kind, required in [("scalar", ["representation"]), ("reference", ["referenceKind", "representation"]), ("enum", ["underlying"]), ("array", ["element"])]:
    then = {"required": required}
    if kind == "scalar":
        then["properties"] = {"representation": {"required": list(normalized_repr)}}
    normalized_type["allOf"].append({"if": {"properties": {"kind": {"const": kind}}}, "then": then})
signature_defs["type"] = normalized_type
normalized_member = obj({"key": ref("key"), "presence": enum("present", "absent", "unknown"),
    "role": enum("field", "base"), "offsetBits": ref("integerFact"), "bitWidth": ref("sizeFact"),
    "occupiedRanges": ref("rangeFact"), "type": ref("type"), "value": ref("layout")}, ["key", "presence"])
present_fields = ["role", "offsetBits", "bitWidth", "occupiedRanges", "type"]
normalized_member["allOf"] = [{"if": {"properties": {"presence": {"const": "present"}}},
    "then": {"required": present_fields}, "else": {"not": {"anyOf": [{"required": [key]} for key in present_fields + ["value"]]}}}]
signature_defs["member"] = normalized_member
signature_defs["layout"] = obj({"type": ref("type"), "extentStartBit": I,
    "metrics": obj({key: ref("sizeFact") for key in ("valueSizeBytes", "runtimeReportedObjectBytes", "alignmentBytes", "arrayStrideBytes")}, []),
    "members": arr(ref("member")), "runtimeRegions": arr(obj({"role": S, "ranges": ref("rangeFact")})),
    "arrayShape": obj({"count": ref("sizeFact"), "dimensions": normalized_fact(arr(N))}),
    "instanceShape": obs_props["instanceShape"]}, ["type", "extentStartBit", "metrics", "members", "runtimeRegions"])
signature_defs["prerequisites"] = obj({"view": obs_props["view"], "contextKind": obs_props["context"]["properties"]["kind"],
    "originKind": obs_props["origin"]["properties"]["kind"], "elementIndex": N, "calibrated": {"type": "boolean"}, "status": obs_props["status"],
    "coverage": obs_props["coverage"], "explicitFields": {"type": "boolean"}, "unmappedFields": arr(S),
    "marshallingProfile": obs_props["marshallingProfile"], "declaredType": ref("type"),
    "members": arr(obj({"key": ref("key"), "childrenRequested": {"type": "boolean"}, "declaredType": ref("type"), "value": ref("prerequisites")}, ["key", "childrenRequested"]))},
    ["view", "contextKind", "originKind", "calibrated", "status", "coverage", "explicitFields", "unmappedFields", "members"])
signature_payload = obj({"signatureFormat": {"const": "layout-signature-v1"}, "scope": signature_manifest["properties"]["scope"],
    "policy": signature_manifest["properties"]["policy"], "endian": enum("little", "big"), "layout": ref("layout")})
signature_case = obj({"id": S, "state": enum("complete", "partial"), "payload": signature_payload,
    "prerequisites": ref("prerequisites"), "unknowns": arr(obj({"path": S, "kind": S, "message": S})),
    "digest": obj({"algorithm": {"const": "sha256"}, "value": {"type": "string", "pattern": "^[0-9a-f]{64}$"}})},
    ["id", "state", "payload", "prerequisites", "unknowns"])
signature_case["allOf"] = [{"if": {"properties": {"state": {"const": "complete"}}},
    "then": {"required": ["digest"], "properties": {"unknowns": {"maxItems": 0}}},
    "else": {"not": {"required": ["digest"]}, "properties": {"unknowns": {"minItems": 1}}}}]
signature = obj({"schemaVersion": {"const": "0.1"}, "signatureFormat": {"const": "layout-signature-v1"},
    "scope": signature_manifest["properties"]["scope"], "policy": signature_manifest["properties"]["policy"],
    "source": context_side, "cases": {**arr(signature_case), "minItems": 1}, "exitCode": enum(0, 2)})

contracts = [("snapshot", snapshot, DEFS), ("compare-manifest", compare, {"fieldMap": field_map}), ("run-manifest", run, {}), ("comparison", comparison, {}), ("project-manifest", project, {"logicalField": logical_field}), ("project-result", project_result, {}), ("signature-manifest", signature_manifest, {"logicalField": logical_field}), ("signature", signature, signature_defs)]
for name, schema, definitions in contracts:
    output = {"$schema": "https://json-schema.org/draft/2020-12/schema", "$id": f"https://ximicpp.github.io/TypeLayout/layout-observer/0.1/{name}.schema.json", "title": f"Layout Compare {name} 0.1", **schema}
    if definitions:
        output["$defs"] = definitions
    (ROOT / f"{name}.schema.json").write_text(json.dumps(output, indent=2) + "\n", encoding="utf-8")
print(f"Generated {len(contracts)} public JSON schemas")
