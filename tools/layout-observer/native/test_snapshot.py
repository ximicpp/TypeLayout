"""Validate emitted observations against an independently built native oracle."""
import hashlib
import json
import pathlib
import subprocess
import sys
import tempfile

collector, oracle, configuration = sys.argv[1:]


def run(*args):
    return subprocess.run(args, text=True, capture_output=True, check=True)


snapshot = json.loads(run(collector, "--configuration", configuration, "--run-id", 'test-"\\\n-id').stdout)
expected = json.loads(run(oracle).stdout)
assert snapshot["schemaVersion"] == "0.1"
assert snapshot["build"]["runId"] == 'test-"\\\n-id'
assert snapshot["build"]["configuration"] == configuration
observations = {o["id"]: o for o in snapshot["observations"]}
types = {t["id"]: t for t in snapshot["typeDescriptors"]}
assert len(observations) == len(snapshot["observations"])
assert len(types) == len(snapshot["typeDescriptors"])


def metric(key, name):
    fact = observations[key]["metrics"][name]
    assert fact["state"] == "known"
    return fact["value"]


def field(key, name):
    return next(f for f in observations[key]["members"] if f["id"] == name)


assert metric("sample", "valueSizeBytes") == expected["sampleSize"]
assert metric("sample", "alignmentBytes") == expected["sampleAlign"]
assert field("sample", "count")["offsetBits"]["value"] == expected["sampleCountOffset"] * 8
assert metric("packed", "valueSizeBytes") == expected["packedSize"]
assert field("packed", "code")["offsetBits"]["value"] == expected["packedCodeOffset"] * 8
assert field("nested", "extra")["offsetBits"]["value"] == expected["nestedExtraOffset"] * 8
assert field("nested", "payload")["childObservationId"] == "nested/payload"
assert metric("nested/payload", "valueSizeBytes") == expected["sampleSize"]
assert field("empty-member", "empty")["bitWidth"]["value"] == 8
assert field("empty-member", "value")["offsetBits"]["value"] == expected["emptyValueOffset"] * 8
assert field("overlap", "empty")["occupiedRanges"]["state"] == "unknown"
assert field("derived", "base-0")["occupiedRanges"]["state"] == "unknown"
assert observations["polymorphic"]["coverage"]["hiddenRegions"] == "unknown"
assert observations["opaque"]["coverage"]["fieldEnumeration"] == "unknown"
assert observations["virtual"]["status"] == "unsupported"
assert observations["nontrivial"]["status"] == "ok"
assert field("bitfields", "first")["bitWidth"]["value"] == 3
assert field("bitfields", "second")["offsetBits"]["value"] == 3
assert field("bitfields", "second")["bitWidth"]["value"] == 5
assert field("union", "integer")["offsetBits"]["value"] == field("union", "real")["offsetBits"]["value"] == 0
assert metric("array/values", "arrayStrideBytes") == 12
assert metric("array/values/0", "arrayStrideBytes") == 4
assert [f["offsetBits"]["value"] for f in observations["array/values"]["members"]] == [0, 32, 64]
assert types[field("array", "values")["typeRef"]]["fixedCount"]["value"] == 3
assert types[field("enum", "value")["typeRef"]]["kind"] == "enum"
assert types[field("char16", "value")["typeRef"]]["representation"]["encoding"]["value"] == "utf16-code-unit"
assert types[field("char16", "value")["typeRef"]]["representation"]["signedness"]["value"] == "not-applicable"

for observation in observations.values():
    assert observation["typeId"] in types
    for member in observation["members"]:
        assert member["typeRef"] in types
        if "childObservationId" in member:
            child = observations[member["childObservationId"]]
            assert child["context"]["hostObservationId"] == observation["id"]
            assert child["context"]["hostMemberId"] == member["id"]
    for fact in observation["metrics"].values():
        if fact["state"] == "known":
            assert "value" in fact and fact["evidence"]["version"]
        else:
            assert "value" not in fact and fact["reason"]

with tempfile.TemporaryDirectory() as directory:
    output = pathlib.Path(directory) / "snapshot.json"
    result = run(collector, "--output", str(output), "--run-id", "file-test")
    assert not result.stdout
    assert json.loads(output.read_text())["snapshotId"] == "native:file-test"
    original_hash = hashlib.sha256(output.read_bytes()).digest()
    duplicate = subprocess.run([collector, "--output", str(output), "--run-id", "must-not-replace"], text=True, capture_output=True)
    assert duplicate.returncode == 3 and not duplicate.stdout and "exclusive-output-create-failed" in duplicate.stderr
    assert hashlib.sha256(output.read_bytes()).digest() == original_hash

opposite = "Release" if configuration == "Debug" else "Debug"
bad = subprocess.run([collector, "--configuration", opposite], text=True, capture_output=True)
assert bad.returncode == 3 and not bad.stdout and "does not match" in bad.stderr
bad = subprocess.run([collector, "--missing"], text=True, capture_output=True)
assert bad.returncode == 3 and not bad.stdout
print(f"PASS native observer: {configuration}, {len(observations)} observations, independent native oracle and failure diagnostics")
