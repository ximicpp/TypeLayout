"""Check public signature schemas, canonical byte vectors, and fixture digests."""
import json
from pathlib import Path
from jsonschema import Draft202012Validator
from signature_codec import canonical_bytes, payload_digest

ROOT = Path(__file__).resolve().parents[1] / "contracts"
VECTORS = ROOT / "fixtures" / "signatures"


def read(path):
    return json.loads(path.read_text(encoding="utf-8"))


validators = {}
for path in ROOT.glob("*.schema.json"):
    schema = read(path)
    Draft202012Validator.check_schema(schema)
    validators[path.name.removesuffix(".schema.json")] = Draft202012Validator(schema)

count = 0
for suffix, kind in (("snapshot", "snapshot"), ("manifest", "signature-manifest"), ("signature", "signature")):
    paths = list(VECTORS.glob(f"*.{suffix}.json"))
    if not paths:
        raise AssertionError("Missing public " + suffix + " vectors")
    for path in paths:
        value = read(path)
        validators[kind].validate(value)
        if kind == "signature":
            for case in value["cases"]:
                if case["state"] == "complete":
                    assert case["digest"]["value"] == payload_digest(case["payload"]), path
                else:
                    assert "digest" not in case, path
        count += 1
        print("PASS schema/vector", path.name)

codec_paths = list(VECTORS.glob("*.codec.json"))
if not codec_paths:
    raise AssertionError("Missing canonical byte vectors")
for path in codec_paths:
    value = read(path)
    actual = canonical_bytes(value["input"])
    assert actual == value["canonical"].encode("ascii"), path
    assert payload_digest(value["input"]) == value["sha256"], path
    count += 1
    print("PASS canonical bytes", path.name)

print(f"PASS: {len(validators)} schemas, {count} public signature vectors")
