"""Exercise the real CLI's source/artifact provenance using isolated Git repositories.

Python stdlib only. The replay collector is an explicit test double; no language
layout claims are inferred from these tests. Build the CLI before invoking this script.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--dotnet", default="dotnet")
parser.add_argument("--cli", required=True, type=Path)
args = parser.parse_args()
cli = [args.dotnet, str(args.cli.resolve())]
environment = os.environ.copy()
for variable in ("GIT_DIR", "GIT_WORK_TREE", "GIT_INDEX_FILE", "GIT_COMMON_DIR", "GIT_PREFIX"):
    environment.pop(variable, None)
count = 0


def known(value):
    return dict(state="known", value=value, evidence=dict(kind="fixture", method="provenance-test-double", version="1", inputs=[]))


def specimen():
    return dict(
        schemaVersion="0.1", snapshotId="test-double",
        producer=dict(id="provenance-replay", version="1", capabilities=["values"]),
        build=dict(buildId="collector-build", runId="collector-run", configuration="Test",
                   sourceRevision="collector-revision", sourceDirty=None, sourceDigest="unknown", artifactDigest="unknown",
                   compiler=dict(name="fixture", version="1"), runtime=dict(name="none", version="none"),
                   target=dict(os="test", architecture="x64", abi="test", pointerBits=64, bitsPerByte=8, endian="little"),
                   flags=["collector-flag"], dependencies={"fixture": "1"}),
        typeDescriptors=[dict(id="i32", kind="scalar", displayName="Int32", representation=dict(
            widthBits=known(32), category=known("integer"), signedness=known("signed"), encoding=known("binary-integer"),
            floatingFormat=dict(state="not-applicable", reason="integer")))],
        observations=[dict(id="sample", typeId="i32", displayName="Sample", status="ok", limitations=[], view="native",
                           context=dict(kind="complete-value"), origin=dict(kind="value-start", extentStartBit=0),
                           metrics=dict(valueSizeBytes=known(4), alignmentBytes=known(4), arrayStrideBytes=known(4)),
                           members=[], runtimeRegions=[], coverage=dict(fieldEnumeration="complete", extent="complete",
                                                                        occupiedRanges="complete", hiddenRegions="not-applicable"))],
        diagnostics=[], limitations=["Explicit replay fixture; no real language compiler is exercised."])


def write(path, value):
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")
    return str(path)


def git(root, *arguments):
    result = subprocess.run(["git", *arguments], cwd=root, env=environment, check=True, capture_output=True, text=True, encoding="utf-8", timeout=30)
    return result.stdout.strip()


def commit(root, message):
    git(root, "add", ".")
    git(root, "-c", "user.name=Provenance Test", "-c", "user.email=fixture@example.invalid", "commit", "--quiet", "-m", message)


def repository(path, label):
    path.mkdir()
    git(path, "init", "--quiet")
    (path / "business.txt").write_text(label, encoding="utf-8")
    commit(path, label)
    return path


with tempfile.TemporaryDirectory(prefix="layout-provenance-") as directory:
    work = Path(directory)
    left = repository(work / "left-source", "left implementation")
    right = repository(work / "right-source", "right implementation")
    artifact = work / "artifact.bin"
    artifact.write_bytes(b"stable artifact")
    snapshot = work / "specimen.json"
    write(snapshot, specimen())
    mapping = work / "compare.json"
    write(mapping, dict(schemaVersion="0.1", mode="regression", scope="value", policy="value-fields-v1",
                        cases=[dict(id="sample", left="sample", right="sample")]))
    replay = work / "replay.py"
    replay.write_text(
        "import json,pathlib,subprocess,sys\n"
        "snapshot=pathlib.Path(sys.argv[1]); action=sys.argv[2]; target=pathlib.Path(sys.argv[3])\n"
        "if action == 'mutate': target.write_text('changed while collecting',encoding='utf-8')\n"
        "if action == 'stage': subprocess.run(['git','add',str(target)],cwd=target.parent,check=True)\n"
        "pathlib.Path(sys.argv[-1]).write_bytes(snapshot.read_bytes())\n", encoding="utf-8")

    def profile(name, source=None, action="none", target=None):
        value = dict(id=name, executable=sys.executable, arguments=[str(replay), str(snapshot), action, str(target or artifact)],
                     workingDirectory=str(work), configuration="Test", target=dict(os="test", architecture="x64"),
                     outputArgument="--output", requiredCapabilities=["values"], timeoutSeconds=20, artifact=str(artifact))
        if source is not None:
            value["sourceRoot"] = os.path.relpath(source, work)
        return value

    def manifest(profiles, source=left):
        return dict(schemaVersion="0.1", sourceRoot=os.path.relpath(source, work), profiles=profiles,
                    comparisons=[dict(id="pair", left=profiles[0]["id"], right=profiles[-1]["id"], compareManifest="compare.json")])

    def run(name, value, expected=0, env=None):
        global count
        path = work / (name + ".run.json")
        write(path, value)
        output = work / name
        result = subprocess.run(cli + ["run", "--manifest", str(path), "--out-dir", str(output)],
                                env=env or environment, capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=90)
        if result.returncode != expected:
            raise AssertionError(f"{name}: expected {expected}, got {result.returncode}\n{result.stdout}\n{result.stderr}")
        summary = json.loads((output / "run.json").read_text(encoding="utf-8")) if (output / "run.json").exists() else None
        if summary is not None:
            assert summary["exitCode"] == expected
        count += 1
        print(f"PASS {name}: exit {expected}")
        return output, summary

    def enriched(output, name):
        return json.loads((output / name / "snapshot.enriched.json").read_text(encoding="utf-8"))["build"]

    output, _ = run("legacy-default", manifest([profile("default")]))
    identity = enriched(output, "default")
    assert identity["sourceRevision"] == git(left, "rev-parse", "HEAD")
    assert identity["collectorProvenance"] == specimen()["build"]
    assert identity["captureProvenance"] == dict(sourceBinding="run-default", sourceDigestScope="git-head-index-worktree-recursive-submodules-v1",
                                                artifactVerifiedStable=True, sourceRelation="unverified")
    assert identity["artifactDigest"] == "sha256:" + hashlib.sha256(artifact.read_bytes()).hexdigest()

    output, _ = run("independent-sources", manifest([profile("left"), profile("right", right)]))
    first, second = enriched(output, "left"), enriched(output, "right")
    assert first["sourceRevision"] == git(left, "rev-parse", "HEAD")
    assert second["sourceRevision"] == git(right, "rev-parse", "HEAD")
    assert first["sourceDigest"] != second["sourceDigest"]
    assert second["captureProvenance"]["sourceBinding"] == "profile"

    bad_source = manifest([profile("missing", work / "missing-source"), profile("working", right)])
    bad_source["comparisons"].append(dict(id="working-only", left="working", right="working", compareManifest="compare.json"))
    _, summary = run("missing-source-retained", bad_source, 3)
    assert [(p["id"], p["status"]) for p in summary["profiles"]] == [("missing", "error"), ("working", "captured")]
    assert [(c["id"], c["status"]) for c in summary["comparisons"]] == [("pair", "error"), ("working-only", "compared")]
    assert "Source registration failed" in summary["profiles"][0]["message"]

    run("profile-path-case-collision", manifest([profile("same"), profile("SAME")]), 3)
    bad_comparisons = manifest([profile("valid")])
    bad_comparisons["comparisons"].append(dict(id="PAIR", left="valid", right="valid", compareManifest="compare.json"))
    run("comparison-path-case-collision", bad_comparisons, 3)
    run("profile-device-name-rejected", manifest([profile("CON")]), 3)
    bad_name = manifest([profile("valid")])
    bad_name["comparisons"][0]["id"] = "LPT1"
    run("comparison-device-name-rejected", bad_name, 3)

    built = profile("built")
    built["buildSteps"] = [dict(executable=sys.executable, arguments=["-c", "import pathlib,sys;pathlib.Path(sys.argv[1]).write_bytes(b'built artifact')", str(artifact)],
                               workingDirectory=str(work), timeoutSeconds=20)]
    output, _ = run("built-in-run", manifest([built]))
    assert enriched(output, "built")["captureProvenance"]["sourceRelation"] == "built-in-run"

    run("artifact-mutation-rejected", manifest([profile("mutator", action="mutate", target=artifact)]), 3)
    artifact.write_bytes(b"stable again")
    claimed = specimen()
    claimed["build"]["artifactDigest"] = "sha256:" + "0" * 64
    write(snapshot, claimed)
    run("artifact-claim-conflict", manifest([profile("conflict")]), 3)
    digest = hashlib.sha256(artifact.read_bytes()).hexdigest()
    claimed["build"]["artifactDigest"] = "sha256:" + digest
    write(snapshot, claimed)
    run("artifact-claim-matches", manifest([profile("matches")]))
    for name, claim in (("artifact-bare-hex-matches", digest), ("artifact-uppercase-bare-matches", digest.upper()),
                        ("artifact-sha256-case-matches", "SHA256:" + digest.upper())):
        claimed["build"]["artifactDigest"] = claim
        write(snapshot, claimed)
        output, _ = run(name, manifest([profile("matches")]))
        identity = enriched(output, "matches")
        assert identity["artifactDigest"] == "sha256:" + digest
        assert identity["collectorProvenance"]["artifactDigest"] == claim
    for name, claim in (("artifact-digest-algorithm-rejected", "sha1:" + digest),
                        ("artifact-digest-nonhex-rejected", "sha256:" + "g" * 64),
                        ("artifact-digest-newline-rejected", "sha256:" + digest + "\n")):
        claimed["build"]["artifactDigest"] = claim
        write(snapshot, claimed)
        _, summary = run(name, manifest([profile("invalid")]), 3)
        assert "unsupported format" in summary["profiles"][0]["message"]
    write(snapshot, specimen())

    injected = environment.copy()
    injected.update(GIT_DIR=str(right / ".git"), GIT_WORK_TREE=str(right), GIT_INDEX_FILE=str(right / ".git/index"), GIT_COMMON_DIR=str(right / ".git"))
    output, _ = run("explicit-root-beats-git-environment", manifest([profile("environment")]), env=injected)
    assert enriched(output, "environment")["sourceRevision"] == git(left, "rev-parse", "HEAD")

    # Every source binding is frozen at run start, including later and previously captured profiles.
    run("future-source-mutation", manifest([profile("left", action="mutate", target=right / "business.txt"), profile("right", right)]), 3)
    (right / "business.txt").write_text("right implementation", encoding="utf-8")
    output, summary = run("previous-source-mutation", manifest([profile("first"), profile("second", right, "mutate", left / "business.txt")]), 3)
    assert all(p["status"] == "error" for p in summary["profiles"])
    assert all(c["status"] == "error" for c in summary["comparisons"])
    assert not (output / "pair.diff.json").exists()
    (left / "business.txt").write_text("left implementation", encoding="utf-8")

    unicode_source = left / "字段-π.txt"
    unicode_source.write_text("unicode path baseline", encoding="utf-8")
    run("unicode-source-mutation-rejected", manifest([profile("unicode", action="mutate", target=unicode_source)]), 3)

    # Exercise an actual submodule, including already-dirty tracked content and a nested gitlink.
    dependency = repository(work / "dependency-origin", "dependency implementation")
    child_origin = repository(work / "child-origin", "child implementation")
    git(dependency, "-c", "protocol.file.allow=always", "submodule", "add", "--quiet", str(child_origin), "child")
    commit(dependency, "nested dependency")
    parent = repository(work / "submodule-source", "parent implementation")
    git(parent, "-c", "protocol.file.allow=always", "submodule", "add", "--quiet", str(dependency), "dependency")
    git(parent, "-c", "protocol.file.allow=always", "submodule", "update", "--init", "--recursive", "--quiet")
    commit(parent, "dependency")
    output, _ = run("recursive-submodule", manifest([profile("submodules")], parent))
    clean_digest = enriched(output, "submodules")["sourceDigest"]
    child = parent / "dependency/child/business.txt"
    child.write_text("already dirty nested dependency", encoding="utf-8")
    output, _ = run("dirty-submodule-recorded", manifest([profile("submodules")], parent))
    assert enriched(output, "submodules")["sourceDirty"] is True
    assert enriched(output, "submodules")["sourceDigest"] != clean_digest
    run("dirty-submodule-mutation-rejected", manifest([profile("submodules", action="mutate", target=child)], parent), 3)

    # Staging unchanged bytes changes the index, while the checkout is dirty both before and after.
    (left / "business.txt").write_text("worktree change", encoding="utf-8")
    (left / "another.txt").write_text("keep worktree dirty", encoding="utf-8")
    run("index-only-mutation-rejected", manifest([profile("index", action="stage", target=left / "business.txt")]), 3)

    missing = parent / "dependency/child"
    missing.rename(parent / "dependency/child-unavailable")
    _, summary = run("uninitialized-submodule-rejected", manifest([profile("submodules")], parent), 3)
    assert summary["profiles"][0]["id"] == "submodules" and summary["profiles"][0]["status"] == "error"
    assert summary["comparisons"][0]["status"] == "error"

print(f"PASS: {count} provenance integration checks")
