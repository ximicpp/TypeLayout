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
import stat
import sys
import tempfile
from urllib.error import HTTPError, URLError
from urllib.parse import urlencode, urljoin, urlsplit
from urllib.request import HTTPRedirectHandler, Request, build_opener


GCC_VERSION = "16.2.0"
CLANG_COMMIT = "060be17654102019e14810c3f948ef85a490755f"
RUNNER_IMAGES_COMMIT = "564e58dbe650c507ccba1171f6159c12f26820c8"
SNAPSHOT = "https://snapshot.debian.org/archive/debian/20260824T000000Z/"
DOCKERFILE_FRONTEND = (
    "docker.io/docker/dockerfile:1.7@sha256:"
    "a57df69d0ea827fb7266491f2813635de6f17269be881f696fbfdf2d83dda33e"
)
RUNNER_INVENTORY_ROOT = (
    "https://raw.githubusercontent.com/actions/runner-images/"
    f"{RUNNER_IMAGES_COMMIT}"
)
UBUNTU_RUNNER_INVENTORY = (
    f"{RUNNER_INVENTORY_ROOT}/images/ubuntu/Ubuntu2404-Readme.md"
)
MACOS_RUNNER_INVENTORIES = {
    "x86_64": f"{RUNNER_INVENTORY_ROOT}/images/macos/macos-15-Readme.md",
    "arm64": f"{RUNNER_INVENTORY_ROOT}/images/macos/macos-15-arm64-Readme.md",
}
MAX_TEXT_BYTES = 4 * 1024 * 1024
MAX_REGISTRY_JSON_BYTES = 2 * 1024 * 1024
MAX_REGISTRY_MANIFEST_BYTES = 16 * 1024 * 1024
MAX_PACKAGES_COMPRESSED_BYTES = 64 * 1024 * 1024
MAX_PACKAGES_DECOMPRESSED_BYTES = 512 * 1024 * 1024
MAX_RECIPE_BYTES = 16 * 1024 * 1024

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
        "binutils",
        "ca-certificates",
        "cmake",
        "git",
        "libc6-dev",
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
        "binutils",
        "ca-certificates",
        "cmake",
        "git",
        "libc6-dev",
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
        source = urlsplit(request.full_url)
        destination = urlsplit(resolved)
        try:
            source_port = source.port
            destination_port = destination.port
        except ValueError as error:
            raise BootstrapError(
                f"redirect contains an invalid port: {request.full_url} -> {resolved}"
            ) from error
        for parsed, port in ((source, source_port), (destination, destination_port)):
            if (
                parsed.scheme.lower() != "https"
                or not parsed.hostname
                or parsed.username is not None
                or parsed.password is not None
                or port not in (None, 443)
            ):
                raise BootstrapError(
                    f"insecure redirect refused: {request.full_url} -> {resolved}"
                )
        if destination.hostname.casefold() != source.hostname.casefold():
            raise BootstrapError(
                f"cross-host redirect refused: {request.full_url} -> {resolved}"
            )
        return super().redirect_request(
            request, file_pointer, code, message, headers, resolved
        )


def _open_same_host(request):
    opener = build_opener(SameHostRedirectHandler())
    try:
        return opener.open(request, timeout=60)
    except (HTTPError, URLError, OSError) as error:
        raise BootstrapError(
            f"cannot fetch reviewed source {request.full_url}: {error}"
        ) from error


def _read_limited(response, maximum, where):
    chunks = []
    total = 0
    while True:
        try:
            chunk = response.read(min(1024 * 1024, maximum - total + 1))
        except OSError as error:
            raise BootstrapError(f"cannot read {where}: {error}") from error
        if not chunk:
            break
        total += len(chunk)
        if total > maximum:
            raise BootstrapError(f"{where} exceeds {maximum} bytes")
        chunks.append(chunk)
    return b"".join(chunks)


def _decode_text(data, where):
    try:
        return data.decode("utf-8")
    except UnicodeDecodeError as error:
        raise BootstrapError(f"{where} is not valid UTF-8") from error


def _decode_json(data, where):
    try:
        value = json.loads(_decode_text(data, where))
    except json.JSONDecodeError as error:
        raise BootstrapError(f"{where} is not valid JSON") from error
    if not isinstance(value, dict):
        raise BootstrapError(f"{where} must contain a JSON object")
    return value


def _require_json_string(value, key, where):
    try:
        result = value[key]
    except (KeyError, TypeError) as error:
        raise BootstrapError(f"{where} is missing {key!r}") from error
    if not isinstance(result, str) or not result:
        raise BootstrapError(f"{where}.{key} must be a nonempty string")
    return result


