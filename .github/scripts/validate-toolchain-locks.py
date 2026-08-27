#!/usr/bin/env python3
"""Validate immutable TypeLayout toolchain source and output locks."""

import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
from urllib.parse import urlsplit


GCC_VERSION = "16.2.0"
GCC_SHA512 = (
    "c51c30ca7422d0cbecf504b2e0f33c3aca31e0f90a76b65217f465163fa6fa17"
    "b3f5de39e145c47e5bab90ac0ce7fff3b03c8d553ae36e01faaea5a50f8648d1"
)
CLANG_COMMIT = "060be17654102019e14810c3f948ef85a490755f"
RUNNER_IMAGES_COMMIT = "564e58dbe650c507ccba1171f6159c12f26820c8"
DEBIAN_IMAGE = (
    "docker.io/library/debian@sha256:"
    "d7e12182ce18b85b93007c1dedf31f2d29e01ccf3182cc4017c709b6259bc132"
)
BUILDKIT_IMAGE = (
    "docker.io/moby/buildkit@sha256:"
    "28a898719c18a33f4e8000685287fa36fd0dd9560c6440227d3a732d79bb41d8"
)
SNAPSHOT = "https://snapshot.debian.org/archive/debian/20260824T000000Z/"
GCC_FLAGS = "-std=c++26 -freflection -O3 -fstrict-aliasing"
CLANG_CORE_FLAGS = (
    "-std=c++26 -freflection -freflection-latest -stdlib=libc++ "
    "-O3 -fstrict-aliasing"
)
CLANG_FLAGS = (
    CLANG_CORE_FLAGS
    + " -nostdinc++ -isystem ${TOOLCHAIN_ROOT}/include/c++/v1 "
    "-L ${TOOLCHAIN_ROOT}/lib/${TARGET_TRIPLE} "
    "-Wl,-rpath,${TOOLCHAIN_ROOT}/lib/${TARGET_TRIPLE}"
)
MACOS_FLAGS = (
    CLANG_CORE_FLAGS
    + " -nostdinc++ -isystem ${TOOLCHAIN_ROOT}/include/c++/v1 "
    "-isysroot ${SDKROOT} -mmacosx-version-min=15.0 "
    "-L ${TOOLCHAIN_ROOT}/lib -Wl,-rpath,${TOOLCHAIN_ROOT}/lib"
)
GCC_CONFIGURE_FLAGS = [
    "--enable-languages=c,c++",
    "--disable-bootstrap",
    "--disable-multilib",
    "--disable-nls",
    "--enable-checking=release",
]
P2996_CMAKE_FLAGS = [
    "-DCMAKE_BUILD_TYPE=Release",
    "-DLLVM_INCLUDE_TESTS=OFF",
    "-DCLANG_INCLUDE_TESTS=OFF",
    "-DLLVM_INCLUDE_EXAMPLES=OFF",
    "-DLLVM_INCLUDE_BENCHMARKS=OFF",
    "-DLLVM_INCLUDE_DOCS=OFF",
    "-DCLANG_BUILD_EXAMPLES=OFF",
    "-DCLANG_DEFAULT_CXX_STDLIB=libc++",
    "-DLLVM_INSTALL_TOOLCHAIN_ONLY=ON",
    "-DLLVM_PARALLEL_LINK_JOBS=1",
]

PREREQUISITES = {
    "gmp": (
        "6.3.0",
        "gmp-6.3.0.tar.bz2",
        "3b684c9bcb9ede2b7e54d0ba4c9764bfa17c20d4f3000017c553b6f1e135b536"
        "949580ff37341680c25dc236cfe0ba1db8cfdfe619ce013656189ef0871b89f8",
    ),
    "mpfr": (
        "4.2.2",
        "mpfr-4.2.2.tar.bz2",
        "0176e50808dcc07afbf5bc3e38bf9b7b21918e5f194aa0bfd860d99b00c470630"
        "aef149776c4be814a61c44269c3a5b9a4b0b1c0fcd4c9feb1459d8466452da8",
    ),
    "mpc": (
        "1.3.1",
        "mpc-1.3.1.tar.gz",
        "4bab4ef6076f8c5dfdc99d810b51108ced61ea2942ba0c1c932d624360a5473df"
        "20d32b300fc76f2ba4aa2a97e1f275c9fd494a1ba9f07c4cb2ad7ceaeb1ae97",
    ),
    "isl": (
        "0.24",
        "isl-0.24.tar.bz2",
        "aab3bddbda96b801d0f56d2869f943157aad52a6f6e6a61745edd740234c635c3"
        "8231af20bc3f1a08d416a5e973a90e18249078ed8e4ae2f1d5de57658738e95",
    ),
}

