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
snapshot = obj({
    "schemaVersion": {"const": "0.1"}, "snapshotId": S,
    "producer": obj({"id": S, "version": S, "capabilities": arr(S)}),
    "build": obj(build_props, [k for k in build_props if k != "requestedProfile"]),
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
profile_props = {**step_props, "id": S, "configuration": S, "target": obj({"os": S, "architecture": S}), "outputArgument": S, "requiredCapabilities": arr(S), "artifact": S, "buildSteps": arr(obj(step_props))}
run = obj({"schemaVersion": {"const": "0.1"}, "sourceRoot": S,
           "profiles": {**arr(obj(profile_props, [k for k in profile_props if k != "buildSteps"])), "minItems": 1},
           "comparisons": {**arr(obj({"id": S, "left": S, "right": S, "compareManifest": S})), "minItems": 1}}, ["schemaVersion", "profiles", "comparisons"])
entry = obj({"path": S, "kind": S, "left": {}, "right": {}, "message": S})
comparison = obj({"schemaVersion": {"const": "0.1"}, "mode": enum("regression", "representation", "marshaled-layout"), "scope": enum("value", "array", "object"), "policy": enum("value-fields-v1", "value-alignment-v1"), "leftSnapshotId": S, "rightSnapshotId": S,
                  "exitCode": enum(0, 1, 2), "cases": {**arr(obj({"id": S, "left": S, "right": S,
                  "verdict": enum("same", "different", "incomplete", "not-comparable"), "coverage": enum("complete", "partial", "unknown"),
                  "differences": arr(entry), "unknowns": arr(entry), "diagnostics": arr(S)})), "minItems": 1}})

for name, schema, definitions in [("snapshot", snapshot, DEFS), ("compare-manifest", compare, {"fieldMap": field_map}), ("run-manifest", run, {}), ("comparison", comparison, {})]:
    output = {"$schema": "https://json-schema.org/draft/2020-12/schema", "$id": f"https://ximicpp.github.io/TypeLayout/layout-observer/0.1/{name}.schema.json", "title": f"Layout Observer {name} 0.1", **schema}
    if definitions:
        output["$defs"] = definitions
    (ROOT / f"{name}.schema.json").write_text(json.dumps(output, indent=2) + "\n", encoding="utf-8")
print("Generated 4 public JSON schemas")
