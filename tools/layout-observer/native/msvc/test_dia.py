"""Exercise real PE/PDB/DIA observations and fail closed on wrong identities."""
import hashlib
import json
import pathlib
import subprocess
import sys
import tempfile

collector, fixture, symbols, wrong_symbols, configuration = sys.argv[1:]


def run(*args):
    return subprocess.run(args, check=True, text=True, encoding="utf-8", capture_output=True)


oracle = json.loads(run(fixture).stdout)
args = [collector, "--pe", fixture, "--pdb", symbols, "--configuration", configuration, "--run-id", "dia-test"]
snapshot = json.loads(run(*args).stdout)
observations = {o["id"]: o for o in snapshot["observations"]}
types = {t["id"]: t for t in snapshot["typeDescriptors"]}
assert snapshot["build"]["target"] == {"os": "windows", "architecture": "x64" if oracle["pointerBits"] == 64 else "x86", "abi": "msvc", "pointerBits": oracle["pointerBits"], "bitsPerByte": 8, "endian": "little"}
assert snapshot["build"]["configuration"] == oracle["configuration"] == configuration
assert snapshot["build"]["sourceDirty"] is None
assert snapshot["build"]["compiler"]["version"] == oracle["compiler"]
assert snapshot["build"]["dependencies"]["pdbGuid"]


def fact(o, key):
    item = observations[o]["metrics"][key]
    assert item["state"] == "known"
    return item["value"]


def member(o, key):
    return next(m for m in observations[o]["members"] if m["id"] == key)


for o, key in [("sample", "sampleSize"), ("packed", "packedSize"), ("array", "arraySize"), ("enum", "enumSize"),
               ("bitfields", "bitsSize"), ("private", "privateSize"), ("derived", "derivedSize")]:
    assert fact(o, "valueSizeBytes") == oracle[key], (o, fact(o, "valueSizeBytes"), oracle[key])
for o, field, key in [("sample", "count", "sampleCountOffset"), ("packed", "code", "packedCodeOffset"),
                      ("nested", "extra", "nestedExtraOffset"), ("private", "count", "privateCountOffset"),
                      ("derived", "base-0", "derivedBaseOffset"), ("derived", "extra", "derivedExtraOffset"),
                      ("bitfields", "value", "bitsValueOffset")]:
    assert member(o, field)["offsetBits"]["value"] == oracle[key] * 8
assert oracle["sampleAlign"] == 4
assert observations["sample"]["metrics"]["alignmentBytes"]["state"] == "unknown"
assert member("bitfields", "first")["bitWidth"]["value"] == 3
assert member("bitfields", "second")["bitWidth"]["value"] == 5
assert member("bitfields", "second")["offsetBits"]["value"] == 3
assert member("union", "integer")["offsetBits"]["value"] == member("union", "real")["offsetBits"]["value"] == 0
assert observations["private"]["coverage"]["fieldEnumeration"] == "complete"
assert observations["sample"]["coverage"]["hiddenRegions"] == "not-applicable"
assert observations["sample"]["runtimeRegions"] == []
assert observations["virtual"]["status"] == "unsupported"
assert observations["polymorphic"]["coverage"]["hiddenRegions"] == "unknown"
assert member("msvc-overlap", "empty")["occupiedRanges"]["state"] == "unknown"
assert observations["msvc-overlap/empty"]["metrics"]["valueSizeBytes"]["state"] == "unknown"
assert fact("array/values", "arrayStrideBytes") == 4
assert [m["offsetBits"]["value"] for m in observations["array/values"]["members"]] == [0, 32, 64]
assert len(observations) == len(snapshot["observations"])
assert len(types) == len(snapshot["typeDescriptors"])
for observation in observations.values():
    assert observation["typeId"] in types
    for field in observation["members"]:
        assert field["typeRef"] in types
        if "childObservationId" in field:
            child = observations[field["childObservationId"]]
            assert child["context"]["hostObservationId"] == observation["id"]
            assert child["context"]["hostMemberId"] == field["id"]


def rejects(extra, expected):
    result = subprocess.run([collector, *extra], text=True, encoding="utf-8", capture_output=True)
    assert result.returncode == 3 and not result.stdout and expected in result.stderr, result


with tempfile.TemporaryDirectory() as directory:
    directory = pathlib.Path(directory)
    output = directory / "snapshot.json"
    run(*args, "--output", str(output))
    assert json.loads(output.read_text())["snapshotId"] == "msvc-dia:dia-test"
    original_hash = hashlib.sha256(output.read_bytes()).digest()
    duplicate = subprocess.run([*args, "--run-id", "must-not-replace", "--output", str(output)], text=True, encoding="utf-8", capture_output=True)
    assert duplicate.returncode == 3 and not duplicate.stdout and "exclusive-output-create-failed" in duplicate.stderr
    assert hashlib.sha256(output.read_bytes()).digest() == original_hash
    rejects(["--pe", fixture, "--pdb", str(directory / "missing.pdb")], "pdb-identity-validation-failed")
    rejects(["--pe", fixture, "--pdb", wrong_symbols], "pdb-identity-validation-failed")
    broken = directory / "broken.exe"
    broken.write_bytes(b"MZ")
    rejects(["--pe", str(broken), "--pdb", symbols], "malformed-pe")
    rejects(["--pe", fixture, "--pdb", symbols, "--type", "missing=MissingType"], "missing")
    opposite = "Release" if configuration == "Debug" else "Debug"
    rejects(["--pe", fixture, "--pdb", symbols, "--configuration", opposite], "configuration-mismatch")
print(f"PASS DIA {configuration}: {len(observations)} actual observations; private/base/bitfield oracle + wrong/missing PDB, malformed PE and configuration checks")