ACTION_PINS = {
    "checkout": "11d5960a326750d5838078e36cf38b85af677262",
    "upload_artifact": "ea165f8d65b6e75b540449e92b4886f43607fa02",
    "download_artifact": "d3f86a106a0bac45b974a628896c90dbdf5c8093",
    "docker_login": "c94ce9fb468520275223c153574b00df6fe4bcc9",
    "setup_buildx": "8d2750c68a42422c14e847fe6c8ac0403b4cbd6f",
    "build_push": "10e90e3645eae34f1e60eeb005ba3a3d33f178e8",
    "github_release": "3bb12739c298aeb8a4eeaf626c5b8d85266b0e65",
}

PACKAGE_LOCKS = {
    "gcc_builder": [
        "build-essential=12.12",
        "bison=2:3.8.2+dfsg-1+b2",
        "ca-certificates=20250419",
        "curl=8.14.1-2+deb13u4",
        "flex=2.6.4-8.2+b4",
        "mawk=1.3.4.20250131-1",
        "gzip=1.13-1",
        "make=4.4.1-2",
        "patch=2.8-2",
        "tar=1.35+dfsg-3.1",
        "xz-utils=5.8.1-1+deb13u1",
        "bzip2=1.0.8-6",
        "zlib1g-dev=1:1.3.dfsg+really1.3.1-1+b1",
        "libzstd-dev=1.5.7+dfsg-1",
    ],
    "gcc_runtime": [
        "ca-certificates=20250419",
        "cmake=3.31.6-2",
        "git=1:2.47.3-0+deb13u1",
        "ninja-build:amd64=1.12.1-1",
        "ninja-build:arm64=1.12.1-1+b1",
        "python3=3.13.5-1",
        "zlib1g=1:1.3.dfsg+really1.3.1-1+b1",
        "libzstd1=1.5.7+dfsg-1",
    ],
    "p2996_builder": [
        "build-essential=12.12",
        "ca-certificates=20250419",
        "cmake=3.31.6-2",
        "curl=8.14.1-2+deb13u4",
        "git=1:2.47.3-0+deb13u1",
        "libncurses-dev=6.5+20250216-2",
        "libxml2-dev=2.12.7+dfsg+really2.9.14-2.1+deb13u3",
        "libzstd-dev=1.5.7+dfsg-1",
        "ninja-build:amd64=1.12.1-1",
        "ninja-build:arm64=1.12.1-1+b1",
        "python3=3.13.5-1",
        "tar=1.35+dfsg-3.1",
        "zlib1g-dev=1:1.3.dfsg+really1.3.1-1+b1",
        "zstd=1.5.7+dfsg-1",
    ],
    "p2996_runtime": [
        "ca-certificates=20250419",
        "cmake=3.31.6-2",
        "git=1:2.47.3-0+deb13u1",
        "libtinfo6=6.5+20250216-2",
        "libxml2=2.12.7+dfsg+really2.9.14-2.1+deb13u3",
        "libzstd1=1.5.7+dfsg-1",
        "ninja-build:amd64=1.12.1-1",
        "ninja-build:arm64=1.12.1-1+b1",
        "python3=3.13.5-1",
        "zlib1g=1:1.3.dfsg+really1.3.1-1+b1",
        "zstd=1.5.7+dfsg-1",
    ],
}

RECIPE_PATHS = (
    ".gitattributes",
    ".github/docker/Dockerfile.gcc16",
    ".github/docker/Dockerfile.p2996",
    ".github/docker/docker-bake.hcl",
    ".github/scripts/build-p2996-macos.sh",
    ".github/scripts/verify-p2996-toolchain.sh",
    ".github/workflows/toolchain-images.yml",
)

GIT_ATTRIBUTES = (
    ".github/docker/** text eol=lf\n"
    ".github/scripts/** text eol=lf\n"
    ".github/workflows/** text eol=lf\n"
    "tools/*.py text eol=lf\n"
    "tools/*.sh text eol=lf\n"
)


class LockError(ValueError):
    pass


def _load_evidence_module():
    repository_root = Path(__file__).resolve().parents[2]
    module_path = repository_root / "tools/relocatable_world_evidence.py"
    specification = importlib.util.spec_from_file_location(
        "typelayout_relocatable_world_evidence", module_path
    )
    if specification is None or specification.loader is None:
        raise LockError(f"cannot load evidence module from {module_path}")
    module = importlib.util.module_from_spec(specification)
    specification.loader.exec_module(module)
    return module


