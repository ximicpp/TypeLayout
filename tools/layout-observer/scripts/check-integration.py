"""Real CLI regression and orchestrator failure-path tests (Python stdlib)."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

TOOL = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--dotnet", default="dotnet")
parser.add_argument("--cli", required=True, type=Path)
parser.add_argument("--native-debug", required=True, type=Path)
parser.add_argument("--native-release", required=True, type=Path)
parser.add_argument("--managed-debug", required=True, type=Path)
parser.add_argument("--managed-release", required=True, type=Path)
args = parser.parse_args()
cli = [args.dotnet, str(args.cli.resolve())]
count = 0
test_environment = os.environ.copy()
# A caller may use these to bridge a Windows worktree into WSL. They must not
# redirect our isolated test repository's git init/add/commit to the user's repo.
for key in ("GIT_DIR", "GIT_WORK_TREE", "GIT_INDEX_FILE", "GIT_COMMON_DIR"):
    test_environment.pop(key, None)

def call(name, command, expected, cwd=None):
    global count
    result = subprocess.run(command, cwd=cwd, env=test_environment, capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=60)
    if result.returncode != expected:
        raise AssertionError(f"{name}: expected {expected}, got {result.returncode}\n{result.stdout}\n{result.stderr}")
    count += 1
    print(f"PASS {name}: exit {expected}")
    return result

def write(path, value):
    path.write_text(json.dumps(value, indent=2), encoding="utf-8")
    return str(path)

with tempfile.TemporaryDirectory(prefix="layout-integration-") as temporary:
    work = Path(temporary)
    shared = TOOL / "cases/shared.compare.json"
    regression = TOOL / "cases/regression.compare.json"
    def compare(name, left, right, manifest, expected, extra=()):
        call(name, cli + ["compare", "--left", str(left.resolve()), "--right", str(right.resolve()), "--manifest", str(manifest)] + list(extra), expected)
    for config in ("debug", "release"):
        compare(f"cross {config}", getattr(args, f"native_{config}"), getattr(args, f"managed_{config}"), shared, 0)
    compare("native Debug/Release", args.native_debug, args.native_release, regression, 0)
    compare("managed Debug/Release", args.managed_debug, args.managed_release, regression, 0)
    compare("packing changes", args.native_release, args.native_release, TOOL / "cases/packing.compare.json", 1)
    strict = json.loads(shared.read_text(encoding="utf-8")); strict["policy"] = "value-alignment-v1"
    compare("strict missing alignment", args.native_release, args.managed_release, write(work / "strict.json", strict), 2)
    output = work / "baseline.json"
    compare("write explicit baseline", args.native_release, args.managed_release, shared, 0, ["--out", str(output)])
    before = hashlib.sha256(output.read_bytes()).digest()
    compare("baseline overwrite rejected", args.native_release, args.managed_release, shared, 3, ["--out", str(output)])
    assert hashlib.sha256(output.read_bytes()).digest() == before
    invalid = work / "duplicate.json"; invalid.write_text('{"schemaVersion":"0.1","schemaVersion":"0.1"}', encoding="utf-8")
    call("duplicate JSON rejected", cli + ["validate", "--input", str(invalid)], 3)

    # Isolated, stable Git source tree: no dependence on ongoing developer edits.
    source = work / "source"; source.mkdir()
    specimen = json.loads(args.native_release.read_text(encoding="utf-8"))
    specimen["producer"]["id"] = "fixture-replay"
    write(source / "specimen.json", specimen)
    collector = source / "replay.py"
    collector.write_text('import pathlib,sys\npathlib.Path(sys.argv[-1]).write_bytes(pathlib.Path(__file__).with_name("specimen.json").read_bytes())\n', encoding="utf-8")
    for command in (["git", "init", "--quiet"], ["git", "add", "."], ["git", "-c", "user.name=Layout Fixture", "-c", "user.email=fixture@example.invalid", "commit", "--quiet", "-m", "fixture"]):
        subprocess.run(command, cwd=source, env=test_environment, check=True, capture_output=True)
    target = specimen["build"]["target"]
    profile = dict(id="fixture", executable=sys.executable, arguments=[str(collector)], workingDirectory=str(source),
                   configuration=specimen["build"]["configuration"], target={k: target[k] for k in ("os", "architecture")},
                   outputArgument="--output", requiredCapabilities=["native-values"], timeoutSeconds=10, artifact=str(collector))
    manifest = dict(schemaVersion="0.1", sourceRoot=str(source), profiles=[profile], comparisons=[dict(id="same", left="fixture", right="fixture", compareManifest=str(regression))])
    def run(name, expected, change=None):
        item = json.loads(json.dumps(manifest))
        if change:
            change(item["profiles"][0])
        path = write(work / f"{name}.json", item)
        directory = work / name
        call(name, cli + ["run", "--manifest", path, "--out-dir", str(directory)], expected)
        result = json.loads((directory / "run.json").read_text(encoding="utf-8"))
        assert result["exitCode"] == expected
        return directory
    completed = run("orchestrated-success", 0)
    enriched = json.loads((completed / "fixture/snapshot.enriched.json").read_text(encoding="utf-8"))
    assert enriched["build"]["artifactDigest"] == "sha256:" + hashlib.sha256(collector.read_bytes()).hexdigest()
    assert enriched["build"]["sourceDigest"].startswith("sha256:") and not enriched["build"]["sourceDirty"]
    call("run directory overwrite rejected", cli + ["run", "--manifest", str(work / "orchestrated-success.json"), "--out-dir", str(completed)], 3)
    run("required-runner-missing", 3, lambda p: p.update(executable=str(work / "missing-runner")))
    run("actual-architecture-mismatch", 3, lambda p: p["target"].update(architecture="arm64"))
    run("actual-configuration-mismatch", 3, lambda p: p.update(configuration="MissingConfiguration"))
    run("required-capability-missing", 3, lambda p: p.update(requiredCapabilities=["not-supported"]))
    run("artifact-missing", 3, lambda p: p.update(artifact=str(work / "missing-artifact")))
print(f"PASS: {count} CLI integration checks")
