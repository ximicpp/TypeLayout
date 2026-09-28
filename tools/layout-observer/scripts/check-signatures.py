"""Exercise independent signature export and wire comparison with real collector snapshots."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import subprocess
from signature_codec import payload_digest

TOOL = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--dotnet", default="dotnet")
parser.add_argument("--cli", required=True, type=Path)
for name in ("native-debug", "native-release", "managed-debug", "managed-release"):
    parser.add_argument("--" + name, required=True, type=Path)
parser.add_argument("--object-snapshot", type=Path, help="Optional CoreCLR fixture capture containing plain-array-2 and nested-array-2")
parser.add_argument("--marshal-native", type=Path)
parser.add_argument("--marshal-managed", type=Path)
parser.add_argument("--output", required=True, type=Path)
args = parser.parse_args()
work = args.output.resolve()
work.mkdir(parents=True, exist_ok=False)
cli = [args.dotnet, str(args.cli.resolve())]
count = 0


def call(name, command, expected):
    global count
    result = subprocess.run(cli + command, capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=60)
    if result.returncode != expected:
        raise AssertionError(f"{name}: expected {expected}, got {result.returncode}\n{result.stdout}\n{result.stderr}")
    count += 1
    print(f"PASS {name}: exit {expected}")
    return result


def write(name, value):
    path = work / name
    path.write_text(json.dumps(value, indent=2, ensure_ascii=False), encoding="utf-8")
    return path


def read(path):
    return json.loads(Path(path).read_text(encoding="utf-8-sig"))


def signature_manifest(pair, side):
    def fields(maps):
        return [dict(id=m["id"], member=m[side], **({"children": fields(m["children"])} if "children" in m else {})) for m in maps]
    return dict(schemaVersion="0.1", scope=pair["scope"], policy=pair["policy"], cases=[
        dict(id=c["id"], observation=c[side], **({"fields": fields(c["fields"])} if "fields" in c else {})) for c in pair["cases"]])


def generate(name, snapshot, manifest, expected=0):
    manifest_path = write(name + ".manifest.json", manifest)
    output = work / (name + ".signature.json")
    call("export " + name, ["signature", "--input", str(snapshot), "--manifest", str(manifest_path), "--out", str(output)], expected)
    call("validate " + name, ["validate-signature", "--input", str(output)], 0)
    for case in read(output)["cases"]:
        if case["state"] == "complete":
            assert case["digest"] == dict(algorithm="sha256", value=payload_digest(case["payload"])), "Independent Python digest disagrees"
        else:
            assert "digest" not in case, "Partial layout has an equivalence digest"
    return output


def compare(name, left, right, mode, expected):
    output = work / (name + ".diff.json")
    call(name, ["compare-signatures", "--left", str(left), "--right", str(right), "--mode", mode, "--out", str(output)], expected)
    result = read(output)
    assert result["exitCode"] == expected
    return result


def verdicts(diff):
    return {c["id"]: (c["verdict"], c["coverage"]) for c in diff["cases"]}


shared = read(TOOL / "cases/shared.compare.json")
left_manifest = signature_manifest(shared, "left")
right_manifest = signature_manifest(shared, "right")
signatures = {}
for variant in ("native_debug", "native_release", "managed_debug", "managed_release"):
    signatures[variant] = generate(variant, getattr(args, variant).resolve(), left_manifest if variant.startswith("native") else right_manifest)
for name, left, right, mode in (
    ("cross-debug", "native_debug", "managed_debug", "representation"),
    ("cross-release", "native_release", "managed_release", "representation"),
    ("native-builds", "native_debug", "native_release", "regression"),
    ("managed-builds", "managed_debug", "managed_release", "regression"),
):
    actual = compare(name, signatures[left], signatures[right], mode, 0)
    pair = copy.deepcopy(shared)
    pair["mode"] = mode
    manifest = write(name + ".pair.json", pair)
    direct = work / (name + ".direct.diff.json")
    call(name + " direct", ["compare", "--left", str(getattr(args, left).resolve()), "--right", str(getattr(args, right).resolve()), "--manifest", str(manifest), "--out", str(direct)], 0)
    assert verdicts(actual) == verdicts(read(direct))

packing = read(TOOL / "cases/packing.compare.json")
packing_left = generate("packing-left", args.native_release.resolve(), signature_manifest(packing, "left"))
packing_right = generate("packing-right", args.native_release.resolve(), signature_manifest(packing, "right"))
compare("packing-change", packing_left, packing_right, packing["mode"], 1)

strict = copy.deepcopy(right_manifest)
strict["policy"] = "value-alignment-v1"
strict_signature = generate("strict-managed", args.managed_release.resolve(), strict, 2)
compare("unknown-does-not-equal-itself", strict_signature, strict_signature, "regression", 2)
compare("view-gate", signatures["native_release"], signatures["managed_release"], "regression", 2)

# The outer "array" fixture is a wrapper record; its standalone array stride is
# not measured. Select its actual observed inline array value (stride 12), not
# the wrapper, so this checks the collector's declared array-placement fact.
array_pair = dict(schemaVersion="0.1", mode="representation", scope="array", policy="value-fields-v1", cases=[
    dict(id="int32-array-value", left="array/values", right="array/values", fields=[
        dict(id=str(index), left=str(index), right=str(index)) for index in range(3)])])
array_left = generate("array-native", args.native_release.resolve(), signature_manifest(array_pair, "left"))
array_right = generate("array-managed", args.managed_release.resolve(), signature_manifest(array_pair, "right"))
compare("array-stride-scope", array_left, array_right, "representation", 0)

if args.object_snapshot:
    object_manifest = dict(schemaVersion="0.1", scope="object", policy="value-fields-v1", cases=[
        dict(id=name, observation=name + "-2") for name in ("plain-array", "nested-array")])
    objects = generate("coreclr-objects", args.object_snapshot.resolve(), object_manifest)
    compare("calibrated-struct-array-objects", objects, objects, "regression", 0)
if bool(args.marshal_native) != bool(args.marshal_managed):
    parser.error("--marshal-native and --marshal-managed must be supplied together")
if args.marshal_native:
    marshaled = read(TOOL / "cases/marshaled.compare.json")
    mn = generate("marshal-native", args.marshal_native.resolve(), signature_manifest(marshaled, "left"))
    mm = generate("marshal-managed", args.marshal_managed.resolve(), signature_manifest(marshaled, "right"))
    compare("recorded-runtime-marshalling", mn, mm, "marshaled-layout", 0)

# A completed output is immutable, even when its content is valid for regeneration.
existing = signatures["native_release"]
before = existing.read_bytes()
call("signature overwrite rejected", ["signature", "--input", str(args.native_release.resolve()), "--manifest", str(work / "native_release.manifest.json"), "--out", str(existing)], 3)
assert existing.read_bytes() == before

# Independently allocated files, reordered case selectors and build labels cannot
# make the result depend on a particular pairing or execution order.
reordered = copy.deepcopy(left_manifest)
reordered["cases"].reverse()
for case in reordered["cases"]:
    if "fields" in case:
        case["fields"].reverse()
reordered_signature = generate("reordered", args.native_release.resolve(), reordered)
compare("selector-order-independent", signatures["native_release"], reordered_signature, "regression", 0)

unicode_manifest = copy.deepcopy(left_manifest)
for case in unicode_manifest["cases"]:
    for field in case.get("fields", []):
        field["id"] = "\U00010000\ue000中文/.[]\\\"\n\t" + field["id"]
unicode_signature = generate("unicode", args.native_release.resolve(), unicode_manifest)
compare("unicode-wire-roundtrip", unicode_signature, unicode_signature, "regression", 0)

duplicate = work / "duplicate.signature.json"
duplicate.write_text('{"schemaVersion":"0.1","schemaVersion":"0.1"}', encoding="utf-8")
call("duplicate signature key rejected", ["validate-signature", "--input", str(duplicate)], 3)
invalid = read(existing)
invalid["schemaVersion"] = "unsupported-version"
call("unknown signature version rejected", ["validate-signature", "--input", str(write("invalid-version.signature.json", invalid))], 3)
tampered = read(existing)
tampered["cases"][0]["digest"]["value"] = "0" * 64
call("tampered digest rejected", ["validate-signature", "--input", str(write("invalid-digest.signature.json", tampered))], 3)

write("verification.json", dict(checks=count, result="pass", sourceSnapshotDigests={name: hashlib.sha256(getattr(args, name).read_bytes()).hexdigest() for name in signatures}))
print(f"PASS: {count} signature CLI checks")
