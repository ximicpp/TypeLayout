#!/usr/bin/env python3
"""Resolve reviewed public inputs into the immutable toolchain source lock."""

import argparse
from functools import lru_cache
import hashlib
import json
import lzma
import os
from pathlib import Path
import re
import sys
import tempfile
from urllib.error import HTTPError, URLError
from urllib.parse import urlencode, urljoin, urlsplit
from urllib.request import HTTPRedirectHandler, Request, build_opener


GCC_VERSION = "16.2.0"
CLANG_COMMIT = "060be17654102019e14810c3f948ef85a490755f"
RUNNER_IMAGES_COMMIT = "564e58dbe650c507ccba1171f6159c12f26820c8"
SNAPSHOT = "https://snapshot.debian.org/archive/debian/20260824T000000Z/"

PREREQUISITE_FILES = {
    "gmp": ("6.3.0", "gmp-6.3.0.tar.bz2"),
    "mpfr": ("4.2.2", "mpfr-4.2.2.tar.bz2"),
    "mpc": ("1.3.1", "mpc-1.3.1.tar.gz"),
    "isl": ("0.24", "isl-0.24.tar.bz2"),
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

RECIPE_PATHS = (
    ".gitattributes",
    ".github/docker/Dockerfile.gcc16",
    ".github/docker/Dockerfile.p2996",
    ".github/docker/docker-bake.hcl",
    ".github/scripts/build-p2996-macos.sh",
    ".github/scripts/verify-p2996-toolchain.sh",
    ".github/workflows/toolchain-images.yml",
)

PACKAGE_NAMES = {
    "gcc_builder": (
        "build-essential",
        "bison",
        "ca-certificates",
        "curl",
        "flex",
        "mawk",
        "gzip",
        "make",
        "patch",
        "tar",
        "xz-utils",
        "bzip2",
        "zlib1g-dev",
        "libzstd-dev",
    ),
    "gcc_runtime": (
        "ca-certificates",
        "cmake",
        "git",
        "ninja-build",
        "python3",
        "zlib1g",
        "libzstd1",
    ),
    "p2996_builder": (
        "build-essential",
        "ca-certificates",
        "cmake",
        "curl",
        "git",
        "libncurses-dev",
        "libxml2-dev",
        "libzstd-dev",
        "ninja-build",
        "python3",
        "tar",
        "zlib1g-dev",
        "zstd",
    ),
    "p2996_runtime": (
        "ca-certificates",
        "cmake",
        "git",
        "libtinfo6",
        "libxml2",
        "libzstd1",
        "ninja-build",
        "python3",
        "zlib1g",
        "zstd",
    ),
}


class BootstrapError(RuntimeError):
    pass


class SameHostRedirectHandler(HTTPRedirectHandler):
    def redirect_request(self, request, file_pointer, code, message, headers, new_url):
        resolved = urljoin(request.full_url, new_url)
        if urlsplit(resolved).hostname != urlsplit(request.full_url).hostname:
            raise BootstrapError(
                f"cross-host redirect refused: {request.full_url} -> {resolved}"
            )
        return super().redirect_request(
            request, file_pointer, code, message, headers, resolved
        )


def _open_same_host(request):
    opener = build_opener(SameHostRedirectHandler())
    return opener.open(request, timeout=60)


def fetch_text(url):
    parsed = urlsplit(url)
    if parsed.scheme != "https" or not parsed.hostname:
        raise BootstrapError(f"source URL must be HTTPS: {url}")
    request = Request(url, headers={"User-Agent": "TypeLayout-toolchain-lock/1"})
    with _open_same_host(request) as response:
        return response.read().decode("utf-8")


def _fetch_bytes(url):
    parsed = urlsplit(url)
    if parsed.scheme != "https" or not parsed.hostname:
        raise BootstrapError(f"source URL must be HTTPS: {url}")
    request = Request(url, headers={"User-Agent": "TypeLayout-toolchain-lock/1"})
    with _open_same_host(request) as response:
        return response.read()


def _checksum_for(text, filename, algorithm):
    width = 128 if algorithm == "sha512" else 64
    pattern = re.compile(
        rf"^([0-9a-f]{{{width}}})\s+\*?{re.escape(filename)}$", re.MULTILINE
    )
    matches = pattern.findall(text)
    if len(matches) != 1:
        raise BootstrapError(
            f"expected exactly one {algorithm} checksum for {filename}"
        )
    return matches[0]


def resolve_image(reference):
    """Resolve one reviewed Docker Hub tag to a digest-qualified reference."""
    if not reference.startswith("docker.io/") or "@" in reference:
        raise BootstrapError(f"unsupported image input: {reference}")
    repository_and_tag = reference[len("docker.io/") :]
    repository, separator, tag = repository_and_tag.rpartition(":")
    if not separator or not repository or not tag:
        raise BootstrapError(f"image input must contain one tag: {reference}")
    token_query = urlencode(
        {
            "service": "registry.docker.io",
            "scope": f"repository:{repository}:pull",
        }
    )
    with _open_same_host(
        Request(
            f"https://auth.docker.io/token?{token_query}",
            headers={"User-Agent": "TypeLayout-toolchain-lock/1"},
        )
    ) as response:
        token = json.loads(response.read().decode("utf-8"))["token"]
    request = Request(
        f"https://registry-1.docker.io/v2/{repository}/manifests/{tag}",
        headers={
            "Authorization": f"Bearer {token}",
            "Accept": (
                "application/vnd.oci.image.index.v1+json, "
                "application/vnd.docker.distribution.manifest.list.v2+json, "
                "application/vnd.oci.image.manifest.v1+json"
            ),
            "User-Agent": "TypeLayout-toolchain-lock/1",
        },
    )
    with _open_same_host(request) as response:
        response.read()
        digest = response.headers.get("Docker-Content-Digest", "")
    if not re.fullmatch(r"sha256:[0-9a-f]{64}", digest):
        raise BootstrapError(f"registry returned an invalid digest for {reference}")
    return f"docker.io/{repository}@{digest}"


def _parse_package_versions(compressed):
    text = lzma.decompress(compressed).decode("utf-8")
    versions = {}
    for paragraph in text.split("\n\n"):
        fields = {}
        for line in paragraph.splitlines():
            if line.startswith((" ", "\t")) or ": " not in line:
                continue
            key, value = line.split(": ", 1)
            fields[key] = value
        if "Package" in fields and "Version" in fields:
            versions.setdefault(fields["Package"], fields["Version"])
    return versions


@lru_cache(maxsize=2)
def _snapshot_packages(architecture):
    url = f"{SNAPSHOT}dists/trixie/main/binary-{architecture}/Packages.xz"
    return _parse_package_versions(_fetch_bytes(url))


def resolve_packages(package_names):
    requested = tuple(dict.fromkeys(package_names))
    per_architecture = [
        _snapshot_packages(architecture) for architecture in ("amd64", "arm64")
    ]
    result = []
    for package in requested:
        versions = [mapping.get(package) for mapping in per_architecture]
        if None in versions:
            raise BootstrapError(f"package is missing from snapshot: {package}")
        if versions[0] == versions[1]:
            result.append(f"{package}={versions[0]}")
        else:
            result.append(f"{package}:amd64={versions[0]}")
            result.append(f"{package}:arm64={versions[1]}")
    return result


def _normalized_recipe_sha256(root, relative):
    root = Path(root).resolve()
    path = (root / relative).resolve()
    try:
        path.relative_to(root)
    except ValueError as error:
        raise BootstrapError(f"recipe escapes root: {relative}") from error
    if not path.is_file():
        raise BootstrapError(f"recipe is missing: {relative}")
    data = path.read_bytes().replace(b"\r\n", b"\n")
    if b"\r" in data:
        raise BootstrapError(f"recipe contains a non-CRLF carriage return: {relative}")
    return hashlib.sha256(data).hexdigest()


def build_source_lock(
    *,
    recipe_root,
    gcc_version,
    clang_commit,
    fetch_text=fetch_text,
    resolve_image=resolve_image,
    resolve_packages=resolve_packages,
):
    if gcc_version != GCC_VERSION:
        raise BootstrapError(f"reviewed GCC version is exactly {GCC_VERSION}")
    if clang_commit != CLANG_COMMIT:
        raise BootstrapError(f"reviewed Bloomberg commit is exactly {CLANG_COMMIT}")

    gcc_filename = f"gcc-{GCC_VERSION}.tar.xz"
    gcc_root = f"https://gcc.gnu.org/pub/gcc/releases/gcc-{GCC_VERSION}"
    gcc_sums = fetch_text(f"{gcc_root}/sha512.sum")
    gcc_digest = _checksum_for(gcc_sums, gcc_filename, "sha512")
    prerequisites_url = (
        "https://raw.githubusercontent.com/gcc-mirror/gcc/"
        f"releases/gcc-{GCC_VERSION}/contrib/prerequisites.sha512"
    )
    prerequisite_sums = fetch_text(prerequisites_url)
    prerequisites = {}
    for name, (version, filename) in PREREQUISITE_FILES.items():
        prerequisites[name] = {
            "version": version,
            "url": f"https://gcc.gnu.org/pub/gcc/infrastructure/{filename}",
            "filename": filename,
            "sha512": _checksum_for(prerequisite_sums, filename, "sha512"),
        }

    base = resolve_image("docker.io/library/debian:13-slim")
    buildkit = resolve_image("docker.io/moby/buildkit:buildx-stable-1")
    packages = {
        key: resolve_packages(names) for key, names in PACKAGE_NAMES.items()
    }
    linux_flags = "-std=c++26 -freflection -O3 -fstrict-aliasing"
    clang_core_flags = (
        "-std=c++26 -freflection -freflection-latest -stdlib=libc++ "
        "-O3 -fstrict-aliasing"
    )
    clang_flags = (
        clang_core_flags
        + " -nostdinc++ -isystem ${TOOLCHAIN_ROOT}/include/c++/v1 "
        "-L ${TOOLCHAIN_ROOT}/lib/${TARGET_TRIPLE} "
        "-Wl,-rpath,${TOOLCHAIN_ROOT}/lib/${TARGET_TRIPLE}"
    )
    mac_flags = (
        clang_core_flags
        + " -nostdinc++ -isystem ${TOOLCHAIN_ROOT}/include/c++/v1 "
        "-isysroot ${SDKROOT} -mmacosx-version-min=15.0 "
        "-L ${TOOLCHAIN_ROOT}/lib -Wl,-rpath,${TOOLCHAIN_ROOT}/lib"
    )
    lock = {
        "schema": 1,
        "gcc": {
            "version": GCC_VERSION,
            "compiler_family": "gcc",
            "compiler_revision": GCC_VERSION,
            "flags": linux_flags,
            "source": {
                "url": f"{gcc_root}/{gcc_filename}",
                "filename": gcc_filename,
                "sha512": gcc_digest,
            },
            "prerequisites": prerequisites,
            "configure_flags": [
                "--enable-languages=c,c++",
                "--disable-bootstrap",
                "--disable-multilib",
                "--disable-nls",
                "--enable-checking=release",
            ],
        },
        "p2996": {
            "repository": "https://github.com/bloomberg/clang-p2996.git",
            "commit": CLANG_COMMIT,
            "compiler_family": "clang",
            "compiler_revision": CLANG_COMMIT,
            "flags": clang_flags,
            "projects": ["clang"],
            "runtimes": ["libcxx", "libcxxabi", "libunwind"],
            "llvm_targets": ["X86", "AArch64"],
            "cmake_flags": [
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
            ],
        },
        "linux": {
            "platforms": {
                "linux/amd64": {
                    "architecture": "x86_64",
                    "runner": "ubuntu-24.04",
                },
                "linux/arm64": {
                    "architecture": "arm64",
                    "runner": "ubuntu-24.04-arm",
                },
            },
            "base_images": {
                "gcc_builder": base,
                "gcc_runtime": base,
                "p2996_builder": base,
                "p2996_runtime": base,
            },
            "apt": {
                "snapshot": SNAPSHOT,
                "suites": ["trixie"],
                "components": ["main"],
            },
            "packages": packages,
            "docker": {
                "runner_images_commit": RUNNER_IMAGES_COMMIT,
                "runners": {
                    "ubuntu-24.04": {
                        "client_version": "28.0.4",
                        "server_version": "28.0.4",
                    },
                    "ubuntu-24.04-arm": {
                        "client_version": "28.0.4",
                        "server_version": "28.0.4",
                    },
                },
                "buildx_version": "0.36.1",
                "buildkit_image": buildkit,
            },
        },
        "macos": {
            "runner_images_repository": (
                "https://github.com/actions/runner-images.git"
            ),
            "runner_images_commit": RUNNER_IMAGES_COMMIT,
            "nodes": {
                "arm64_macos_clang": {
                    "runner": "macos-15",
                    "architecture": "arm64",
                    "llvm_target": "AArch64",
                    "flags": mac_flags,
                    "xcode_version": "16.4",
                    "xcode_build": "16F6",
                    "sdk_version": "15.5",
                    "sdk_build": "24F74",
                    "deployment_target": "15.0",
                },
                "x86_64_macos_clang": {
                    "runner": "macos-15-intel",
                    "architecture": "x86_64",
                    "llvm_target": "X86",
                    "flags": mac_flags,
                    "xcode_version": "16.4",
                    "xcode_build": "16F6",
                    "sdk_version": "15.5",
                    "sdk_build": "24F74",
                    "deployment_target": "15.0",
                },
            },
        },
        "actions": dict(ACTION_PINS),
        "recipes": {
            relative: _normalized_recipe_sha256(recipe_root, relative)
            for relative in RECIPE_PATHS
        },
    }
    return lock


def _atomic_write_json(path, value):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary_name = tempfile.mkstemp(
        prefix=f".{path.name}.", suffix=".tmp", dir=path.parent
    )
    temporary = Path(temporary_name)
    try:
        with os.fdopen(descriptor, "w", encoding="utf-8", newline="\n") as output:
            json.dump(value, output, indent=2, sort_keys=True)
            output.write("\n")
            output.flush()
            os.fsync(output.fileno())
        os.replace(temporary, path)
    except BaseException:
        temporary.unlink(missing_ok=True)
        raise


def parse_arguments(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("--gcc-version", required=True)
    parser.add_argument("--clang-commit", required=True)
    parser.add_argument("--recipe-root", required=True)
    parser.add_argument("--output", required=True)
    return parser.parse_args(argv)


def main(argv=None):
    arguments = parse_arguments(argv)
    try:
        lock = build_source_lock(
            recipe_root=arguments.recipe_root,
            gcc_version=arguments.gcc_version,
            clang_commit=arguments.clang_commit,
        )
        _atomic_write_json(arguments.output, lock)
        print(
            f"WROTE SOURCE LOCK gcc={GCC_VERSION} clang={CLANG_COMMIT} "
            f"recipes={len(lock['recipes'])} output={arguments.output}"
        )
        return 0
    except (BootstrapError, HTTPError, URLError, OSError, UnicodeError) as error:
        print(f"toolchain source bootstrap error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