def _sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def _artifact_digest(label):
    return hashlib.sha256(label.encode("utf-8")).hexdigest()


def _synthetic_outputs(sources, sources_path):
    """Build a complete non-publishable output fixture for source-only validation."""
    sources_digest = _sha256(sources_path)
    clang_revision = sources["p2996"]["compiler_revision"]
    linux = {}
    for toolchain in ("gcc", "p2996"):
        compiler = sources["gcc" if toolchain == "gcc" else "p2996"]
        platforms = {}
        for platform, target in (
            ("linux/amd64", "x86_64-unknown-linux-gnu"),
            ("linux/arm64", "aarch64-unknown-linux-gnu"),
        ):
            platforms[platform] = {
                "manifest_digest": "sha256:"
                + _artifact_digest(f"fixture:{toolchain}:{platform}"),
                "target": target,
            }
        linux[toolchain] = {
            "repository": f"ghcr.io/ximicpp/fixture-{toolchain}",
            "index_digest": "sha256:" + _artifact_digest(f"index:{toolchain}"),
            "compiler_revision": compiler["compiler_revision"],
            "compiler_version": f"fixture {toolchain}",
            "stdlib": "fixture-stdlib",
            "platforms": platforms,
        }

    release_root = (
        "https://github.com/ximicpp/TypeLayout/releases/download/"
        f"typelayout-toolchains-{sources_digest}"
    )
    macos = {}
    for node, source_node in sources["macos"]["nodes"].items():
        architecture = source_node["architecture"]
        macos[node] = {
            "url": (
                f"{release_root}/p2996-macos-{architecture}-"
                f"{clang_revision}.tar.zst"
            ),
            "archive_sha256": _artifact_digest(f"archive:{node}"),
            "compiler_revision": clang_revision,
            "compiler_version": "fixture clang",
            "target": f"{architecture}-apple-macosx15.0.0",
            "stdlib": "fixture-libc++",
            **{
                key: source_node[key]
                for key in (
                    "xcode_version",
                    "xcode_build",
                    "sdk_version",
                    "sdk_build",
                    "deployment_target",
                )
            },
            "observed_runner": {
                "image_os": "diagnostic-only",
                "image_version": "diagnostic-only",
            },
        }
    return {
        "schema": 1,
        "sources_sha256": sources_digest,
        "source_sha": "a" * 40,
        "workflow_run": "1.1",
        "linux": linux,
        "macos": macos,
    }


def _validate_with_shared_policy(module, sources_path, outputs_path=None):
    sources = module.load_json(sources_path)
    if outputs_path is None:
        outputs = _synthetic_outputs(sources, sources_path)
        with tempfile.TemporaryDirectory() as directory:
            synthetic_path = Path(directory) / "toolchains.lock"
            synthetic_path.write_text(
                json.dumps(outputs, indent=2, sort_keys=True) + "\n",
                encoding="utf-8",
            )
            for node in module.NODES:
                module.load_node_toolchain_policy(
                    sources_path, synthetic_path, node
                )
    else:
        for node in module.NODES:
            module.load_node_toolchain_policy(sources_path, outputs_path, node)
    return sources


def _require_equal(actual, expected, where):
    if actual != expected:
        raise LockError(f"{where} must be {expected!r}, got {actual!r}")


