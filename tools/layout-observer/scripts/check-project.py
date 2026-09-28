"""Exercise snapshot import, reusable mappings, replay and required-input failures through the real CLI."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile

TOOL = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--dotnet", default="dotnet")
parser.add_argument("--cli", required=True, type=Path)
for name in ("native-debug", "native-release", "managed-debug", "managed-release"):
    parser.add_argument("--" + name, required=True, type=Path)
args = parser.parse_args()
cli = [args.dotnet, str(args.cli.resolve())]
count = 0

def write(path, value):
    path.write_text(json.dumps(value, indent=2), encoding="utf-8")
    return path

with tempfile.TemporaryDirectory(prefix="layout-project-") as temporary:
    work = Path(temporary)
    shared = json.loads((TOOL / "cases/shared.compare.json").read_text(encoding="utf-8"))

    def fields(items, side):
        return [dict(id=f["id"], member=f[side], **({"children": fields(f["children"], side)} if "children" in f else {})) for f in items]

    project = dict(schemaVersion="0.1", projectId="independent build inputs", variants=[], mappings=[], comparisons=[])
    for language, side in (("native", "left"), ("managed", "right")):
        project["mappings"].append(dict(id=language, cases=[dict(id=c["id"], observation=c[side], fields=fields(c["fields"], side)) for c in shared["cases"]]))
        for configuration in ("debug", "release"):
            variant = f"{language}-{configuration}"
            path = getattr(args, variant.replace("-", "_"))
            project["variants"].append(dict(id=variant, snapshot=str(path.resolve()), mapping=language))
        project["comparisons"].append(dict(id=f"{language}-builds", left=f"{language}-debug", right=f"{language}-release", mode="regression", scope="value", policy="value-fields-v1", cases=["sample", "nested", "array"]))
    for configuration in ("debug", "release"):
        project["comparisons"].append(dict(id=f"cross-{configuration}", left=f"native-{configuration}", right=f"managed-{configuration}", mode="representation", scope="value", policy="value-fields-v1", cases=[c["id"] for c in shared["cases"]]))

    def run(name, value, expected):
        global count
        manifest = write(work / (name + ".json"), value)
        output = work / name
        result = subprocess.run(cli + ["project", "--manifest", str(manifest), "--out-dir", str(output)], capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=60)
        assert result.returncode == expected, f"{name}: expected {expected}, got {result.returncode}\n{result.stdout}\n{result.stderr}"
        count += 1
        print(f"PASS {name}: exit {expected}")
        return output

    output = run("four-builds", project, 0)
    summary = json.loads((output / "result.json").read_text())
    assert len(summary["comparisons"]) == 4 and len(summary["variants"]) == 4
    native_diff = json.loads((output / "comparisons/native-builds.diff.json").read_text())
    assert len(native_diff["cases"]) == 3
    assert any(c["path"] == "build/configuration" and c["kind"] == "environment" for c in native_diff["context"]["changes"])
    assert native_diff["exitCode"] == 0
    replay = work / "replay"
    result = subprocess.run(cli + ["project", "--manifest", str(output / "project.json"), "--out-dir", str(replay)], capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stderr
    for pair in summary["comparisons"]:
        assert (output / pair["diff"]).read_bytes() == (replay / pair["diff"]).read_bytes()
    count += 1
    print("PASS portable replay preserves every diff byte")
    before = hashlib.sha256((output / "result.json").read_bytes()).digest()
    run("four-builds", project, 3)
    assert hashlib.sha256((output / "result.json").read_bytes()).digest() == before

    missing = copy.deepcopy(project); missing["variants"][0]["snapshot"] = "missing.json"
    failed = run("missing-required-input", missing, 3)
    failed_summary = json.loads((failed / "result.json").read_text())
    assert len(failed_summary["variants"]) == 4 and len(failed_summary["comparisons"]) == 4
    assert sum(p["status"] == "error" for p in failed_summary["comparisons"]) == 2
    assert not (failed / "project.json").exists()

    broken = copy.deepcopy(project); broken["mappings"][1]["cases"][0]["fields"].pop()
    run("missing-logical-field", broken, 3)
    for name, change in (
        ("empty-comparisons", lambda p: p.update(comparisons=[])),
        ("empty-cases", lambda p: p["comparisons"][0].update(cases=[])),
        ("missing-mapping", lambda p: p["variants"][0].update(mapping="absent")),
        ("unknown-variant", lambda p: p["comparisons"][0].update(left="absent")),
        ("missing-logical-case", lambda p: p["comparisons"][0].update(cases=["absent"])),
        ("duplicate-selected-case", lambda p: p["comparisons"][0].update(cases=["sample", "sample"])),
        ("duplicate-physical-field", lambda p: p["mappings"][0]["cases"][0]["fields"][1].update(member="tag")),
        ("path-traversal", lambda p: p["variants"][0].update(id="../escape")),
        ("device-name", lambda p: p["variants"][0].update(id="CON")),
        ("case-collision", lambda p: p["variants"].append({**p["variants"][0], "id": "NATIVE-DEBUG"})),
        ("unknown-property", lambda p: p.update(implicitAllPairs=True)),
    ):
        altered = copy.deepcopy(project); change(altered); run(name, altered, 3)

    strict = copy.deepcopy(project); strict["comparisons"][2]["policy"] = "value-alignment-v1"
    run("unknown-required-alignment", strict, 2)
    packing = copy.deepcopy(project)
    packing["mappings"].append(copy.deepcopy(packing["mappings"][0]))
    packing["mappings"][-1]["id"] = "packed"
    packing["mappings"][-1]["cases"][0]["observation"] = "packed"
    packing["variants"].append(dict(id="native-packed", snapshot=str(args.native_release.resolve()), mapping="packed"))
    packing["comparisons"].append(dict(id="packing-change", left="native-release", right="native-packed", mode="representation", scope="value", policy="value-fields-v1", cases=["sample"]))
    run("same-view-representation-known-difference", packing, 1)

    # Accepted metadata nesting must survive snapshot copy, context serialization,
    # HTML rendering and a portable replay (the default serializer depth is only 64).
    deep = json.loads(args.native_release.read_text(encoding="utf-8"))
    nested = {"leaf": "input remains descriptive metadata"}
    for _ in range(70):
        nested = {"next": nested}
    deep["build"]["dependencies"] = nested
    deep_path = write(work / "deep-snapshot.json", deep)
    deep_project = copy.deepcopy(project)
    for variant in deep_project["variants"]:
        if variant["id"].startswith("native"):
            variant["snapshot"] = str(deep_path)
    deep_output = run("deep-metadata", deep_project, 0)
    result = subprocess.run(cli + ["project", "--manifest", str(deep_output / "project.json"), "--out-dir", str(work / "deep-replay")], capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stderr
    count += 1
    print("PASS deep metadata replay")

print(f"PASS {count} comparison project checks")
