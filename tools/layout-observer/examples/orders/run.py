"""Build consumer-owned C++/C# types, compare five variants, then create a replayable project bundle.

Requires a real Linux x64 P2996 toolchain and .NET SDK from global.json. No fixture snapshots are imported.
"""
import argparse
import json
import os
from pathlib import Path
import platform
import shlex
import shutil
import subprocess
import sys

EXAMPLE = Path(__file__).resolve().parent
TOOL = EXAMPLE.parents[1]
REPO = TOOL.parents[1]

def executable(value):
    path = shutil.which(value)
    if not path:
        raise ValueError("Required executable missing: " + value)
    return os.path.abspath(path)  # Preserve clang++ driver name, including symlinks.

def write(path, data):
    path.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default="clang++")
    parser.add_argument("--dotnet", default="dotnet")
    parser.add_argument("--cmake", default="cmake")
    parser.add_argument("--linker-flags", default="")
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    if platform.system() != "Linux" or platform.machine().lower() not in ("x86_64", "amd64"):
        raise ValueError("The example requires an actual Linux x64 runner; other platforms need their own explicit profiles.")
    compiler, dotnet, cmake = map(executable, (args.compiler, args.dotnet, args.cmake))
    output = args.output.resolve()
    if output.exists():
        raise ValueError("Select a fresh output directory.")
    if output.is_relative_to(REPO) and subprocess.run(["git", "check-ignore", "--quiet", str(output.relative_to(REPO))], cwd=REPO).returncode:
        raise ValueError("Output inside the source repository must be gitignored.")
    output.mkdir(parents=True)
    tooling = output / "tooling"
    subprocess.run([dotnet, "build", str(TOOL / "dotnet/LayoutObserver.Cli"), "-c", "Release", "--artifacts-path", str(tooling)], cwd=TOOL, check=True)
    cli = [dotnet, str(tooling / "bin/LayoutObserver.Cli/release/LayoutObserver.Cli.dll")]
    project = json.loads((EXAMPLE / "project.json").read_text())
    profiles = []

    def step(program, arguments):
        return dict(executable=str(program), arguments=list(map(str, arguments)), workingDirectory=str(TOOL), timeoutSeconds=300)

    for variant in project["variants"]:
        name = variant["id"]
        configuration = "Debug" if name.endswith("debug") else "Release"
        build = output / "build" / name
        if name.startswith("native"):
            pack = "1" if name.endswith("packed") else "4"
            binary = build / "typelayout-native"
            oracle = build / "orders-oracle"
            configure = ["-S", TOOL / "native", "-B", build, f"-DCMAKE_BUILD_TYPE={configuration}", f"-DCMAKE_CXX_COMPILER={compiler}",
                         f"-DTYPELAYOUT_OBSERVER_REGISTRATION_HEADER={EXAMPLE / 'native/register.hpp'}", f"-DCMAKE_CXX_FLAGS=-DORDER_PACK={pack}"]
            if args.linker_flags:
                configure.append("-DCMAKE_EXE_LINKER_FLAGS=" + args.linker_flags)
            steps = [step(cmake, configure), step(cmake, ["--build", build, "--parallel", "2"]),
                     step(compiler, ["-std=c++20", "-stdlib=libc++", f"-DORDER_PACK={pack}", EXAMPLE / "native/oracle.cpp", "-o", oracle] + shlex.split(args.linker_flags)),
                     step(oracle, [])]
            command = step(binary, ["--configuration", configuration])
            capabilities = ["native-values"]
        else:
            binary = build / f"bin/Orders/{configuration.lower()}/Orders.dll"
            steps = [step(dotnet, ["build", EXAMPLE / "managed/Orders.csproj", "-c", configuration, "--artifacts-path", build])]
            command = step(dotnet, [binary, "--configuration", configuration])
            capabilities = ["managed-values", "typed-byref"]
        profiles.append({**command, "id": name, "configuration": configuration, "sourceRoot": str(REPO),
                         "target": {"os": "linux", "architecture": "x64"}, "artifact": str(binary), "outputArgument": "--output",
                         "requiredCapabilities": capabilities, "buildSteps": steps})
        variant["snapshot"] = str(output / "run" / name / "snapshot.enriched.json")

    # The collector runner consumes expanded pair manifests. The comparison project
    # remains the canonical reusable mapping; expansion is deterministic and explicit.
    variants = {v["id"]: v for v in project["variants"]}
    mappings = {m["id"]: m["cases"][0] for m in project["mappings"]}
    pairs = []
    for pair in project["comparisons"]:
        left = mappings[variants[pair["left"]]["mapping"]]
        right = mappings[variants[pair["right"]]["mapping"]]
        right_fields = {f["id"]: f["member"] for f in right["fields"]}
        manifest = {k: pair[k] for k in ("mode", "scope", "policy")}
        manifest.update(schemaVersion="0.1", cases=[dict(id="order", left=left["observation"], right=right["observation"],
            fields=[dict(id=f["id"], left=f["member"], right=right_fields[f["id"]]) for f in left["fields"]])])
        path = output / (pair["id"] + ".compare.json"); write(path, manifest)
        pairs.append(dict(id=pair["id"], left=pair["left"], right=pair["right"], compareManifest=str(path)))
    run_manifest = output / "capture.run.json"
    write(run_manifest, dict(schemaVersion="0.1", profiles=profiles, comparisons=pairs))
    captured = subprocess.run(cli + ["run", "--manifest", str(run_manifest), "--out-dir", str(output / "run")], cwd=TOOL)
    if captured.returncode != 1:
        raise ValueError(f"Expected complete packing difference (exit 1), got {captured.returncode}; inspect run/run.json and logs.")
    summary = json.loads((output / "run/run.json").read_text())
    expected = {p["id"]: 1 if p["id"] == "packing-change" else 0 for p in pairs}
    if any(p["status"] != "captured" for p in summary["profiles"]) or {p["id"]: p.get("exitCode") for p in summary["comparisons"]} != expected:
        raise ValueError("Unexpected comparison outcome; retained reports describe the failure.")
    for profile in profiles:
        directory = output / "run" / profile["id"]
        snapshot = json.loads((directory / "snapshot.json").read_text())
        if not profile["id"].startswith("native"):
            if [o["id"] for o in snapshot["observations"]] != ["orders.managed"]:
                raise ValueError("Managed consumer capture included other observations.")
            continue
        oracle = json.loads((directory / "build-3.stdout.log").read_text())
        root = next(o for o in snapshot["observations"] if o["id"] == "orders.native")
        if len(snapshot["observations"]) != 1 or root["metrics"]["valueSizeBytes"]["value"] != oracle["size"]:
            raise ValueError("Native consumer observations/size disagree with independent oracle.")
        if {f["id"]: f["offsetBits"]["value"] for f in root["members"]} != {k: v * 8 for k, v in oracle.items() if k != "size"}:
            raise ValueError("Native consumer field offsets disagree with independent oracle.")
    project_path = output / "orders.project.json"; write(project_path, project)
    compared = subprocess.run(cli + ["project", "--manifest", str(project_path), "--out-dir", str(output / "report")], cwd=TOOL)
    if compared.returncode != 1:
        raise ValueError(f"Project comparison expected complete packing difference (exit 1), got {compared.returncode}.")
    project_result = json.loads((output / "report/result.json").read_text())
    if {p["id"]: p.get("exitCode") for p in project_result["comparisons"]} != expected or any(v["status"] != "ok" for v in project_result["variants"]):
        raise ValueError("Project comparison pairs differ from the required outcomes.")
    replayed = subprocess.run(cli + ["project", "--manifest", str(output / "report/project.json"), "--out-dir", str(output / "replay")], cwd=TOOL)
    if replayed.returncode != 1:
        raise ValueError("Portable project replay did not retain the expected difference.")
    for pair in project_result["comparisons"]:
        if (output / "report" / pair["diff"]).read_bytes() != (output / "replay" / pair["diff"]).read_bytes():
            raise ValueError("Portable replay changed comparison " + pair["id"])
    print("PASS: five real consumer builds, four complete equal pairs, packing difference, independent C++ oracle and replayable bundle")
    print(output / "report/index.html")
    return 0

if __name__ == "__main__":
    try:
        sys.exit(main())
    except (ValueError, OSError, AssertionError, subprocess.CalledProcessError) as error:
        print("ERROR:", error, file=sys.stderr)
        sys.exit(3)
