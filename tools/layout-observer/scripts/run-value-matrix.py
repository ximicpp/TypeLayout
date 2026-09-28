"""Build and execute the required Linux native/managed Debug+Release matrix.

Python stdlib only. A real reflection compiler and .NET SDK from global.json are required.
No runner is emulated or silently skipped. All build arguments and outputs are retained.
"""
import argparse
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys

TOOL = Path(__file__).resolve().parents[1]
REPO = TOOL.parents[1]

def executable(value):
    resolved = shutil.which(value)
    if not resolved:
        raise ValueError(f"Required executable is missing: {value}")
    # clang++ commonly points to clang-N; following that link changes argv[0]
    # and loses the C++ driver's automatic standard-library link behavior.
    return os.path.abspath(resolved)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default="clang++")
    parser.add_argument("--dotnet", default="dotnet")
    parser.add_argument("--cmake", default="cmake")
    parser.add_argument("--linker-flags", default="")
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    if platform.system() != "Linux" or platform.machine().lower() not in ("x86_64", "amd64"):
        raise ValueError("This required matrix needs an actual Linux x64 runner. Use explicit run profiles for other targets.")
    compiler, dotnet, cmake = map(executable, (args.compiler, args.dotnet, args.cmake))
    output = args.output.resolve()
    if output.exists():
        raise ValueError("Output exists; select a fresh run directory.")
    if output.is_relative_to(REPO):
        ignored = subprocess.run(["git", "check-ignore", "--quiet", str(output.relative_to(REPO))], cwd=REPO).returncode == 0
        if not ignored:
            raise ValueError("Generated build/run output inside the repository must be gitignored; use tools/layout-observer/artifacts/NAME.")
    output.mkdir(parents=True)
    cli_artifacts = output / "tooling"
    subprocess.run([dotnet, "build", str(TOOL / "dotnet/LayoutObserver.Cli"), "-c", "Release", "--artifacts-path", str(cli_artifacts)], cwd=TOOL, check=True)
    cli = cli_artifacts / "bin/LayoutObserver.Cli/release/LayoutObserver.Cli.dll"
    profiles = []
    for configuration in ("Debug", "Release"):
        suffix = configuration.lower()
        native = output / f"build/native-{suffix}"
        managed = output / f"build/managed-{suffix}"
        configure = ["-S", str(TOOL / "native"), "-B", str(native), f"-DCMAKE_BUILD_TYPE={configuration}", f"-DCMAKE_CXX_COMPILER={compiler}"]
        if args.linker_flags:
            configure.append(f"-DCMAKE_EXE_LINKER_FLAGS={args.linker_flags}")
        def step(program, arguments):
            return dict(executable=program, arguments=arguments, workingDirectory=str(TOOL), timeoutSeconds=300)
        for language, program, arguments, artifact, capabilities, steps in (
            ("native", str(native / "typelayout-native"), ["--configuration", configuration], str(native / "typelayout-native"),
             ["native-values"], [step(cmake, configure), step(cmake, ["--build", str(native), "--parallel", "2"]), step(str(Path(cmake).with_name("ctest")), ["--test-dir", str(native), "--output-on-failure"])]),
            ("managed", dotnet, [str(managed / f"bin/LayoutObserver.Managed/{suffix}/LayoutObserver.Managed.dll"), "--configuration", configuration], str(managed / f"bin/LayoutObserver.Managed/{suffix}/LayoutObserver.Managed.dll"),
             ["managed-values", "typed-byref", "inline-array"], [step(dotnet, ["build", str(TOOL / "dotnet/LayoutObserver.Managed"), "-c", configuration, "--artifacts-path", str(managed)])]),
        ):
            profiles.append({**step(program, arguments), "id": f"{language}-{suffix}", "configuration": configuration,
                             "target": {"os": "linux", "architecture": "x64"}, "outputArgument": "--output", "requiredCapabilities": capabilities,
                             "artifact": artifact, "buildSteps": steps})
    comparisons = [dict(id=f"cross-{c}", left=f"native-{c}", right=f"managed-{c}", compareManifest=str(TOOL / "cases/shared.compare.json")) for c in ("debug", "release")]
    comparisons += [dict(id=f"{l}-builds", left=f"{l}-debug", right=f"{l}-release", compareManifest=str(TOOL / "cases/regression.compare.json")) for l in ("native", "managed")]
    manifest = output / "matrix.run.json"
    manifest.write_text(json.dumps(dict(schemaVersion="0.1", sourceRoot=str(REPO), profiles=profiles, comparisons=comparisons), indent=2) + "\n", encoding="utf-8")
    return subprocess.run([dotnet, str(cli), "run", "--manifest", str(manifest), "--out-dir", str(output / "run")], cwd=TOOL).returncode

if __name__ == "__main__":
    try:
        sys.exit(main())
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        sys.exit(3)