def fetch_text(url):
    parsed = urlsplit(url)
    if parsed.scheme != "https" or not parsed.hostname:
        raise BootstrapError(f"source URL must be HTTPS: {url}")
    request = Request(url, headers={"User-Agent": "TypeLayout-toolchain-lock/1"})
    with _open_same_host(request) as response:
        return _decode_text(_read_limited(response, MAX_TEXT_BYTES, url), url)


def _fetch_bytes(url, maximum=MAX_PACKAGES_COMPRESSED_BYTES):
    parsed = urlsplit(url)
    if parsed.scheme != "https" or not parsed.hostname:
        raise BootstrapError(f"source URL must be HTTPS: {url}")
    request = Request(url, headers={"User-Agent": "TypeLayout-toolchain-lock/1"})
    with _open_same_host(request) as response:
        return _read_limited(response, maximum, url)


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
        token_response = _read_limited(
            response, MAX_REGISTRY_JSON_BYTES, "Docker registry token response"
        )
        token = _require_json_string(
            _decode_json(token_response, "Docker registry token response"),
            "token",
            "Docker registry token response",
        )
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
        _read_limited(
            response,
            MAX_REGISTRY_MANIFEST_BYTES,
            f"Docker manifest for {reference}",
        )
        digest = response.headers.get("Docker-Content-Digest", "")
    if not re.fullmatch(r"sha256:[0-9a-f]{64}", digest):
        raise BootstrapError(f"registry returned an invalid digest for {reference}")
    return f"docker.io/{repository}@{digest}"


def _parse_package_versions(compressed):
    if len(compressed) > MAX_PACKAGES_COMPRESSED_BYTES:
        raise BootstrapError(
            "Debian Packages.xz exceeds the compressed download limit"
        )
    decompressor = lzma.LZMADecompressor()
    output = []
    output_bytes = 0
    try:
        for offset in range(0, len(compressed), 1024 * 1024):
            chunk = compressed[offset : offset + 1024 * 1024]
            while True:
                remaining = MAX_PACKAGES_DECOMPRESSED_BYTES - output_bytes
                part = decompressor.decompress(chunk, max_length=remaining + 1)
                chunk = b""
                output_bytes += len(part)
                if output_bytes > MAX_PACKAGES_DECOMPRESSED_BYTES:
                    raise BootstrapError(
                        "Debian Packages.xz exceeds the decompressed size limit"
                    )
                output.append(part)
                if decompressor.eof or decompressor.needs_input:
                    break
            if decompressor.eof:
                if decompressor.unused_data or offset + 1024 * 1024 < len(compressed):
                    raise BootstrapError("Debian Packages.xz contains trailing data")
                break
    except lzma.LZMAError as error:
        raise BootstrapError("Debian Packages.xz is not a valid XZ stream") from error
    if not decompressor.eof:
        raise BootstrapError("Debian Packages.xz is truncated")
    text = _decode_text(b"".join(output), "Debian Packages index")
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


def _single_inventory_match(text, pattern, where):
    matches = re.findall(pattern, text, re.MULTILINE)
    if len(matches) != 1:
        raise BootstrapError(f"runner inventory must contain exactly one {where}")
    return matches[0]


def _runner_inventory(fetch):
    ubuntu = fetch(UBUNTU_RUNNER_INVENTORY)
    docker = {
        "buildx_version": _single_inventory_match(
            ubuntu, r"^- Docker-Buildx ([0-9]+(?:\.[0-9]+)+)\s*$", "Docker-Buildx version"
        ),
        "client_version": _single_inventory_match(
            ubuntu, r"^- Docker Client ([0-9]+(?:\.[0-9]+)+)\s*$", "Docker Client version"
        ),
        "server_version": _single_inventory_match(
            ubuntu, r"^- Docker Server ([0-9]+(?:\.[0-9]+)+)\s*$", "Docker Server version"
        ),
    }
    apple = {}
    for architecture, url in MACOS_RUNNER_INVENTORIES.items():
        inventory = fetch(url)
        xcode_version, xcode_build = _single_inventory_match(
            inventory,
            r"^\|\s*([0-9]+(?:\.[0-9]+)+) \(default\)\s*"
            r"\|\s*([0-9A-Z]+)\s*\|.*$",
            f"default Xcode row for {architecture}",
        )
        sdk_rows = re.findall(
            r"^\|\s*macOS ([0-9]+(?:\.[0-9]+)+)\s*\|\s*"
            r"macosx([0-9]+(?:\.[0-9]+)+)\s*\|\s*"
            r"([0-9]+(?:\.[0-9]+)+)\s*\|",
            inventory,
            re.MULTILINE,
        )
        sdk_rows = [row for row in sdk_rows if row[2] == xcode_version]
        if len(sdk_rows) != 1 or sdk_rows[0][0] != sdk_rows[0][1]:
            raise BootstrapError(
                f"runner inventory must bind one macOS SDK to Xcode for {architecture}"
            )
        sdk_version = sdk_rows[0][0]
        # runner-images publishes the Xcode build and SDK version, while the
        # SDK build is an immutable property verified from that exact Xcode at
        # build and verification time.
        sdk_builds = {("16.4", "16F6", "15.5"): "24F74"}
        try:
            sdk_build = sdk_builds[(xcode_version, xcode_build, sdk_version)]
        except KeyError as error:
            raise BootstrapError(
                "runner inventory selected an unreviewed Xcode/SDK combination"
            ) from error
        apple[architecture] = {
            "xcode_version": xcode_version,
            "xcode_build": xcode_build,
            "sdk_version": sdk_version,
            "sdk_build": sdk_build,
        }
    if apple["x86_64"] != apple["arm64"]:
        raise BootstrapError("macOS runner inventories disagree on Xcode/SDK identity")
    return docker, apple["arm64"]