def _validate_reviewed_sources(sources):
    gcc = sources["gcc"]
    _require_equal(gcc["version"], GCC_VERSION, "gcc.version")
    _require_equal(gcc["compiler_family"], "gcc", "gcc.compiler_family")
    _require_equal(gcc["compiler_revision"], GCC_VERSION, "gcc.compiler_revision")
    _require_equal(gcc["flags"], GCC_FLAGS, "gcc.flags")
    _require_equal(
        gcc["source"]["url"],
        "https://gcc.gnu.org/pub/gcc/releases/gcc-16.2.0/gcc-16.2.0.tar.xz",
        "gcc source URL",
    )
    _require_equal(gcc["source"]["filename"], "gcc-16.2.0.tar.xz", "gcc source")
    _require_equal(gcc["source"]["sha512"], GCC_SHA512, "gcc source SHA512")
    _require_equal(
        gcc["configure_flags"], GCC_CONFIGURE_FLAGS, "gcc.configure_flags"
    )
    for name, (version, filename, digest) in PREREQUISITES.items():
        record = gcc["prerequisites"][name]
        _require_equal(record["version"], version, f"{name}.version")
        _require_equal(
            record["url"],
            f"https://gcc.gnu.org/pub/gcc/infrastructure/{filename}",
            f"{name}.url",
        )
        _require_equal(record["filename"], filename, f"{name}.filename")
        _require_equal(record["sha512"], digest, f"{name}.sha512")

    p2996 = sources["p2996"]
    _require_equal(
        p2996["repository"],
        "https://github.com/bloomberg/clang-p2996.git",
        "p2996.repository",
    )
    _require_equal(p2996["commit"], CLANG_COMMIT, "p2996.commit")
    _require_equal(p2996["compiler_family"], "clang", "p2996.compiler_family")
    _require_equal(
        p2996["compiler_revision"], CLANG_COMMIT, "p2996.compiler_revision"
    )
    _require_equal(p2996["flags"], CLANG_FLAGS, "p2996.flags")
    _require_equal(p2996["projects"], ["clang"], "p2996.projects")
    _require_equal(
        p2996["runtimes"],
        ["libcxx", "libcxxabi", "libunwind"],
        "p2996.runtimes",
    )
    _require_equal(p2996["llvm_targets"], ["X86", "AArch64"], "p2996.llvm_targets")
    _require_equal(
        p2996["cmake_flags"], P2996_CMAKE_FLAGS, "p2996.cmake_flags"
    )

    linux = sources["linux"]
    _require_equal(
        linux["platforms"],
        {
            "linux/amd64": {
                "architecture": "x86_64",
                "runner": "ubuntu-24.04",
            },
            "linux/arm64": {
                "architecture": "arm64",
                "runner": "ubuntu-24.04-arm",
            },
        },
        "linux.platforms",
    )
    _require_equal(
        linux["base_images"],
        {key: DEBIAN_IMAGE for key in linux["base_images"]},
        "linux.base_images",
    )
    _require_equal(
        linux["apt"],
        {"snapshot": SNAPSHOT, "suites": ["trixie"], "components": ["main"]},
        "linux.apt",
    )

    docker = linux["docker"]
    _require_equal(
        docker["runner_images_commit"],
        RUNNER_IMAGES_COMMIT,
        "linux.docker.runner_images_commit",
    )
    _require_equal(docker["buildx_version"], "0.36.1", "Docker Buildx version")
    _require_equal(
        docker["buildkit_image"], BUILDKIT_IMAGE, "Docker BuildKit image"
    )
    for runner in ("ubuntu-24.04", "ubuntu-24.04-arm"):
        _require_equal(
            docker["runners"][runner],
            {"client_version": "28.0.4", "server_version": "28.0.4"},
            f"Docker versions for {runner}",
        )
    _require_equal(
        sources["linux"]["packages"], PACKAGE_LOCKS, "linux.packages"
    )

    macos = sources["macos"]
    _require_equal(
        macos["runner_images_repository"],
        "https://github.com/actions/runner-images.git",
        "macos.runner_images_repository",
    )
    _require_equal(
        macos["runner_images_commit"],
        RUNNER_IMAGES_COMMIT,
        "macos.runner_images_commit",
    )
    expected_apple = {
        "xcode_version": "16.4",
        "xcode_build": "16F6",
        "sdk_version": "15.5",
        "sdk_build": "24F74",
        "deployment_target": "15.0",
    }
    for node, record in macos["nodes"].items():
        for key, value in expected_apple.items():
            _require_equal(record[key], value, f"macos.nodes.{node}.{key}")
        _require_equal(record["flags"], MACOS_FLAGS, f"macos.nodes.{node}.flags")
    _require_equal(
        {
            node: {
                key: record[key]
                for key in ("runner", "architecture", "llvm_target")
            }
            for node, record in macos["nodes"].items()
        },
        {
            "arm64_macos_clang": {
                "runner": "macos-15",
                "architecture": "arm64",
                "llvm_target": "AArch64",
            },
            "x86_64_macos_clang": {
                "runner": "macos-15-intel",
                "architecture": "x86_64",
                "llvm_target": "X86",
            },
        },
        "macos.nodes platform mapping",
    )

    _require_equal(sources["actions"], ACTION_PINS, "reviewed Action pins")


def _normalized_recipe_bytes(path):
    data = path.read_bytes().replace(b"\r\n", b"\n")
    if b"\r" in data:
        raise LockError(f"recipe contains a non-CRLF carriage return: {path}")
    return data


