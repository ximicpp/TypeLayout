#!/usr/bin/env python3
"""Validate immutable TypeLayout toolchain source and output locks."""

import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import stat
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
DOCKERFILE_FRONTEND = (
    "docker.io/docker/dockerfile:1.7@sha256:"
    "a57df69d0ea827fb7266491f2813635de6f17269be881f696fbfdf2d83dda33e"
)
SNAPSHOT = "https://snapshot.debian.org/archive/debian/20260824T000000Z/"
GCC_FLAGS = "-std=c++26 -freflection -O3 -fstrict-aliasing"
CLANG_CORE_FLAGS = (
    "-std=c++26 -freflection -freflection-latest -stdlib=libc++ "
    "-O3 -fstrict-aliasing"
)
CLANG_FLAGS = (
    CLANG_CORE_FLAGS
    + " -nostdinc++ "
    "-isystem ${TOOLCHAIN_ROOT}/include/${TARGET_TRIPLE}/c++/v1 "
    "-isystem ${TOOLCHAIN_ROOT}/include/c++/v1 "
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
    "--prefix=/opt/gcc-16.2.0",
    "--enable-languages=c,c++",
    "--disable-bootstrap",
    "--disable-multilib",
    "--disable-nls",
    "--enable-checking=release",
]
P2996_CMAKE_FLAGS = [
    "-DCMAKE_BUILD_TYPE=Release",
    "-DLLVM_ENABLE_PROJECTS=clang",
    "-DLLVM_ENABLE_RUNTIMES=libcxx;libcxxabi;libunwind",
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
P2996_PLATFORM_CMAKE_FLAGS = {
    "linux/amd64": [
        "-DCMAKE_INSTALL_PREFIX=/opt/p2996-toolchain",
        "-DLLVM_TARGETS_TO_BUILD=X86",
        "-DLLVM_ENABLE_PER_TARGET_RUNTIME_DIR=ON",
    ],
    "linux/arm64": [
        "-DCMAKE_INSTALL_PREFIX=/opt/p2996-toolchain",
        "-DLLVM_TARGETS_TO_BUILD=AArch64",
        "-DLLVM_ENABLE_PER_TARGET_RUNTIME_DIR=ON",
    ],
    "macos/arm64": [
        "-DCMAKE_INSTALL_PREFIX=${TOOLCHAIN_ROOT}",
        "-DCMAKE_OSX_ARCHITECTURES=arm64",
        "-DCMAKE_OSX_SYSROOT=${SDKROOT}",
        "-DCMAKE_OSX_DEPLOYMENT_TARGET=15.0",
        "-DLLVM_TARGETS_TO_BUILD=AArch64",
        "-DLLVM_ENABLE_PER_TARGET_RUNTIME_DIR=OFF",
    ],
    "macos/x86_64": [
        "-DCMAKE_INSTALL_PREFIX=${TOOLCHAIN_ROOT}",
        "-DCMAKE_OSX_ARCHITECTURES=x86_64",
        "-DCMAKE_OSX_SYSROOT=${SDKROOT}",
        "-DCMAKE_OSX_DEPLOYMENT_TARGET=15.0",
        "-DLLVM_TARGETS_TO_BUILD=X86",
        "-DLLVM_ENABLE_PER_TARGET_RUNTIME_DIR=OFF",
    ],
}

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
        "binutils=2.44-3",
        "ca-certificates=20250419",
        "cmake=3.31.6-2",
        "git=1:2.47.3-0+deb13u1",
        "libc6-dev=2.41-12+deb13u3",
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
        "binutils=2.44-3",
        "ca-certificates=20250419",
        "cmake=3.31.6-2",
        "git=1:2.47.3-0+deb13u1",
        "libc6-dev=2.41-12+deb13u3",
        "libgcc-14-dev=14.2.0-19",
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
# These normalized full-file hashes are an independent review gate: rebasing a
# source lock recipe hash cannot authorize changed executable build semantics.
# The workflow joins this table only after Task 5 freezes its final bytes.
REVIEWED_RECIPE_SHA256 = {
    ".github/docker/Dockerfile.gcc16": (
        "b2ab4c2cdab754ca863621248402dd6e05f5aabfeb88b03fd86d169dfa2c2253"
    ),
    ".github/docker/Dockerfile.p2996": (
        "ecd9900e0778f8a20d60edc4d5fb9f5812543ac4cd4f98594b59bf23adf1630e"
    ),
    ".github/docker/docker-bake.hcl": (
        "ec7978e3b34056c46745623579889d416db0f5c6faa9df8a86bb933633e7f18b"
    ),
    ".github/scripts/build-p2996-macos.sh": (
        "5dd189f68eed06050fd53dd8aef3e6eba00bfcd3ba89c18eb352a98836f2393a"
    ),
    ".github/scripts/verify-p2996-toolchain.sh": (
        "cb8597b111b572d64c7d81d2d713db80a0b7646b8a931f95303a87c1f00bd2c8"
    ),
    ".github/workflows/toolchain-images.yml": (
        "f9f9a526c33c1c6c0fe0ee1aa3f23abb43c9bd5d6c657dd1483fd1a21651de49"
    ),
}

GIT_ATTRIBUTES = (
    ".github/docker/** text eol=lf\n"
    ".github/scripts/** text eol=lf\n"
    ".github/workflows/** text eol=lf\n"
    "tools/*.py text eol=lf\n"
    "tools/*.sh text eol=lf\n"
)
MAX_RECIPE_BYTES = 16 * 1024 * 1024


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
            "compiler_version": (
                "16.2.0" if toolchain == "gcc" else "clang version 21.0.0"
            ),
            "stdlib": (
                "libstdc++-20260807"
                if toolchain == "gcc"
                else "libc++-210000"
            ),
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
            "compiler_version": "clang version 21.0.0",
            "target": f"{architecture}-apple-macosx15.0.0",
            "stdlib": "libc++-210000",
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
    _require_equal(
        p2996["platform_cmake_flags"],
        P2996_PLATFORM_CMAKE_FLAGS,
        "p2996.platform_cmake_flags",
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
    _require_equal(
        docker["dockerfile_frontend"],
        DOCKERFILE_FRONTEND,
        "Dockerfile frontend",
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


def _secure_recipe_bytes(root, relative):
    root = Path(root).absolute()
    relative_path = Path(relative)
    if relative_path.is_absolute() or any(
        part in ("", ".", "..") for part in relative_path.parts
    ):
        raise LockError(f"recipe has an unsafe path: {relative}")
    components = (root,) + tuple(
        root / Path(*relative_path.parts[:index])
        for index in range(1, len(relative_path.parts) + 1)
    )
    final_stat = None
    final_path = None
    for index, component in enumerate(components):
        try:
            status = os.lstat(component)
        except OSError as error:
            raise LockError(f"recipe is missing: {relative}") from error
        if stat.S_ISLNK(status.st_mode):
            raise LockError(f"recipe path contains a symbolic link: {relative}")
        if index < len(components) - 1 and not stat.S_ISDIR(status.st_mode):
            raise LockError(
                f"recipe path component is not a directory: {relative}"
            )
        final_stat = status
        final_path = component
    if final_stat is None or not stat.S_ISREG(final_stat.st_mode):
        raise LockError(f"recipe is not a regular file: {relative}")
    flags = (
        os.O_RDONLY
        | getattr(os, "O_BINARY", 0)
        | getattr(os, "O_NOFOLLOW", 0)
    )
    try:
        descriptor = os.open(final_path, flags)
    except OSError as error:
        raise LockError(f"cannot open recipe safely: {relative}") from error
    try:
        opened_stat = os.fstat(descriptor)
        if not stat.S_ISREG(opened_stat.st_mode) or (
            opened_stat.st_dev,
            opened_stat.st_ino,
        ) != (final_stat.st_dev, final_stat.st_ino):
            raise LockError(f"recipe changed during secure open: {relative}")
        chunks = []
        total = 0
        while True:
            chunk = os.read(descriptor, 1024 * 1024)
            if not chunk:
                break
            total += len(chunk)
            if total > MAX_RECIPE_BYTES:
                raise LockError(
                    f"recipe exceeds {MAX_RECIPE_BYTES} bytes: {relative}"
                )
            chunks.append(chunk)
        return b"".join(chunks)
    finally:
        os.close(descriptor)


def _normalized_recipe_bytes(data, label):
    data = data.replace(b"\r\n", b"\n")
    if b"\r" in data:
        raise LockError(f"recipe contains a non-CRLF carriage return: {label}")
    return data


def _docker_stages(content, recipe_name):
    stages = re.split(r"(?m)^FROM[ \t]+", content)[1:]
    if len(stages) != 2:
        raise LockError(
            f"{recipe_name} must contain exactly builder and runtime stages"
        )
    headers = [stage.splitlines()[0].strip() for stage in stages]
    if headers != ["${DEBIAN_IMAGE} AS builder", "${DEBIAN_IMAGE} AS runtime"]:
        raise LockError(
            f"{recipe_name} stages must use the locked DEBIAN_IMAGE exactly"
        )
    return stages


def _parse_docker_args(content, recipe_name):
    arguments = []
    for line in content.splitlines():
        stripped = line.strip()
        if not stripped.startswith("ARG "):
            continue
        payload = stripped.removeprefix("ARG ")
        name, separator, value = payload.partition("=")
        if not re.fullmatch(r"[A-Z][A-Z0-9_]*", name) or not name:
            raise LockError(f"{recipe_name} contains a noncanonical ARG: {line!r}")
        arguments.append((name, value if separator else None))
    return arguments


def _expected_docker_args(sources, recipe_name):
    base_images = sources["linux"]["base_images"]
    prefix = "gcc" if recipe_name == "Dockerfile.gcc16" else "p2996"
    selected_images = [
        base_images[f"{prefix}_builder"],
        base_images[f"{prefix}_runtime"],
    ]
    if len(set(selected_images)) != 1:
        raise LockError(f"{recipe_name} requires one base image for both stages")
    snapshot = sources["linux"]["apt"]["snapshot"]
    builder_prelude = [
        ("DEBIAN_IMAGE", selected_images[0]),
        ("BUILDARCH", None),
        ("TARGETARCH", None),
        ("DEBIAN_SNAPSHOT", snapshot),
    ]
    runtime_prelude = [
        ("BUILDARCH", None),
        ("TARGETARCH", None),
        ("DEBIAN_SNAPSHOT", snapshot),
    ]
    if recipe_name == "Dockerfile.gcc16":
        source = sources["gcc"]["source"]
        source_arguments = [
            ("GCC_URL", source["url"]),
            ("GCC_FILE", source["filename"]),
            ("GCC_SHA512", source["sha512"]),
        ]
        for name in ("gmp", "mpfr", "mpc", "isl"):
            record = sources["gcc"]["prerequisites"][name]
            stem = name.upper()
            source_arguments.extend(
                (
                    (f"{stem}_URL", record["url"]),
                    (f"{stem}_FILE", record["filename"]),
                    (f"{stem}_SHA512", record["sha512"]),
                )
            )
    else:
        source_arguments = [
            ("P2996_REPOSITORY", sources["p2996"]["repository"]),
            ("P2996_COMMIT", sources["p2996"]["commit"]),
        ]
    return builder_prelude + source_arguments + runtime_prelude


def _docker_shell_command_count(content, command):
    return len(
        re.findall(
            rf"(?m)(?:^[ \t]*|(?:RUN|&&|;)[ \t]+)"
            rf"(?:/[^ \t;]+/)?{re.escape(command)}(?=[ \t])",
            content,
        )
    )


def _validate_locked_source_consumption(sources, content, recipe_name):
    expected_args = _expected_docker_args(sources, recipe_name)
    actual_args = _parse_docker_args(content, recipe_name)
    if actual_args != expected_args:
        raise LockError(
            f"{recipe_name} ARG inputs do not match the source lock: "
            f"actual={actual_args!r}, expected={expected_args!r}"
        )

    protected = {name for name, _ in expected_args}
    for line in content.splitlines():
        stripped = line.strip()
        if stripped.startswith("ARG ") or stripped.startswith("#"):
            continue
        assignment = re.match(
            r"(?:ENV\s+|export\s+)?([A-Z][A-Z0-9_]*)=", stripped
        )
        if assignment and assignment.group(1) in protected:
            raise LockError(
                f"{recipe_name} reassigns locked input {assignment.group(1)}"
            )

    if recipe_name == "Dockerfile.gcc16":
        expected_fetches = [
            f'fetch "${{{stem}_URL}}" "${{{stem}_FILE}}" '
            f'"${{{stem}_SHA512}}";'
            for stem in ("GCC", "GMP", "MPFR", "MPC", "ISL")
        ]
        actual_fetches = [
            line.strip().removesuffix("\\").rstrip()
            for line in content.splitlines()
            if re.match(r"^\s*fetch\s+", line)
        ]
        if actual_fetches != expected_fetches:
            raise LockError(
                "Dockerfile.gcc16 fetch calls do not consume the locked sources"
            )
        required_fetch_fragments = (
            'curl --fail --location --proto \'=https\' --tlsv1.2',
            '--retry 3 --output "${file}" "${url}";',
            'printf \'%s  %s\\n\' "${digest}" "${file}" '
            '| sha512sum --check --strict -;',
        )
        if any(content.count(fragment) != 1 for fragment in required_fetch_fragments):
            raise LockError(
                "Dockerfile.gcc16 download does not verify the locked URL and SHA512"
            )
        if (
            _docker_shell_command_count(content, "curl") != 1
            or _docker_shell_command_count(content, "wget")
            or _docker_shell_command_count(content, "git")
        ):
            raise LockError("Dockerfile.gcc16 contains an extra source command")
    else:
        expected_git_block = (
            'RUN git init . \\\n'
            '    && git remote add origin "${P2996_REPOSITORY}" \\\n'
            '    && git fetch --depth=1 origin "${P2996_COMMIT}" \\\n'
            '    && git checkout --detach FETCH_HEAD \\\n'
            '    && test "$(git rev-parse HEAD)" = "${P2996_COMMIT}"'
        )
        if content.count(expected_git_block) != 1:
            raise LockError(
                "Dockerfile.p2996 does not fetch and verify the locked commit"
            )
        if (
            _docker_shell_command_count(content, "git") != 4
            or _docker_shell_command_count(content, "curl")
            or _docker_shell_command_count(content, "wget")
        ):
            raise LockError("Dockerfile.p2996 contains an extra source command")


def _validate_native_stages(content, recipe_name, snapshot):
    expected_guard = (
        "RUN set -eu; \\",
        'case "$(uname -m)" in \\',
        "x86_64) native_arch=amd64 ;; \\",
        "aarch64|arm64) native_arch=arm64 ;; \\",
        '*) echo "unsupported native architecture: $(uname -m)" >&2; exit 1 ;; \\',
        "esac; \\",
        'test -n "${BUILDARCH}"; \\',
        'test -n "${TARGETARCH}"; \\',
        'test "${BUILDARCH}" = "${TARGETARCH}"; \\',
        'test "${BUILDARCH}" = "${native_arch}"',
    )
    for index, stage in enumerate(_docker_stages(content, recipe_name)):
        where = f"{recipe_name} stage {index + 1}"
        if (
            stage.count("ARG BUILDARCH") != 1
            or stage.count("ARG TARGETARCH") != 1
        ):
            raise LockError(f"{where} must declare BUILDARCH and TARGETARCH once")
        lines = stage.splitlines()
        run_indexes = [
            line_index
            for line_index, line in enumerate(lines)
            if line.startswith("RUN ")
        ]
        if not run_indexes:
            raise LockError(f"{where} has no native guard")
        first_run_line = run_indexes[0]
        prelude = [
            line.strip()
            for line in lines[1:first_run_line]
            if line.strip() and not line.lstrip().startswith("#")
        ]
        expected_prelude = [
            "ARG BUILDARCH",
            "ARG TARGETARCH",
            f"ARG DEBIAN_SNAPSHOT={snapshot}",
            "ENV DEBIAN_FRONTEND=noninteractive",
        ]
        if prelude != expected_prelude:
            raise LockError(f"{where} has a noncanonical native prelude")
        guard_lines = []
        line_index = first_run_line
        while True:
            line = lines[line_index].strip()
            guard_lines.append(line)
            if not line.endswith("\\"):
                break
            line_index += 1
            if line_index >= len(lines):
                raise LockError(f"{where} has an unterminated native guard")
        if tuple(guard_lines) != expected_guard:
            raise LockError(f"{where} first RUN is not the exact native guard")
        first_run = stage.index(lines[first_run_line])
        guard_end = stage.index(lines[line_index], first_run) + len(lines[line_index])
        if stage.find("ARG BUILDARCH") > first_run or stage.find("ARG TARGETARCH") > first_run:
            raise LockError(f"{where} native architecture args must precede its guard")
        for token in (
            "apt-get",
            "curl ",
            "git fetch",
            "cmake -S",
            "/configure",
            "make -j",
        ):
            token_index = stage.find(token)
            if token_index >= 0 and token_index < guard_end:
                raise LockError(f"{where} performs work before its native guard: {token}")


def _locked_packages_in_stage(stage, where):
    lines = stage.splitlines()
    marker = "apt-get install -y --no-install-recommends"
    install_lines = [
        line
        for line in lines
        if not line.lstrip().startswith("#")
        and re.search(r"\bapt(?:-get)?\b[^\n]*\binstall\b", line)
    ]
    if len(install_lines) not in (1, 2):
        raise LockError(f"{where} must contain one locked install and optional CA bootstrap")
    for install_line in install_lines:
        install_command = install_line.strip()
        if install_command not in (
            f"{marker} \\",
            f"&& {marker} \\",
            f"RUN {marker} \\",
        ):
            raise LockError(f"{where} package install command is not canonical")
    starts = [index for index, line in enumerate(lines) if marker in line]
    if len(starts) not in (1, 2):
        raise LockError(f"{where} must contain one locked install and optional CA bootstrap")
    locked_start = starts[-1]
    package_pattern = re.compile(
        r"[a-z][a-z0-9.+-]*(?::(?:amd64|arm64))?=[^\s'\";\\]+"
    )
    ninja_pattern = re.compile(
        r"(?m)^\s*(amd64|arm64)\)\s+"
        r"ninja_package='([^']+)'\s*;;\s*\\?$"
    )
    ninja_mappings = ninja_pattern.findall(stage)
    if ninja_mappings and stage.count("ninja_package=") != 2:
        raise LockError(f"{where} contains an extra Ninja package assignment")
    packages = []
    used_ninja_mapping = False
    for line in lines[locked_start + 1 :]:
        token = line.strip()
        if token.endswith("\\"):
            token = token[:-1].rstrip()
        command_ends = token.endswith(";")
        if command_ends:
            token = token[:-1].rstrip()
        if token == '"${ninja_package}"':
            if used_ninja_mapping:
                raise LockError(f"{where} repeats the ninja package placeholder")
            if {architecture for architecture, _ in ninja_mappings} != {
                "amd64",
                "arm64",
            } or len(ninja_mappings) != 2:
                raise LockError(f"{where} must map Ninja for amd64 and arm64 once")
            for architecture, package in ninja_mappings:
                if not package.startswith(f"ninja-build:{architecture}="):
                    raise LockError(
                        f"{where} Ninja package does not match {architecture}: {package}"
                    )
                packages.append(package)
            used_ninja_mapping = True
        elif package_pattern.fullmatch(token):
            packages.append(token)
        elif packages and token.startswith(("&& ", "rm ")):
            break
        else:
            raise LockError(f"{where} contains an unparsed apt package token: {token!r}")
        if command_ends:
            break
    if not packages:
        raise LockError(f"{where} apt install command has no locked packages")
    if ninja_mappings and not used_ninja_mapping:
        raise LockError(f"{where} defines but does not install its Ninja mapping")
    return packages


def _validate_apt_stage(stage, sources, where):
    uncommented = "\n".join(
        line for line in stage.splitlines() if not line.lstrip().startswith("#")
    )
    apt_commands = re.findall(
        r"(?<![A-Za-z0-9_.-])(?:apt-get|apt)(?=\s)", uncommented
    )
    if len(apt_commands) != 4:
        raise LockError(
            f"{where} must contain exactly the CA bootstrap and locked apt commands"
        )
    update = "apt-get -o Acquire::Check-Valid-Until=false update"
    install = "apt-get install -y --no-install-recommends"
    if uncommented.count(update) != 2 or uncommented.count(install) != 2:
        raise LockError(f"{where} contains a noncanonical apt command")
    update_lines = [
        line.strip() for line in uncommented.splitlines() if update in line
    ]
    if update_lines != [f"{update}; \\", f"{update}; \\"]:
        raise LockError(f"{where} may not ignore a Debian index refresh failure")
    apt = sources["linux"]["apt"]
    source_options = (
        "check-valid-until=no "
        "signed-by=/usr/share/keyrings/debian-archive-keyring.gpg"
    )
    bootstrap_assignment = (
        'bootstrap_snapshot="http://${DEBIAN_SNAPSHOT#https://}"'
    )
    bootstrap_source_line = (
        f"printf 'deb [{source_options}] %s "
        f"{apt['suites'][0]} {apt['components'][0]}\\n' "
        '"${bootstrap_snapshot}"'
    )
    secure_source_line = (
        f"printf 'deb [{source_options}] %s "
        f"{apt['suites'][0]} {apt['components'][0]}\\n' "
        '"${DEBIAN_SNAPSHOT}"'
    )
    ca_packages = {
        package
        for packages in sources["linux"]["packages"].values()
        for package in packages
        if package.startswith("ca-certificates=")
    }
    if len(ca_packages) != 1:
        raise LockError("linux package locks disagree on ca-certificates")
    bootstrap_install = (
        f"{install} \\\n        {next(iter(ca_packages))};"
    )
    if (
        uncommented.count(bootstrap_assignment) != 1
        or uncommented.count(bootstrap_source_line) != 1
        or uncommented.count(secure_source_line) != 1
        or uncommented.count(bootstrap_install) != 1
    ):
        raise LockError(f"{where} does not use the canonical authenticated CA bootstrap")
    if uncommented.count("${DEBIAN_SNAPSHOT}") != 1:
        raise LockError(f"{where} reuses or bypasses the locked Debian snapshot")
    forbidden = (
        "trusted=yes",
        "AllowInsecureRepositories",
        "AllowUnauthenticated",
        "Verify-Peer=false",
        "Verify-Host=false",
    )
    if any(token in uncommented for token in forbidden):
        raise LockError(f"{where} weakens Debian repository authentication")
    first_update = uncommented.index(update)
    second_update = uncommented.index(update, first_update + len(update))
    first_install = uncommented.index(install)
    second_install = uncommented.index(install, first_install + len(install))
    first_cleanup = uncommented.find(
        "rm -rf /var/lib/apt/lists/*", first_install + len(install)
    )
    if not (
        uncommented.index(bootstrap_assignment)
        < uncommented.index(bootstrap_source_line)
        < first_update
        < first_install
        < first_cleanup
        < uncommented.index(secure_source_line)
        < second_update
        < second_install
    ):
        raise LockError(f"{where} does not refresh indexes after CA bootstrap")


def _continued_command_options(
    content, marker, option_prefix, where, replacements=None
):
    lines = content.splitlines()
    starts = [index for index, line in enumerate(lines) if marker in line]
    if len(starts) != 1:
        raise LockError(f"{where} must contain exactly one {marker!r} command")
    options = []
    index = starts[0]
    command_line = lines[index].strip()
    if command_line not in (f"{marker} \\", f"RUN {marker} \\"):
        raise LockError(f"{where} has an unexpected command line: {command_line!r}")
    while True:
        line = lines[index].strip()
        continued = line.endswith("\\")
        token = line[:-1].rstrip() if continued else line
        if token.startswith(option_prefix):
            token = token.replace('"', "")
            for source, replacement in (replacements or {}).items():
                token = token.replace(source, replacement)
            options.append(token)
        elif index != starts[0]:
            raise LockError(f"{where} has unexpected continued token: {token!r}")
        if not continued:
            break
        index += 1
        if index >= len(lines):
            raise LockError(f"{where} contains an unterminated continued command")
    return options


def _require_exact_options(actual, expected, where, *, ordered=False):
    if len(actual) != len(set(actual)):
        raise LockError(f"{where} contains duplicate options: {actual!r}")
    matches = actual == expected if ordered else set(actual) == set(expected)
    if not matches:
        raise LockError(
            f"{where} does not match locked configuration: "
            f"actual={actual!r}, expected={expected!r}"
        )


def _validate_configure_recipes(sources, gcc_content, p2996_content, mac_content):
    if gcc_content.count("/configure") != 1:
        raise LockError("GCC recipe must contain exactly one configure invocation")
    if p2996_content.count("cmake -S") != 1:
        raise LockError("Linux P2996 recipe must contain exactly one CMake configure")
    if mac_content.count("cmake -S") != 1:
        raise LockError("macOS P2996 recipe must contain exactly one CMake configure")
    gcc_options = _continued_command_options(
        gcc_content,
        "/opt/sources/gcc-16.2.0/configure",
        "--",
        "GCC configure invocation",
    )
    _require_exact_options(
        gcc_options,
        sources["gcc"]["configure_flags"],
        "GCC configure invocation",
        ordered=True,
    )

    common = sources["p2996"]["cmake_flags"]
    platform = sources["p2996"]["platform_cmake_flags"]
    for linux_platform, llvm_target in (
        ("linux/amd64", "X86"),
        ("linux/arm64", "AArch64"),
    ):
        linux_options = _continued_command_options(
            p2996_content,
            "cmake -S llvm -B build -G Ninja",
            "-D",
            f"{linux_platform} P2996 CMake configuration",
            {"${llvm_target}": llvm_target},
        )
        _require_exact_options(
            linux_options,
            common + platform[linux_platform],
            f"{linux_platform} P2996 CMake configuration",
        )
    for mac_platform, architecture, llvm_target in (
        ("macos/arm64", "arm64", "AArch64"),
        ("macos/x86_64", "x86_64", "X86"),
    ):
        mac_options = _continued_command_options(
            mac_content,
            'cmake -S "${source_dir}/llvm" -B "${build_dir}" -G Ninja',
            "-D",
            f"{mac_platform} P2996 CMake configuration",
            {
                "${toolchain_root}": "${TOOLCHAIN_ROOT}",
                "${architecture}": architecture,
                "${sdkroot}": "${SDKROOT}",
                "${deployment_target}": "15.0",
                "${llvm_target}": llvm_target,
            },
        )
        _require_exact_options(
            mac_options,
            common + platform[mac_platform],
            f"{mac_platform} P2996 CMake configuration",
        )
    target_mapping = (
        '    case "${TARGETARCH}" in \\\n'
        '        amd64) llvm_target=X86 ;; \\\n'
        '        arm64) llvm_target=AArch64 ;; \\\n'
        '        *) echo "unsupported TARGETARCH: ${TARGETARCH}" >&2; exit 1 ;; \\\n'
        '    esac; \\\n'
        '    cmake -S llvm -B build -G Ninja \\\n'
    )
    if (
        p2996_content.count(target_mapping) != 1
        or p2996_content.count("llvm_target=") != 2
        or p2996_content.count("${llvm_target}") != 1
    ):
        raise LockError("Linux P2996 target mapping contains an override")


def _validate_bake_recipe(content):
    expected = {
        "gcc16-amd64": (
            '.github/docker/Dockerfile.gcc16',
            "linux/amd64",
        ),
        "gcc16-arm64": (
            '.github/docker/Dockerfile.gcc16',
            "linux/arm64",
        ),
        "p2996-amd64": (
            '.github/docker/Dockerfile.p2996',
            "linux/amd64",
        ),
        "p2996-arm64": (
            '.github/docker/Dockerfile.p2996',
            "linux/arm64",
        ),
    }
    if "qemu" in content.casefold():
        raise LockError("docker-bake.hcl must not enable emulated builds")
    declarations = re.findall(
        r'(?m)^(variable|group|target) "([^"]+)" \{', content
    )
    if declarations != [
        ("variable", "REGISTRY"),
        ("group", "default"),
        ("target", "native"),
        *(('target', target) for target in expected),
    ]:
        raise LockError("docker-bake.hcl contains noncanonical declarations")
    if re.search(r"(?mi)^\s*args\s*=", content):
        raise LockError("docker-bake.hcl must not inject build args")
    for target, (dockerfile, platform) in expected.items():
        matches = re.findall(
            rf'(?ms)^target "{re.escape(target)}" \{{(.*?)^\}}', content
        )
        if len(matches) != 1:
            raise LockError(f"docker-bake.hcl must define {target} exactly once")
        block = matches[0]
        if re.search(r"(?mi)^\s*contexts?\s*=", block):
            raise LockError(
                f"docker-bake.hcl target {target} must not override context"
            )
        required = (
            'inherits   = ["native"]',
            f'dockerfile = "{dockerfile}"',
            f'platforms  = ["{platform}"]',
        )
        if any(token not in block for token in required):
            raise LockError(
                f"docker-bake.hcl target {target} does not lock its native platform"
            )


def _validate_workflow_build_inputs(content):
    for line in content.splitlines():
        if "--set" not in line:
            continue
        if re.search(
            r"\.((?:args)(?:\.|=)|contexts?(?:\.|=)|dockerfile=|platforms?="
            r"|provenance=|sbom=)",
            line,
            re.IGNORECASE,
        ):
            raise LockError(
                "toolchain workflow must not override bake build inputs"
            )


def _validate_recipes(sources, recipe_root):
    root = Path(recipe_root).absolute()
    normalized = {}
    for relative in RECIPE_PATHS:
        data = _secure_recipe_bytes(root, relative)
        normalized[relative] = _normalized_recipe_bytes(data, relative)
        actual = hashlib.sha256(normalized[relative]).hexdigest()
        expected = sources["recipes"][relative]
        if actual != expected:
            raise LockError(
                f"recipe SHA256 mismatch for {relative}: {actual} != {expected}"
            )
    attributes = normalized[".gitattributes"].decode("utf-8")
    if attributes != GIT_ATTRIBUTES:
        raise LockError(".gitattributes does not contain the exact LF policy")

    gcc_path = root / ".github/docker/Dockerfile.gcc16"
    p2996_path = root / ".github/docker/Dockerfile.p2996"
    bake_path = root / ".github/docker/docker-bake.hcl"
    mac_path = root / ".github/scripts/build-p2996-macos.sh"
    gcc_content = normalized[".github/docker/Dockerfile.gcc16"].decode("utf-8")
    p2996_content = normalized[".github/docker/Dockerfile.p2996"].decode("utf-8")
    bake_content = normalized[".github/docker/docker-bake.hcl"].decode("utf-8")
    mac_content = normalized[".github/scripts/build-p2996-macos.sh"].decode("utf-8")
    workflow_content = normalized[".github/workflows/toolchain-images.yml"].decode(
        "utf-8"
    )
    expected_syntax = f"# syntax={sources['linux']['docker']['dockerfile_frontend']}"
    for recipe, content in ((gcc_path, gcc_content), (p2996_path, p2996_content)):
        if not content.startswith(expected_syntax + "\n"):
            raise LockError(
                f"{recipe.name} does not use the locked Dockerfile frontend"
            )
        _validate_locked_source_consumption(sources, content, recipe.name)
        _validate_native_stages(
            content, recipe.name, sources["linux"]["apt"]["snapshot"]
        )
    _validate_bake_recipe(bake_content)
    _validate_workflow_build_inputs(workflow_content)

    stage_sets = {
        "gcc_builder": (gcc_path, _docker_stages(gcc_content, gcc_path.name)[0]),
        "gcc_runtime": (gcc_path, _docker_stages(gcc_content, gcc_path.name)[1]),
        "p2996_builder": (
            p2996_path,
            _docker_stages(p2996_content, p2996_path.name)[0],
        ),
        "p2996_runtime": (
            p2996_path,
            _docker_stages(p2996_content, p2996_path.name)[1],
        ),
    }
    for package_set, packages in sources["linux"]["packages"].items():
        recipe, stage = stage_sets[package_set]
        _validate_apt_stage(
            stage, sources, f"{recipe.name} {package_set}"
        )
        actual_packages = _locked_packages_in_stage(
            stage, f"{recipe.name} {package_set}"
        )
        if (
            len(actual_packages) != len(set(actual_packages))
            or set(actual_packages) != set(packages)
        ):
            raise LockError(
                f"{recipe.name} {package_set} package installation does not match lock: "
                f"actual={actual_packages!r}, expected={packages!r}"
            )
    _validate_configure_recipes(sources, gcc_content, p2996_content, mac_content)
    for relative, reviewed in REVIEWED_RECIPE_SHA256.items():
        actual = hashlib.sha256(normalized[relative]).hexdigest()
        if actual != reviewed:
            raise LockError(
                f"recipe differs from the reviewed semantic recipe for {relative}: "
                f"{actual} != {reviewed}"
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