def _secure_recipe_bytes(root, relative):
    root = Path(root).absolute()
    relative_path = Path(relative)
    if relative_path.is_absolute() or any(part in ("", ".", "..") for part in relative_path.parts):
        raise BootstrapError(f"recipe has an unsafe path: {relative}")
    current = root
    components = (root,) + tuple(
        root / Path(*relative_path.parts[:index])
        for index in range(1, len(relative_path.parts) + 1)
    )
    final_stat = None
    for index, component in enumerate(components):
        try:
            status = os.lstat(component)
        except OSError as error:
            raise BootstrapError(f"recipe is missing: {relative}") from error
        if stat.S_ISLNK(status.st_mode):
            raise BootstrapError(f"recipe path contains a symbolic link: {relative}")
        if index < len(components) - 1 and not stat.S_ISDIR(status.st_mode):
            raise BootstrapError(f"recipe path component is not a directory: {relative}")
        final_stat = status
        current = component
    if final_stat is None or not stat.S_ISREG(final_stat.st_mode):
        raise BootstrapError(f"recipe is not a regular file: {relative}")
    flags = os.O_RDONLY | getattr(os, "O_BINARY", 0) | getattr(os, "O_NOFOLLOW", 0)
    try:
        descriptor = os.open(current, flags)
    except OSError as error:
        raise BootstrapError(f"cannot open recipe safely: {relative}") from error
    try:
        opened_stat = os.fstat(descriptor)
        if not stat.S_ISREG(opened_stat.st_mode) or (
            opened_stat.st_dev,
            opened_stat.st_ino,
        ) != (final_stat.st_dev, final_stat.st_ino):
            raise BootstrapError(f"recipe changed during secure open: {relative}")
        chunks = []
        total = 0
        while True:
            chunk = os.read(descriptor, 1024 * 1024)
            if not chunk:
                break
            total += len(chunk)
            if total > MAX_RECIPE_BYTES:
                raise BootstrapError(f"recipe exceeds {MAX_RECIPE_BYTES} bytes: {relative}")
            chunks.append(chunk)
        return b"".join(chunks)
    finally:
        os.close(descriptor)


def _normalized_recipe_sha256(root, relative):
    data = _secure_recipe_bytes(root, relative).replace(b"\r\n", b"\n")
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

    runner_docker, runner_apple = _runner_inventory(fetch_text)

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
        + " -nostdinc++ "
        "-isystem ${TOOLCHAIN_ROOT}/include/${TARGET_TRIPLE}/c++/v1 "
        "-isystem ${TOOLCHAIN_ROOT}/include/c++/v1 "
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
                "--prefix=/opt/gcc-16.2.0",
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
            ],
            "platform_cmake_flags": {
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
            },
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
                        "client_version": runner_docker["client_version"],
                        "server_version": runner_docker["server_version"],
                    },
                    "ubuntu-24.04-arm": {
                        "client_version": runner_docker["client_version"],
                        "server_version": runner_docker["server_version"],
                    },
                },
                "buildx_version": runner_docker["buildx_version"],
                "buildkit_image": buildkit,
                "dockerfile_frontend": DOCKERFILE_FRONTEND,
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
                    **runner_apple,
                    "deployment_target": "15.0",
                },
                "x86_64_macos_clang": {
                    "runner": "macos-15-intel",
                    "architecture": "x86_64",
                    "llvm_target": "X86",
                    "flags": mac_flags,
                    **runner_apple,
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