def _validate_recipes(sources, recipe_root):
    root = Path(recipe_root).resolve()
    for relative in RECIPE_PATHS:
        path = (root / relative).resolve()
        try:
            path.relative_to(root)
        except ValueError as error:
            raise LockError(f"recipe escapes root: {relative}") from error
        if not path.is_file():
            raise LockError(f"recipe is missing: {relative}")
        actual = hashlib.sha256(_normalized_recipe_bytes(path)).hexdigest()
        expected = sources["recipes"][relative]
        if actual != expected:
            raise LockError(
                f"recipe SHA256 mismatch for {relative}: {actual} != {expected}"
            )
    attributes = _normalized_recipe_bytes(root / ".gitattributes").decode("utf-8")
    if attributes != GIT_ATTRIBUTES:
        raise LockError(".gitattributes does not contain the exact LF policy")

    dockerfiles = {
        "gcc_builder": root / ".github/docker/Dockerfile.gcc16",
        "gcc_runtime": root / ".github/docker/Dockerfile.gcc16",
        "p2996_builder": root / ".github/docker/Dockerfile.p2996",
        "p2996_runtime": root / ".github/docker/Dockerfile.p2996",
    }
    for package_set, packages in sources["linux"]["packages"].items():
        recipe = dockerfiles[package_set]
        content = _normalized_recipe_bytes(recipe).decode("utf-8")
        for package in packages:
            if package not in content:
                raise LockError(
                    f"{recipe.name} does not install locked {package_set} "
                    f"package {package}"
                )


def _validate_outputs(module, sources_path, outputs_path):
    sources = module.load_json(sources_path)
    outputs = module.load_json(outputs_path)
    expected_repositories = {
        "gcc": "ghcr.io/ximicpp/typelayout-gcc16",
        "p2996": "ghcr.io/ximicpp/typelayout-p2996",
    }
    for toolchain, repository in expected_repositories.items():
        _require_equal(
            outputs["linux"][toolchain]["repository"],
            repository,
            f"output Linux repository {toolchain}",
        )
    sources_digest = _sha256(sources_path)
    for node, record in outputs["macos"].items():
        parsed = urlsplit(record["url"])
        if parsed.hostname != "github.com" or parsed.query or parsed.fragment:
            raise LockError(f"mutable or non-GitHub macOS archive URL for {node}")
        architecture = sources["macos"]["nodes"][node]["architecture"]
        expected_path = (
            "/ximicpp/TypeLayout/releases/download/"
            f"typelayout-toolchains-{sources_digest}/"
            f"p2996-macos-{architecture}-{CLANG_COMMIT}.tar.zst"
        )
        if record["url"] != f"https://github.com{expected_path}":
            raise LockError(f"macOS archive URL is not immutable for {node}")


def validate(sources_path, recipe_root, outputs_path=None):
    module = _load_evidence_module()
    try:
        sources = _validate_with_shared_policy(
            module, Path(sources_path), Path(outputs_path) if outputs_path else None
        )
    except (KeyError, TypeError, ValueError, OSError) as error:
        raise LockError(str(error)) from error
    _validate_reviewed_sources(sources)
    _validate_recipes(sources, recipe_root)
    if outputs_path:
        _validate_outputs(module, Path(sources_path), Path(outputs_path))
    return module, sources


def parse_arguments(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("--sources", required=True)
    parser.add_argument("--outputs")
    parser.add_argument("--recipe-root", required=True)
    parser.add_argument("--print-image", choices=("gcc16", "p2996"))
    arguments = parser.parse_args(argv)
    if arguments.print_image and not arguments.outputs:
        parser.error("--print-image requires --outputs")
    return arguments


def main(argv=None):
    arguments = parse_arguments(argv)
    try:
        _, sources = validate(
            arguments.sources, arguments.recipe_root, arguments.outputs
        )
        if arguments.print_image:
            outputs = json.loads(Path(arguments.outputs).read_text(encoding="utf-8"))
            key = "gcc" if arguments.print_image == "gcc16" else "p2996"
            record = outputs["linux"][key]
            print(f"{record['repository']}@{record['index_digest']}")
        elif arguments.outputs:
            print("TOOLCHAIN LOCKS PASS nodes=6 linux_indexes=2 macos_archives=2")
        else:
            print(
                "SOURCE LOCK PASS "
                f"gcc={sources['gcc']['version']} "
                f"clang={sources['p2996']['commit']} "
                f"recipes={len(sources['recipes'])}"
            )
        return 0
    except (LockError, OSError, UnicodeError, json.JSONDecodeError) as error:
        print(f"toolchain lock error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
