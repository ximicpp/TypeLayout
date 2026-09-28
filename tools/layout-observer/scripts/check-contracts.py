"""Validate schema definitions and supplied real snapshots/manifests/diffs."""
import argparse
import json
from pathlib import Path
from jsonschema import Draft202012Validator

parser = argparse.ArgumentParser()
parser.add_argument("kind", choices=["snapshot", "compare-manifest", "run-manifest", "comparison", "project-manifest", "project-result", "signature-manifest", "signature"])
parser.add_argument("files", nargs="+")
args = parser.parse_args()
contracts = Path(__file__).resolve().parents[1] / "contracts"
schema = json.loads((contracts / f"{args.kind}.schema.json").read_text(encoding="utf-8"))
Draft202012Validator.check_schema(schema)
validator = Draft202012Validator(schema)
for file in args.files:
    instance = json.loads(Path(file).read_text(encoding="utf-8-sig"))
    errors = sorted(validator.iter_errors(instance), key=lambda error: str(error.path))
    if errors:
        for error in errors[:20]:
            print(f"FAIL {file}: {list(error.path)}: {error.message}")
        raise SystemExit(1)
    print(f"PASS {args.kind}: {file}")
