import hashlib
import json
import pathlib
import subprocess
import sys
import tempfile

executable, configuration = sys.argv[1:]
result = subprocess.run([executable, "--configuration", configuration, "--run-id", "native-marshal-test"], text=True, capture_output=True, check=True)
snapshot = json.loads(result.stdout)
assert snapshot["build"]["configuration"] == configuration
assert snapshot["build"]["sourceDirty"] is None
observations = {o["id"]: o for o in snapshot["observations"]}
for root, expected in [("marshal-default", 12), ("marshal-byte", 8), ("marshal-array", 16)]:
    assert observations[root]["metrics"]["valueSizeBytes"]["value"] == expected
assert [f["offsetBits"]["value"] for f in observations["marshal-default"]["members"]] == [0, 32, 64]
assert [f["offsetBits"]["value"] for f in observations["marshal-byte"]["members"]] == [0, 16, 32]
assert [f["offsetBits"]["value"] for f in observations["marshal-array"]["members"]] == [0, 96]
assert observations["marshal-array/values"]["metrics"]["arrayStrideBytes"]["value"] == 4
for observation in observations.values():
    for member in observation["members"]:
        if "childObservationId" in member:
            child = observations[member["childObservationId"]]
            assert child["context"]["hostObservationId"] == observation["id"]
            assert child["context"]["hostMemberId"] == member["id"]
bad = subprocess.run([executable, "--configuration", "not-actual"], text=True, capture_output=True)
assert bad.returncode == 3 and not bad.stdout
with tempfile.TemporaryDirectory() as temporary:
    output = pathlib.Path(temporary) / "布局.json"
    subprocess.run([executable, "--output", str(output), "--run-id", "unicode-path"], check=True)
    assert json.loads(output.read_text(encoding="utf-8"))["snapshotId"] == "native-marshalling:unicode-path"
    original_hash = hashlib.sha256(output.read_bytes()).digest()
    duplicate = subprocess.run([executable, "--output", str(output), "--run-id", "must-not-replace"], text=True, capture_output=True)
    assert duplicate.returncode == 3 and not duplicate.stdout and "exclusive-output-create-failed" in duplicate.stderr
    assert hashlib.sha256(output.read_bytes()).digest() == original_hash
print("PASS native marshalling snapshot: 3 roots, sizes/offsets, array relations, configuration validation")
