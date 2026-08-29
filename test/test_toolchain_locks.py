import hashlib
import importlib.util
import io
import json
import lzma
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tarfile
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
VALIDATOR = ROOT / ".github/scripts/validate-toolchain-locks.py"
BOOTSTRAP = ROOT / ".github/scripts/bootstrap-toolchain-sources.py"
SOURCE_LOCK = ROOT / ".github/docker/toolchain-sources.lock"

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

UBUNTU_RUNNER_INVENTORY = """# Ubuntu 24.04
- Docker-Buildx 0.36.1
- Docker Client 28.0.4
- Docker Server 28.0.4
"""
MACOS_RUNNER_INVENTORY = """# macOS 15
| Version        | Build    | Path                           | Symlinks |
| 16.4 (default) | 16F6     | /Applications/Xcode_16.4.app   | /Applications/Xcode.app |
| macOS 15.5     | macosx15.5 | 16.4 |
"""

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
    ".github/scripts/macos-runtime-origin-probe.cpp",
    ".github/scripts/verify-p2996-toolchain.sh",
    ".github/workflows/toolchain-images.yml",
)


def normalized_sha256(path):
    data = path.read_bytes().replace(b"\r\n", b"\n")
    return hashlib.sha256(data).hexdigest()


def write_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )


def bash_command(script, *arguments):
    if os.name != "nt":
        return ["bash", str(script), *arguments]
    resolved = Path(script).resolve()
    drive = resolved.drive[0].lower()
    relative = resolved.as_posix().split(":", 1)[1]
    return ["wsl", "bash", f"/mnt/{drive}{relative}", *arguments]


def bash_syntax_command(script):
    command = bash_command(script)
    return command[:-1] + ["-n", command[-1]]


def bash_path(path):
    resolved = Path(path).resolve()
    if os.name != "nt":
        return str(resolved)
    drive = resolved.drive[0].lower()
    relative = resolved.as_posix().split(":", 1)[1]
    return f"/mnt/{drive}{relative}"


class ToolchainLockTests(unittest.TestCase):
    maxDiff = None

    @staticmethod
    def canonical_apt_bootstrap_stage():
        return """builder
RUN set -eu; \\
    bootstrap_snapshot="http://${DEBIAN_SNAPSHOT#https://}"; \\
    printf 'deb [check-valid-until=no signed-by=/usr/share/keyrings/debian-archive-keyring.gpg] %s trixie main\\n' "${bootstrap_snapshot}" \\
        > /etc/apt/sources.list; \\
    rm -f /etc/apt/sources.list.d/debian.sources; \\
    apt-get -o Acquire::Check-Valid-Until=false update; \\
    apt-get install -y --no-install-recommends \\
        ca-certificates=20250419; \\
    rm -rf /var/lib/apt/lists/*; \\
    printf 'deb [check-valid-until=no signed-by=/usr/share/keyrings/debian-archive-keyring.gpg] %s trixie main\\n' "${DEBIAN_SNAPSHOT}" \\
        > /etc/apt/sources.list; \\
    apt-get -o Acquire::Check-Valid-Until=false update; \\
    apt-get install -y --no-install-recommends \\
        build-essential=12.12 \\
        ca-certificates=20250419; \\
    rm -rf /var/lib/apt/lists/*
"""

    def setUp(self):
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary_directory.name)
        for relative in RECIPE_PATHS:
            path = self.root / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            if relative == ".gitattributes":
                path.write_text(
                    ".github/docker/** text eol=lf\n"
                    ".github/scripts/** text eol=lf\n"
                    ".github/workflows/** text eol=lf\n"
                    "tools/*.py text eol=lf\n"
                    "tools/*.sh text eol=lf\n",
                    encoding="utf-8",
                )
            else:
                path.write_bytes((ROOT / relative).read_bytes())

    def tearDown(self):
        self.temporary_directory.cleanup()

    def make_sources(self):
        prerequisite_values = {
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
        prerequisites = {
            key: {
                "version": version,
                "url": f"https://gcc.gnu.org/pub/gcc/infrastructure/{filename}",
                "filename": filename,
                "sha512": digest,
            }
            for key, (version, filename, digest) in prerequisite_values.items()
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
        return {
            "schema": 1,
            "gcc": {
                "version": "16.2.0",
                "compiler_family": "gcc",
                "compiler_revision": "16.2.0",
                "flags": linux_flags,
                "source": {
                    "url": (
                        "https://gcc.gnu.org/pub/gcc/releases/gcc-16.2.0/"
                        "gcc-16.2.0.tar.xz"
                    ),
                    "filename": "gcc-16.2.0.tar.xz",
                    "sha512": GCC_SHA512,
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
                    key: DEBIAN_IMAGE
                    for key in (
                        "gcc_builder",
                        "gcc_runtime",
                        "p2996_builder",
                        "p2996_runtime",
                    )
                },
                "apt": {
                    "snapshot": (
                        "https://snapshot.debian.org/archive/debian/"
                        "20260824T000000Z/"
                    ),
                    "suites": ["trixie"],
                    "components": ["main"],
                },
                "packages": {
                    key: list(values) for key, values in PACKAGE_LOCKS.items()
                },
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
                    "buildkit_image": BUILDKIT_IMAGE,
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
                relative: normalized_sha256(self.root / relative)
                for relative in RECIPE_PATHS
            },
        }

    def make_outputs(self, sources_path):
        sources = json.loads(sources_path.read_text(encoding="utf-8"))
        sources_digest = hashlib.sha256(sources_path.read_bytes()).hexdigest()
        release = (
            "https://github.com/ximicpp/TypeLayout/releases/download/"
            f"typelayout-toolchains-{sources_digest}"
        )
        return {
            "schema": 1,
            "sources_sha256": sources_digest,
            "source_sha": "a" * 40,
            "workflow_run": "1234.1",
            "linux": {
                "gcc": {
                    "repository": "ghcr.io/ximicpp/typelayout-gcc16",
                    "index_digest": "sha256:" + "1" * 64,
                    "compiler_revision": "16.2.0",
                    "compiler_version": "16.2.0",
                    "stdlib": "libstdc++-20260807",
                    "platforms": {
                        "linux/amd64": {
                            "manifest_digest": "sha256:" + "2" * 64,
                            "target": "x86_64-unknown-linux-gnu",
                        },
                        "linux/arm64": {
                            "manifest_digest": "sha256:" + "3" * 64,
                            "target": "aarch64-unknown-linux-gnu",
                        },
                    },
                },
                "p2996": {
                    "repository": "ghcr.io/ximicpp/typelayout-p2996",
                    "index_digest": "sha256:" + "4" * 64,
                    "compiler_revision": CLANG_COMMIT,
                    "compiler_version": "clang version 21.0.0",
                    "stdlib": "libc++-210000",
                    "platforms": {
                        "linux/amd64": {
                            "manifest_digest": "sha256:" + "5" * 64,
                            "target": "x86_64-unknown-linux-gnu",
                        },
                        "linux/arm64": {
                            "manifest_digest": "sha256:" + "6" * 64,
                            "target": "aarch64-unknown-linux-gnu",
                        },
                    },
                },
            },
            "macos": {
                node: {
                    "url": (
                        f"{release}/p2996-macos-{record['architecture']}-"
                        f"{CLANG_COMMIT}.tar.zst"
                    ),
                    "archive_sha256": digest * 64,
                    "compiler_revision": CLANG_COMMIT,
                    "compiler_version": "clang version 21.0.0",
                    "target": (
                        f"{record['architecture']}-apple-macosx15.0.0"
                    ),
                    "stdlib": "libc++-210000",
                    **{
                        key: record[key]
                        for key in (
                            "xcode_version",
                            "xcode_build",
                            "sdk_version",
                            "sdk_build",
                            "deployment_target",
                        )
                    },
                    "observed_runner": {
                        "image_os": "macos15",
                        "image_version": "20260824.1",
                    },
                }
                for (node, record), digest in zip(
                    sources["macos"]["nodes"].items(), ("7", "8")
                )
            },
        }

    def run_validator(self, sources, outputs=None, *extra):
        command = [
            sys.executable,
            str(VALIDATOR),
            "--sources",
            str(sources),
            "--recipe-root",
            str(self.root),
        ]
        if outputs is not None:
            command.extend(("--outputs", str(outputs)))
        command.extend(extra)
        return subprocess.run(command, capture_output=True, text=True, check=False)

    def rebase_recipe(self, relative, transform):
        recipe = self.root / relative
        original = recipe.read_text(encoding="utf-8")
        changed = transform(original)
        self.assertNotEqual(changed, original, f"mutation did not change {relative}")
        recipe.write_text(changed, encoding="utf-8")
        sources_value = self.make_sources()
        sources_value["recipes"][relative] = normalized_sha256(recipe)
        sources = self.root / "toolchain-sources.lock"
        write_json(sources, sources_value)
        return self.run_validator(sources)

    @staticmethod
    def load_script(path, name):
        specification = importlib.util.spec_from_file_location(name, path)
        if specification is None or specification.loader is None:
            raise AssertionError(f"cannot load {path}")
        module = importlib.util.module_from_spec(specification)
        specification.loader.exec_module(module)
        return module

    def test_complete_source_lock_is_accepted(self):
        sources = self.root / "toolchain-sources.lock"
        write_json(sources, self.make_sources())

        completed = self.run_validator(sources)

        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertEqual(
            completed.stdout.strip(),
            "SOURCE LOCK PASS gcc=16.2.0 "
            f"clang={CLANG_COMMIT} recipes=8",
        )

    def test_nested_unknown_source_field_is_rejected(self):
        sources = self.make_sources()
        sources["macos"]["nodes"]["arm64_macos_clang"]["unknown"] = True
        path = self.root / "toolchain-sources.lock"
        write_json(path, sources)

        completed = self.run_validator(path)

        self.assertNotEqual(completed.returncode, 0)
        self.assertIn("keys", completed.stderr)

    def test_nested_unknown_output_field_is_rejected(self):
        sources = self.root / "toolchain-sources.lock"
        write_json(sources, self.make_sources())
        outputs_value = self.make_outputs(sources)
        outputs_value["linux"]["gcc"]["platforms"]["linux/amd64"][
            "unknown"
        ] = True
        outputs = self.root / "toolchains.lock"
        write_json(outputs, outputs_value)

        completed = self.run_validator(sources, outputs)

        self.assertNotEqual(completed.returncode, 0)
        self.assertIn("keys", completed.stderr)

    def test_unreviewed_base_and_buildkit_digests_are_rejected(self):
        for field_path in (
            ("base_images", "gcc_builder"),
            ("docker", "buildkit_image"),
        ):
            with self.subTest(field_path=field_path):
                sources_value = self.make_sources()
                sources_value["linux"][field_path[0]][field_path[1]] = (
                    "docker.io/library/debian@sha256:" + "f" * 64
                )
                sources = self.root / "toolchain-sources.lock"
                write_json(sources, sources_value)

                completed = self.run_validator(sources)

                self.assertNotEqual(completed.returncode, 0)

    def test_recipe_hash_uses_lf_normalization_but_detects_content_change(self):
        sources = self.root / "toolchain-sources.lock"
        write_json(sources, self.make_sources())
        dockerfile = self.root / ".github/docker/Dockerfile.gcc16"
        lf_bytes = dockerfile.read_bytes().replace(b"\r\n", b"\n")
        dockerfile.write_bytes(lf_bytes.replace(b"\n", b"\r\n"))
        self.assertEqual(self.run_validator(sources).returncode, 0)

        dockerfile.write_bytes(dockerfile.read_bytes() + b"changed\r\n")
        completed = self.run_validator(sources)
        self.assertNotEqual(completed.returncode, 0)
        self.assertIn("recipe", completed.stderr)

    def test_recipe_paths_reject_final_and_intermediate_symlinks(self):
        relative = ".github/docker/Dockerfile.gcc16"
        recipe = self.root / relative
        external = self.root / "external-dockerfile"
        external.write_bytes(recipe.read_bytes())
        recipe.unlink()
        os.symlink(external, recipe)
        sources = self.root / "toolchain-sources.lock"
        write_json(sources, self.make_sources())
        completed = self.run_validator(sources)
        self.assertNotEqual(completed.returncode, 0)
        self.assertIn("symbolic link", completed.stderr)

        bootstrap = self.load_script(BOOTSTRAP, "toolchain_recipe_path_test")
        with self.assertRaisesRegex(bootstrap.BootstrapError, "symbolic link"):
            bootstrap._normalized_recipe_sha256(self.root, relative)

        recipe.unlink()
        recipe.write_bytes(external.read_bytes())
        docker_directory = self.root / ".github/docker"
        real_directory = self.root / ".github/real-docker"
        docker_directory.rename(real_directory)
        os.symlink(real_directory, docker_directory, target_is_directory=True)
        sources_value = self.make_sources()
        sources_value["recipes"] = {
            path: normalized_sha256(self.root / path) for path in RECIPE_PATHS
        }
        write_json(sources, sources_value)
        completed = self.run_validator(sources)
        self.assertNotEqual(completed.returncode, 0)
        self.assertIn("symbolic link", completed.stderr)
        with self.assertRaisesRegex(bootstrap.BootstrapError, "symbolic link"):
            bootstrap._normalized_recipe_sha256(self.root, relative)

    def test_locked_package_version_drift_is_rejected(self):
        sources = self.make_sources()
        sources["linux"]["packages"]["gcc_builder"][0] = (
            "build-essential=unreviewed"
        )
        path = self.root / "toolchain-sources.lock"
        write_json(path, sources)

        completed = self.run_validator(path)

        self.assertNotEqual(completed.returncode, 0)
        self.assertIn("linux.packages", completed.stderr)

    def test_recipe_package_drift_is_rejected_even_with_rebased_hash(self):
        sources = self.make_sources()
        dockerfile = self.root / ".github/docker/Dockerfile.gcc16"
        dockerfile.write_text(
            dockerfile.read_text(encoding="utf-8").replace(
                "build-essential=12.12", "build-essential=unreviewed"
            ),
            encoding="utf-8",
        )
        sources["recipes"][".github/docker/Dockerfile.gcc16"] = (
            normalized_sha256(dockerfile)
        )
        path = self.root / "toolchain-sources.lock"
        write_json(path, sources)

        completed = self.run_validator(path)

        self.assertNotEqual(completed.returncode, 0)
        self.assertIn("Dockerfile.gcc16", completed.stderr)

    def test_apt_validator_accepts_authenticated_ca_bootstrap_then_https(self):
        validator = self.load_script(VALIDATOR, "toolchain_apt_bootstrap_test")
        sources = self.make_sources()
        stage = self.canonical_apt_bootstrap_stage()

        validator._validate_apt_stage(stage, sources, "fixture builder")
        self.assertEqual(
            validator._locked_packages_in_stage(stage, "fixture builder"),
            ["build-essential=12.12", "ca-certificates=20250419"],
        )

    def test_apt_validator_rejects_ignored_https_refresh_failure(self):
        validator = self.load_script(
            VALIDATOR, "toolchain_apt_refresh_failure_test"
        )
        sources = self.make_sources()
        update = "apt-get -o Acquire::Check-Valid-Until=false update"
        prefix, suffix = self.canonical_apt_bootstrap_stage().rsplit(update, 1)
        stage = prefix + update + " || true" + suffix

        with self.assertRaises(validator.LockError):
            validator._validate_apt_stage(stage, sources, "fixture builder")

    def test_package_parser_is_scoped_to_install_and_expands_ninja_mapping(self):
        validator = self.load_script(VALIDATOR, "toolchain_package_parser_test")
        stage = """runtime
RUN printf 'deb [check-valid-until=no] %s trixie main\\n' snapshot
RUN set -eu; \\
    case "${TARGETARCH}" in \\
        amd64) ninja_package='ninja-build:amd64=1.12.1-1' ;; \\
        arm64) ninja_package='ninja-build:arm64=1.12.1-1+b1' ;; \\
    esac; \\
    apt-get install -y --no-install-recommends \\
        binutils=2.44-3 \\
        "${ninja_package}" \\
        libc6-dev=2.41-12+deb13u3; \\
    rm -rf /var/lib/apt/lists/*
"""

        self.assertEqual(
            validator._locked_packages_in_stage(stage, "fixture runtime"),
            [
                "binutils=2.44-3",
                "ninja-build:amd64=1.12.1-1",
                "ninja-build:arm64=1.12.1-1+b1",
                "libc6-dev=2.41-12+deb13u3",
            ],
        )

        unversioned = stage.replace(
            "        libc6-dev=2.41-12+deb13u3; \\\n",
            "        bash \\\n"
            "        libc6-dev=2.41-12+deb13u3; \\\n",
        )
        with self.assertRaises(validator.LockError):
            validator._locked_packages_in_stage(unversioned, "fixture runtime")

    def test_recipe_configuration_drift_is_rejected_even_with_rebased_hash(self):
        sources = self.make_sources()
        mac_build = self.root / ".github/scripts/build-p2996-macos.sh"
        mac_build.write_text(
            mac_build.read_text(encoding="utf-8").replace(
                "-DCMAKE_OSX_SYSROOT", "-DCMAKE_OSX_UNREVIEWED_SYSROOT"
            ),
            encoding="utf-8",
        )
        sources["recipes"][".github/scripts/build-p2996-macos.sh"] = (
            normalized_sha256(mac_build)
        )
        path = self.root / "toolchain-sources.lock"
        write_json(path, sources)

        completed = self.run_validator(path)

        self.assertNotEqual(completed.returncode, 0)
        self.assertIn("CMake configuration", completed.stderr)

    def test_recipe_configuration_rejects_non_option_in_continued_command(self):
        sources = self.make_sources()
        mac_build = self.root / ".github/scripts/build-p2996-macos.sh"
        mac_build.write_text(
            mac_build.read_text(encoding="utf-8").replace(
                "    -DCMAKE_BUILD_TYPE=Release \\\n",
                "    -DCMAKE_BUILD_TYPE=Release \\\n"
                "    $(printf unreviewed) \\\n",
            ),
            encoding="utf-8",
        )
        sources["recipes"][".github/scripts/build-p2996-macos.sh"] = (
            normalized_sha256(mac_build)
        )
        path = self.root / "toolchain-sources.lock"
        write_json(path, sources)

        completed = self.run_validator(path)

        self.assertNotEqual(completed.returncode, 0)
        self.assertIn("unexpected continued token", completed.stderr)

    def test_rebased_docker_recipe_cannot_neutralize_native_guard(self):
        sources = self.make_sources()
        dockerfile = self.root / ".github/docker/Dockerfile.gcc16"
        dockerfile.write_text(
            dockerfile.read_text(encoding="utf-8").replace(
                '    test "${BUILDARCH}" = "${native_arch}"\n',
                '    test "${BUILDARCH}" = "${native_arch}" || true\n',
                1,
            ),
            encoding="utf-8",
        )
        sources["recipes"][".github/docker/Dockerfile.gcc16"] = (
            normalized_sha256(dockerfile)
        )
        path = self.root / "toolchain-sources.lock"
        write_json(path, sources)

        completed = self.run_validator(path)

        self.assertNotEqual(completed.returncode, 0)
        self.assertIn("native", completed.stderr)

    def test_rebased_docker_recipe_cannot_shadow_architecture_before_guard(self):
        sources = self.make_sources()
        dockerfile = self.root / ".github/docker/Dockerfile.gcc16"
        content = dockerfile.read_text(encoding="utf-8")
        marker = 'RUN set -eu; \\\n'
        self.assertIn(marker, content)
        dockerfile.write_text(
            content.replace(
                marker,
                'ENV BUILDARCH=${TARGETARCH}\n' + marker,
                1,
            ),
            encoding="utf-8",
        )
        sources["recipes"][".github/docker/Dockerfile.gcc16"] = (
            normalized_sha256(dockerfile)
        )
        path = self.root / "toolchain-sources.lock"
        write_json(path, sources)

        completed = self.run_validator(path)

        self.assertNotEqual(completed.returncode, 0)
        self.assertIn("BUILDARCH", completed.stderr)

    def test_rebased_docker_recipe_cannot_add_noncanonical_apt_command(self):
        sources = self.make_sources()
        dockerfile = self.root / ".github/docker/Dockerfile.gcc16"
        content = dockerfile.read_text(encoding="utf-8")
        marker = '    test "${BUILDARCH}" = "${native_arch}"\n'
        self.assertIn(marker, content)
        dockerfile.write_text(
            content.replace(marker, marker + "RUN apt-get -y install wget\n", 1),
            encoding="utf-8",
        )
        sources["recipes"][".github/docker/Dockerfile.gcc16"] = (
            normalized_sha256(dockerfile)
        )
        path = self.root / "toolchain-sources.lock"
        write_json(path, sources)

        completed = self.run_validator(path)

        self.assertNotEqual(completed.returncode, 0)
        self.assertIn("apt", completed.stderr)

    def test_rebased_p2996_recipe_cannot_override_target_after_mapping(self):
        sources = self.make_sources()
        dockerfile = self.root / ".github/docker/Dockerfile.p2996"
        content = dockerfile.read_text(encoding="utf-8")
        marker = '    esac; \\\n    cmake -S llvm -B build -G Ninja \\\n'
        self.assertIn(marker, content)
        dockerfile.write_text(
            content.replace(
                marker,
                '    esac; \\\n'
                '    llvm_target=WebAssembly; \\\n'
                '    cmake -S llvm -B build -G Ninja \\\n',
                1,
            ),
            encoding="utf-8",
        )
        sources["recipes"][".github/docker/Dockerfile.p2996"] = (
            normalized_sha256(dockerfile)
        )
        path = self.root / "toolchain-sources.lock"
        write_json(path, sources)

        completed = self.run_validator(path)

        self.assertNotEqual(completed.returncode, 0)
        self.assertIn("target", completed.stderr.casefold())

    def test_rebased_docker_recipes_cannot_change_locked_source_inputs(self):
        attacks = (
            (
                ".github/docker/Dockerfile.gcc16",
                f"ARG DEBIAN_IMAGE={DEBIAN_IMAGE}",
                "ARG DEBIAN_IMAGE=docker.io/library/debian:latest",
            ),
            (
                ".github/docker/Dockerfile.gcc16",
                "ARG DEBIAN_SNAPSHOT=https://snapshot.debian.org/archive/"
                "debian/20260824T000000Z/",
                "ARG DEBIAN_SNAPSHOT=https://deb.debian.org/debian/",
            ),
            (
                ".github/docker/Dockerfile.gcc16",
                "ARG GCC_URL=https://gcc.gnu.org/pub/gcc/releases/gcc-16.2.0/"
                "gcc-16.2.0.tar.xz",
                "ARG GCC_URL=https://example.invalid/gcc.tar.xz",
            ),
            (
                ".github/docker/Dockerfile.gcc16",
                f"ARG GCC_SHA512={GCC_SHA512}",
                "ARG GCC_SHA512=" + "0" * 128,
            ),
            (
                ".github/docker/Dockerfile.gcc16",
                "ARG GMP_URL=https://gcc.gnu.org/pub/gcc/infrastructure/"
                "gmp-6.3.0.tar.bz2",
                "ARG GMP_URL=https://example.invalid/gmp.tar.bz2",
            ),
            (
                ".github/docker/Dockerfile.p2996",
                "ARG P2996_REPOSITORY=https://github.com/bloomberg/"
                "clang-p2996.git",
                "ARG P2996_REPOSITORY=https://example.invalid/clang.git",
            ),
            (
                ".github/docker/Dockerfile.p2996",
                f"ARG P2996_COMMIT={CLANG_COMMIT}",
                "ARG P2996_COMMIT=" + "f" * 40,
            ),
        )
        for relative, locked, replacement in attacks:
            with self.subTest(relative=relative, locked=locked.split("=", 1)[0]):
                dockerfile = self.root / relative
                original = dockerfile.read_text(encoding="utf-8")
                self.assertIn(locked, original)
                try:
                    dockerfile.write_text(
                        original.replace(locked, replacement, 1),
                        encoding="utf-8",
                    )
                    sources = self.make_sources()
                    sources["recipes"][relative] = normalized_sha256(dockerfile)
                    path = self.root / "toolchain-sources.lock"
                    write_json(path, sources)

                    completed = self.run_validator(path)

                    self.assertNotEqual(completed.returncode, 0)
                    self.assertIn("Dockerfile", completed.stderr)
                finally:
                    dockerfile.write_text(original, encoding="utf-8")

    def test_rebased_bake_recipe_cannot_swap_native_platform(self):
        sources = self.make_sources()
        bake = self.root / ".github/docker/docker-bake.hcl"
        bake.write_text(
            bake.read_text(encoding="utf-8").replace(
                'platforms  = ["linux/amd64"]',
                'platforms  = ["linux/arm64"]',
                1,
            ),
            encoding="utf-8",
        )
        sources["recipes"][".github/docker/docker-bake.hcl"] = normalized_sha256(
            bake
        )
        path = self.root / "toolchain-sources.lock"
        write_json(path, sources)

        completed = self.run_validator(path)

        self.assertNotEqual(completed.returncode, 0)
        self.assertIn("docker-bake.hcl", completed.stderr)

    def test_rebased_critical_recipes_reject_unreviewed_instructions(self):
        attacks = (
            (
                ".github/docker/Dockerfile.gcc16",
                lambda text: text.replace(
                    "WORKDIR /workspace\n",
                    "RUN cp /bin/false /opt/gcc-16.2.0/bin/g++\n"
                    "WORKDIR /workspace\n",
                    1,
                ),
            ),
            (
                ".github/docker/Dockerfile.gcc16",
                lambda text: text.replace(
                    "WORKDIR /opt/sources\n",
                    "ADD https://example.invalid/source.tar.xz /tmp/source.tar.xz\n"
                    "WORKDIR /opt/sources\n",
                    1,
                ),
            ),
            (
                ".github/docker/Dockerfile.p2996",
                lambda text: text.replace(
                    "WORKDIR /workspace\n", "COPY . /tmp/context\nWORKDIR /workspace\n", 1
                ),
            ),
            (
                ".github/docker/Dockerfile.p2996",
                lambda text: text.replace(
                    "WORKDIR /workspace\n", "RUN true\nWORKDIR /workspace\n", 1
                ),
            ),
        )
        for relative, transform in attacks:
            with self.subTest(relative=relative):
                completed = self.rebase_recipe(relative, transform)
                self.assertNotEqual(completed.returncode, 0, completed.stdout)
                (self.root / relative).write_bytes((ROOT / relative).read_bytes())

    def test_rebased_gcc_fetch_cannot_rewrite_url_digest_or_order(self):
        attacks = (
            lambda text: text.replace(
                'url="$1"; file="$2"; digest="$3"; \\\n',
                'url="$1"; file="$2"; digest="$3"; \\\n'
                '        url=https://example.invalid/unreviewed; \\\n',
                1,
            ),
            lambda text: text.replace(
                'url="$1"; file="$2"; digest="$3"; \\\n',
                'url="$1"; file="$2"; digest="$3"; \\\n'
                '        digest="$(sha512sum "${file}" | cut -d" " -f1)"; \\\n',
                1,
            ),
            lambda text: text.replace(
                "        curl --fail --location --proto '=https' --tlsv1.2 \\\n"
                '            --retry 3 --output "${file}" "${url}"; \\\n'
                "        printf '%s  %s\\n' \"${digest}\" \"${file}\" "
                "| sha512sum --check --strict -; \\\n",
                "        printf '%s  %s\\n' \"${digest}\" \"${file}\" "
                "| sha512sum --check --strict -; \\\n"
                "        curl --fail --location --proto '=https' --tlsv1.2 \\\n"
                '            --retry 3 --output "${file}" "${url}"; \\\n',
                1,
            ),
            lambda text: text.replace(
                '            --retry 3 --output "${file}" "${url}"; \\\n',
                '            --retry 3 --output "${file}" "${url}"; \\\n'
                '        tar -tf "${file}" >/dev/null; \\\n',
                1,
            ),
        )
        relative = ".github/docker/Dockerfile.gcc16"
        for transform in attacks:
            with self.subTest(transform=transform):
                completed = self.rebase_recipe(relative, transform)
                self.assertNotEqual(completed.returncode, 0, completed.stdout)
                (self.root / relative).write_bytes((ROOT / relative).read_bytes())

    def test_bake_and_workflow_cannot_inject_build_inputs(self):
        bake_attacks = (
            lambda text: text.replace(
                'target "gcc16-amd64" {\n',
                'target "gcc16-amd64" {\n  args = { UNREVIEWED = "1" }\n',
                1,
            ),
            lambda text: text.replace(
                'target "gcc16-amd64" {\n',
                'target "gcc16-amd64" {\n  context = "https://example.invalid/repo.git"\n',
                1,
            ),
            lambda text: text
            + '\ntarget "unreviewed" {\n  context = "https://example.invalid"\n}\n',
        )
        relative = ".github/docker/docker-bake.hcl"
        for transform in bake_attacks:
            with self.subTest(kind="bake"):
                completed = self.rebase_recipe(relative, transform)
                self.assertNotEqual(completed.returncode, 0, completed.stdout)
                (self.root / relative).write_bytes((ROOT / relative).read_bytes())

        workflow = ".github/workflows/toolchain-images.yml"
        output_override = (
            '            --set "${BAKE_TARGET}.output=type=image,'
            'push-by-digest=true,name-canonical=true,oci-mediatypes=true,push=true" \\\n'
        )
        for override in (
            "args.UNREVIEWED=1",
            "provenance=true",
            "sbom=0",
        ):
            with self.subTest(kind="workflow", override=override):
                completed = self.rebase_recipe(
                    workflow,
                    lambda text, override=override: text.replace(
                        output_override,
                        f'            --set "${{BAKE_TARGET}}.{override}" \\\n'
                        + output_override,
                        1,
                    ),
                )
                self.assertNotEqual(completed.returncode, 0, completed.stdout)
                self.assertIn(
                    "must not override bake build inputs",
                    completed.stderr,
                )
                (self.root / workflow).write_bytes((ROOT / workflow).read_bytes())

    def test_rebased_frozen_workflow_rejects_unreviewed_bytes(self):
        relative = ".github/workflows/toolchain-images.yml"
        completed = self.rebase_recipe(
            relative,
            lambda text: text + "\n# unreviewed workflow bytes\n",
        )

        self.assertNotEqual(completed.returncode, 0, completed.stdout)
        self.assertIn("reviewed semantic recipe", completed.stderr)
        self.assertIn(relative, completed.stderr)

    def test_rebased_macos_recipes_reject_critical_step_mutations(self):
        attacks = (
            (
                ".github/scripts/build-p2996-macos.sh",
                lambda text: text.replace(
                    '[[ "$(git -C "${source_dir}" rev-parse HEAD)" == "${p2996_commit}" ]]',
                    ": # skipped locked revision verification",
                    1,
                ),
            ),
            (
                ".github/scripts/build-p2996-macos.sh",
                lambda text: text.replace(
                    'cmake --install "${build_dir}" --strip',
                    'cmake --install "${build_dir}"',
                    1,
                ),
            ),
            (
                ".github/scripts/build-p2996-macos.sh",
                lambda text: text.replace(
                    'if [[ "${host_architecture}" != "${architecture}" ]]; then',
                    "if false; then",
                    1,
                ),
            ),
            (
                ".github/scripts/build-p2996-macos.sh",
                lambda text: text.replace(
                    '[[ "${actual_sdk_build}" == "${sdk_build}" ]] || {',
                    "true || {",
                    1,
                ),
            ),
            (
                ".github/scripts/build-p2996-macos.sh",
                lambda text: text.replace(
                    "available_memory / 2147483648",
                    "available_memory / 1",
                    1,
                ),
            ),
            (
                ".github/scripts/build-p2996-macos.sh",
                lambda text: text.replace("--require-locked-sdk", "--allow-unlocked-sdk", 1),
            ),
            (
                ".github/scripts/verify-p2996-toolchain.sh",
                lambda text: text.replace(
                    'actual_sha256="$(shasum -a 256 "${archive}" | awk \'{print $1}\')"',
                    'actual_sha256="${expected_sha256}"',
                    1,
                ),
            ),
            (
                ".github/scripts/verify-p2996-toolchain.sh",
                lambda text: text.replace(
                    "printf '#include <vector>\\n' \\\n",
                    "printf 'int main() {}\\n' \\\n",
                    1,
                ),
            ),
            (
                ".github/scripts/verify-p2996-toolchain.sh",
                lambda text: text.replace(
                    'zstd -dc "${archive}" | python3 "${archive_validator}"',
                    ": # skipped archive validation",
                    1,
                ),
            ),
            (
                ".github/scripts/verify-p2996-toolchain.sh",
                lambda text: text.replace(
                    'python3 - "${rpath_output}" "${library_dir}" <<\'PY\'',
                    'python3 - "${rpath_output}" "/unreviewed" <<\'PY\'',
                    1,
                ),
            ),
            (
                ".github/scripts/verify-p2996-toolchain.sh",
                lambda text: text.replace(
                    'python3 - "${dyld_output}" "${library_dir}" <<\'PY\'',
                    'python3 - "${dyld_output}" "/usr/lib" <<\'PY\'',
                    1,
                ),
            ),
        )
        for relative, transform in attacks:
            with self.subTest(relative=relative):
                completed = self.rebase_recipe(relative, transform)
                self.assertNotEqual(completed.returncode, 0, completed.stdout)
                (self.root / relative).write_bytes((ROOT / relative).read_bytes())

    def test_complete_output_lock_prints_only_digest_qualified_index(self):
        sources = self.root / "toolchain-sources.lock"
        write_json(sources, self.make_sources())
        outputs = self.root / "toolchains.lock"
        write_json(outputs, self.make_outputs(sources))

        completed = self.run_validator(
            sources, outputs, "--print-image", "p2996"
        )

        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertEqual(
            completed.stdout.strip(),
            "ghcr.io/ximicpp/typelayout-p2996@sha256:" + "4" * 64,
        )

    def test_output_lock_rejects_noncanonical_runtime_identities(self):
        mutations = (
            lambda outputs: outputs["linux"]["gcc"].__setitem__(
                "compiler_version", "gcc (GCC) 16.2.0"
            ),
            lambda outputs: outputs["linux"]["gcc"].__setitem__(
                "stdlib", "libstdc++"
            ),
            lambda outputs: outputs["linux"]["p2996"].__setitem__(
                "stdlib", "libc++"
            ),
            lambda outputs: outputs["macos"]["arm64_macos_clang"].__setitem__(
                "compiler_version", "clang version unreviewed"
            ),
            lambda outputs: outputs["macos"]["x86_64_macos_clang"].__setitem__(
                "stdlib", "libc++-999999"
            ),
        )
        sources = self.root / "toolchain-sources.lock"
        write_json(sources, self.make_sources())
        for mutate in mutations:
            with self.subTest(mutate=mutate):
                outputs_value = self.make_outputs(sources)
                mutate(outputs_value)
                outputs = self.root / "toolchains.lock"
                write_json(outputs, outputs_value)
                completed = self.run_validator(sources, outputs)
                self.assertNotEqual(completed.returncode, 0, completed.stdout)

    def test_duplicate_platform_manifest_is_rejected(self):
        sources = self.root / "toolchain-sources.lock"
        write_json(sources, self.make_sources())
        outputs_value = self.make_outputs(sources)
        outputs_value["linux"]["p2996"]["platforms"]["linux/amd64"][
            "manifest_digest"
        ] = "sha256:" + "2" * 64
        outputs = self.root / "toolchains.lock"
        write_json(outputs, outputs_value)

        completed = self.run_validator(sources, outputs)

        self.assertNotEqual(completed.returncode, 0)
        self.assertIn("unique", completed.stderr)

    def test_output_lock_rejects_each_noncanonical_platform_target(self):
        mutations = (
            lambda outputs: outputs["linux"]["gcc"]["platforms"][
                "linux/amd64"
            ].__setitem__("target", "aarch64-unknown-linux-gnu"),
            lambda outputs: outputs["linux"]["p2996"]["platforms"][
                "linux/amd64"
            ].__setitem__("target", "x86_64-linux-gnu"),
            lambda outputs: outputs["linux"]["p2996"]["platforms"][
                "linux/arm64"
            ].__setitem__("target", "arm64-unknown-linux-gnu"),
            lambda outputs: outputs["macos"]["arm64_macos_clang"].__setitem__(
                "target", "x86_64-apple-macosx15.0.0"
            ),
            lambda outputs: outputs["macos"]["x86_64_macos_clang"].__setitem__(
                "target", "x86_64-apple-macosx15.1.0"
            ),
        )
        sources = self.root / "toolchain-sources.lock"
        write_json(sources, self.make_sources())
        for mutate in mutations:
            with self.subTest(mutate=mutate):
                outputs_value = self.make_outputs(sources)
                mutate(outputs_value)
                outputs = self.root / "toolchains.lock"
                write_json(outputs, outputs_value)
                completed = self.run_validator(sources, outputs)
                self.assertNotEqual(completed.returncode, 0, completed.stdout)

    def test_observed_runner_change_does_not_change_lock_validity(self):
        sources = self.root / "toolchain-sources.lock"
        write_json(sources, self.make_sources())
        outputs_value = self.make_outputs(sources)
        outputs_value["macos"]["arm64_macos_clang"]["observed_runner"] = {
            "image_os": "macos15-refreshed",
            "image_version": "future",
        }
        outputs = self.root / "toolchains.lock"
        write_json(outputs, outputs_value)

        completed = self.run_validator(sources, outputs)

        self.assertEqual(completed.returncode, 0, completed.stderr)

    def test_macos_archive_url_must_use_exact_immutable_release_path(self):
        sources = self.root / "toolchain-sources.lock"
        write_json(sources, self.make_sources())
        outputs_value = self.make_outputs(sources)
        outputs_value["macos"]["arm64_macos_clang"]["url"] = (
            outputs_value["macos"]["arm64_macos_clang"]["url"].replace(
                "/ximicpp/TypeLayout/", "/unreviewed/TypeLayout/"
            )
        )
        outputs = self.root / "toolchains.lock"
        write_json(outputs, outputs_value)

        completed = self.run_validator(sources, outputs)

        self.assertNotEqual(completed.returncode, 0)
        self.assertIn("immutable", completed.stderr)

    def test_print_image_requires_a_complete_output_lock(self):
        sources = self.root / "toolchain-sources.lock"
        write_json(sources, self.make_sources())

        completed = self.run_validator(
            sources, None, "--print-image", "gcc16"
        )

        self.assertNotEqual(completed.returncode, 0)
        self.assertIn("--outputs", completed.stderr)

    def test_bootstrap_rejects_unreviewed_compiler_revision(self):
        completed = subprocess.run(
            [
                sys.executable,
                str(BOOTSTRAP),
                "--gcc-version",
                "16.2.0",
                "--clang-commit",
                "f" * 40,
                "--recipe-root",
                str(self.root),
                "--output",
                str(self.root / "out.lock"),
            ],
            capture_output=True,
            text=True,
            check=False,
        )

        self.assertNotEqual(completed.returncode, 0)
        self.assertIn(CLANG_COMMIT, completed.stderr)
        self.assertFalse((self.root / "out.lock").exists())

    def test_bootstrap_refuses_cross_host_redirect(self):
        bootstrap = self.load_script(BOOTSTRAP, "toolchain_redirect_test")
        request = bootstrap.Request("https://gcc.gnu.org/source")

        with self.assertRaisesRegex(
            bootstrap.BootstrapError, "cross-host redirect refused"
        ):
            bootstrap.SameHostRedirectHandler().redirect_request(
                request,
                None,
                302,
                "Found",
                {},
                "https://example.invalid/unreviewed",
            )

    def test_bootstrap_refuses_redirect_downgrade_and_nonstandard_port(self):
        bootstrap = self.load_script(BOOTSTRAP, "toolchain_redirect_security_test")
        request = bootstrap.Request("https://gcc.gnu.org/source")
        for redirect in (
            "http://gcc.gnu.org/unreviewed",
            "https://gcc.gnu.org:8443/unreviewed",
        ):
            with self.subTest(redirect=redirect), self.assertRaises(
                bootstrap.BootstrapError
            ):
                bootstrap.SameHostRedirectHandler().redirect_request(
                    request, None, 302, "Found", {}, redirect
                )

    def test_bootstrap_builds_complete_lock_from_resolved_inputs(self):
        self.assertTrue(BOOTSTRAP.is_file(), "bootstrap script must exist")
        bootstrap = self.load_script(BOOTSTRAP, "toolchain_source_bootstrap_test")
        gcc_sums = f"{GCC_SHA512}  gcc-16.2.0.tar.xz\n"
        prerequisite_sums = "\n".join(
            f"{record['sha512']}  {record['filename']}"
            for record in self.make_sources()["gcc"]["prerequisites"].values()
        )

        def fetch_text(url):
            if url.endswith("sha512.sum"):
                return gcc_sums
            if url.endswith("prerequisites.sha512"):
                return prerequisite_sums
            if url.endswith("images/ubuntu/Ubuntu2404-Readme.md"):
                return UBUNTU_RUNNER_INVENTORY
            if url.endswith(
                ("images/macos/macos-15-Readme.md", "images/macos/macos-15-arm64-Readme.md")
            ):
                return MACOS_RUNNER_INVENTORY
            raise AssertionError(f"unexpected URL {url}")

        def resolve_image(reference):
            return reference.split(":", 1)[0] + "@sha256:" + hashlib.sha256(
                reference.encode("utf-8")
            ).hexdigest()

        def resolve_packages(package_names):
            return [f"{name}=fixture-1" for name in package_names]

        lock = bootstrap.build_source_lock(
            recipe_root=self.root,
            gcc_version="16.2.0",
            clang_commit=CLANG_COMMIT,
            fetch_text=fetch_text,
            resolve_image=resolve_image,
            resolve_packages=resolve_packages,
        )

        self.assertEqual(lock["gcc"]["source"]["sha512"], GCC_SHA512)
        self.assertEqual(
            lock["gcc"]["configure_flags"][0], "--prefix=/opt/gcc-16.2.0"
        )
        self.assertEqual(lock["p2996"]["projects"], ["clang"])
        self.assertEqual(
            lock["p2996"]["runtimes"], ["libcxx", "libcxxabi", "libunwind"]
        )
        self.assertEqual(
            lock["macos"]["nodes"]["arm64_macos_clang"]["sdk_build"],
            "24F74",
        )
        self.assertEqual(
            lock["linux"]["docker"]["dockerfile_frontend"],
            DOCKERFILE_FRONTEND,
        )
        self.assertEqual(
            set(lock["p2996"]["platform_cmake_flags"]),
            {"linux/amd64", "linux/arm64", "macos/arm64", "macos/x86_64"},
        )
        self.assertEqual(
            lock["p2996"]["platform_cmake_flags"]["linux/amd64"][1],
            "-DLLVM_TARGETS_TO_BUILD=X86",
        )
        self.assertEqual(
            lock["p2996"]["platform_cmake_flags"]["linux/arm64"][1],
            "-DLLVM_TARGETS_TO_BUILD=AArch64",
        )
        for package_set in ("gcc_runtime", "p2996_runtime"):
            self.assertIn("binutils=fixture-1", lock["linux"]["packages"][package_set])
            self.assertIn("libc6-dev=fixture-1", lock["linux"]["packages"][package_set])
        self.assertEqual(set(lock["recipes"]), set(RECIPE_PATHS))

    def test_bootstrap_rejects_missing_or_disagreeing_runner_inventory(self):
        bootstrap = self.load_script(BOOTSTRAP, "toolchain_inventory_test")
        gcc_sums = f"{GCC_SHA512}  gcc-16.2.0.tar.xz\n"
        prerequisite_sums = "\n".join(
            f"{record['sha512']}  {record['filename']}"
            for record in self.make_sources()["gcc"]["prerequisites"].values()
        )

        def build_with(ubuntu, mac_x64, mac_arm64):
            def fetch_text(url):
                if url.endswith("sha512.sum"):
                    return gcc_sums
                if url.endswith("prerequisites.sha512"):
                    return prerequisite_sums
                if url.endswith("images/ubuntu/Ubuntu2404-Readme.md"):
                    return ubuntu
                if url.endswith("images/macos/macos-15-Readme.md"):
                    return mac_x64
                if url.endswith("images/macos/macos-15-arm64-Readme.md"):
                    return mac_arm64
                raise AssertionError(url)

            return bootstrap.build_source_lock(
                recipe_root=self.root,
                gcc_version="16.2.0",
                clang_commit=CLANG_COMMIT,
                fetch_text=fetch_text,
                resolve_image=lambda reference: (
                    reference.split(":", 1)[0] + "@sha256:" + "a" * 64
                ),
                resolve_packages=lambda names: [f"{name}=1" for name in names],
            )

        with self.assertRaises(bootstrap.BootstrapError):
            build_with(
                UBUNTU_RUNNER_INVENTORY.replace("- Docker-Buildx 0.36.1\n", ""),
                MACOS_RUNNER_INVENTORY,
                MACOS_RUNNER_INVENTORY,
            )
        with self.assertRaises(bootstrap.BootstrapError):
            build_with(
                UBUNTU_RUNNER_INVENTORY,
                MACOS_RUNNER_INVENTORY,
                MACOS_RUNNER_INVENTORY.replace("16F6", "16F7"),
            )

    def test_bootstrap_normalizes_lzma_json_and_download_limit_errors(self):
        bootstrap = self.load_script(BOOTSTRAP, "toolchain_remote_error_test")
        with self.assertRaises(bootstrap.BootstrapError):
            bootstrap._parse_package_versions(b"not an xz stream")

        original_limit = bootstrap.MAX_PACKAGES_DECOMPRESSED_BYTES
        bootstrap.MAX_PACKAGES_DECOMPRESSED_BYTES = 8
        try:
            with self.assertRaises(bootstrap.BootstrapError):
                bootstrap._parse_package_versions(lzma.compress(b"x" * 9))
        finally:
            bootstrap.MAX_PACKAGES_DECOMPRESSED_BYTES = original_limit

        with self.assertRaises(bootstrap.BootstrapError):
            bootstrap._decode_json(b"{broken", "fixture JSON")
        with self.assertRaises(bootstrap.BootstrapError):
            bootstrap._require_json_string({}, "token", "fixture JSON")

        class Response:
            def __init__(self):
                self.chunks = [b"1234", b"5", b""]

            def read(self, _size):
                return self.chunks.pop(0)

        with self.assertRaises(bootstrap.BootstrapError):
            bootstrap._read_limited(Response(), 4, "fixture download")

        class BrokenResponse:
            def read(self, _size):
                raise OSError("fixture transport failure")

        with self.assertRaises(bootstrap.BootstrapError):
            bootstrap._read_limited(BrokenResponse(), 4, "fixture download")

    def test_checked_in_source_lock_and_recipes_validate(self):
        completed = subprocess.run(
            [
                sys.executable,
                str(VALIDATOR),
                "--sources",
                str(SOURCE_LOCK),
                "--recipe-root",
                str(ROOT),
            ],
            cwd=ROOT,
            capture_output=True,
            text=True,
            check=False,
        )

        self.assertEqual(completed.returncode, 0, completed.stderr)

    def test_native_recipes_exclude_unlocked_shortcuts(self):
        self.assertTrue(
            (ROOT / ".github/docker/docker-bake.hcl").is_file(),
            "native bake file must exist",
        )
        gcc = (ROOT / ".github/docker/Dockerfile.gcc16").read_text(encoding="utf-8")
        clang = (ROOT / ".github/docker/Dockerfile.p2996").read_text(
            encoding="utf-8"
        )
        bake = (ROOT / ".github/docker/docker-bake.hcl").read_text(
            encoding="utf-8"
        )
        mac_build = (
            ROOT / ".github/scripts/build-p2996-macos.sh"
        ).read_text(encoding="utf-8")

        self.assertNotIn("gcc-latest.deb", gcc)
        self.assertNotIn("download_prerequisites", gcc)
        self.assertIn("gcc-16.2.0.tar.xz", gcc)
        self.assertIn("install-strip", gcc)
        self.assertNotIn("--branch p2996", clang)
        self.assertNotIn("clang-tools-extra", clang)
        self.assertIn(CLANG_COMMIT, clang)
        self.assertIn('LLVM_ENABLE_PROJECTS="clang"', clang)
        self.assertIn('LLVM_ENABLE_RUNTIMES="libcxx;libcxxabi;libunwind"', clang)
        self.assertIn("LLVM_ENABLE_PER_TARGET_RUNTIME_DIR=ON", clang)
        self.assertIn('/lib/${runtime_triple}', clang)
        self.assertIn(
            "/opt/p2996-toolchain/lib/x86_64-unknown-linux-gnu", clang
        )
        self.assertIn(
            "/opt/p2996-toolchain/lib/aarch64-unknown-linux-gnu", clang
        )
        self.assertIn("-nostdinc++", clang)
        self.assertIn("-isystem /opt/p2996-toolchain/include/c++/v1", clang)
        self.assertIn('-L "${runtime_dir}"', clang)
        self.assertIn('-Wl,-rpath,"${runtime_dir}"', clang)
        self.assertIn("2147483648", gcc)
        self.assertIn("2147483648", clang)
        self.assertIn("2147483648", mac_build)
        self.assertIn("vm_stat", mac_build)
        for target in (
            "gcc16-amd64",
            "gcc16-arm64",
            "p2996-amd64",
            "p2996-arm64",
        ):
            self.assertEqual(bake.count(f'target "{target}"'), 1)
        self.assertEqual(bake.count('platforms  = ["linux/amd64"]'), 2)
        self.assertEqual(bake.count('platforms  = ["linux/arm64"]'), 2)
        self.assertIn("provenance = false", bake)
        self.assertIn("sbom       = false", bake)
        self.assertIn("push-by-digest=true", bake)
        self.assertNotIn("qemu", bake.lower())
        self.assertNotIn(":latest", bake)

    def test_linux_builder_memory_probe_outputs_decimal_bytes(self):
        awk_runner = self.root / "run-awk.sh"
        awk_runner.write_bytes(b'#!/bin/sh\nawk -f "$1"\n')
        for index, relative in enumerate(
            (
                ".github/docker/Dockerfile.gcc16",
                ".github/docker/Dockerfile.p2996",
            )
        ):
            content = (ROOT / relative).read_text(encoding="utf-8")
            builder = re.split(r"(?m)^FROM ", content)[1]
            match = re.search(
                r'''available="\$\(awk '([^']+)' /proc/meminfo\)"''', builder
            )
            self.assertIsNotNone(match, relative)
            awk_program = self.root / f"memory-probe-{index}.awk"
            awk_program.write_bytes((match.group(1) + "\n").encode("utf-8"))

            completed = subprocess.run(
                bash_command(awk_runner, bash_path(awk_program)),
                input="MemAvailable: 123 kB\n",
                capture_output=True,
                text=True,
                check=False,
            )

            with self.subTest(recipe=relative):
                self.assertEqual(completed.returncode, 0, completed.stderr)
                self.assertEqual(completed.stdout, "125952\n")

    def test_every_docker_stage_requires_a_native_build_before_work(self):
        for relative in (
            ".github/docker/Dockerfile.gcc16",
            ".github/docker/Dockerfile.p2996",
        ):
            content = (ROOT / relative).read_text(encoding="utf-8")
            stages = re.split(r"(?m)^FROM ", content)[1:]
            self.assertEqual(len(stages), 2, relative)
            for index, stage in enumerate(stages):
                with self.subTest(recipe=relative, stage=index):
                    self.assertEqual(stage.count("ARG BUILDARCH"), 1)
                    self.assertEqual(stage.count("ARG TARGETARCH"), 1)
                    guard = 'test "${BUILDARCH}" = "${TARGETARCH}"'
                    self.assertEqual(stage.count(guard), 1)
                    self.assertEqual(stage.count('test -n "${BUILDARCH}"'), 1)
                    self.assertEqual(stage.count('test -n "${TARGETARCH}"'), 1)
                    self.assertEqual(stage.count('case "$(uname -m)" in'), 1)
                    self.assertEqual(
                        stage.count('test "${BUILDARCH}" = "${native_arch}"'), 1
                    )
                    first_run = stage.index("RUN ")
                    guard_index = stage.index(guard)
                    self.assertLess(first_run, guard_index)
                    for token in (
                        "apt-get",
                        "curl ",
                        "git fetch",
                        "cmake -S",
                        "/configure",
                        "make -j",
                    ):
                        if token in stage:
                            self.assertLess(guard_index, stage.index(token), token)

    def test_runtime_probe_dependencies_are_locked_and_installed(self):
        sources = json.loads(SOURCE_LOCK.read_text(encoding="utf-8"))
        for package_set, relative in (
            ("gcc_runtime", ".github/docker/Dockerfile.gcc16"),
            ("p2996_runtime", ".github/docker/Dockerfile.p2996"),
        ):
            runtime = re.split(
                r"(?m)^FROM ", (ROOT / relative).read_text(encoding="utf-8")
            )[2]
            for package in (
                "binutils=2.44-3",
                "libc6-dev=2.41-12+deb13u3",
            ):
                with self.subTest(package_set=package_set, package=package):
                    self.assertIn(package, sources["linux"]["packages"][package_set])
                    self.assertIn(package, runtime)

        p2996_runtime = re.split(
            r"(?m)^FROM ",
            (ROOT / ".github/docker/Dockerfile.p2996").read_text(
                encoding="utf-8"
            ),
        )[2]
        libgcc_packages = [
            package
            for package in sources["linux"]["packages"]["p2996_runtime"]
            if package.startswith("libgcc-14-dev=")
        ]
        self.assertEqual(len(libgcc_packages), 1)
        self.assertIn(libgcc_packages[0], p2996_runtime)
        self.assertIn(
            'crtbegin="$(clang++ --print-file-name=crtbeginS.o)"',
            p2996_runtime,
        )
        self.assertIn('test "${crtbegin}" != crtbeginS.o', p2996_runtime)
        self.assertIn('test -f "${crtbegin}"', p2996_runtime)

    def test_linux_p2996_uses_target_config_before_generic_libcxx_headers(self):
        sources = json.loads(SOURCE_LOCK.read_text(encoding="utf-8"))
        flags = shlex.split(sources["p2996"]["flags"])
        target_include = (
            "${TOOLCHAIN_ROOT}/include/${TARGET_TRIPLE}/c++/v1"
        )
        generic_include = "${TOOLCHAIN_ROOT}/include/c++/v1"
        self.assertEqual(
            flags[flags.index("-nostdinc++") : flags.index(generic_include) + 1],
            [
                "-nostdinc++",
                "-isystem",
                target_include,
                "-isystem",
                generic_include,
            ],
        )

        stages = re.split(
            r"(?m)^FROM ",
            (ROOT / ".github/docker/Dockerfile.p2996").read_text(
                encoding="utf-8"
            ),
        )[1:]
        self.assertEqual(len(stages), 2)
        for stage in stages:
            self.assertIn(
                'target_include_dir="/opt/p2996-toolchain/include/'
                '${runtime_triple}/c++/v1"',
                stage,
            )
            self.assertIn(
                'test -f "${target_include_dir}/__config_site"', stage
            )
        runtime = stages[1]
        self.assertIn(
            '-nostdinc++ -isystem "${target_include_dir}" \\\n'
            '    -isystem /opt/p2996-toolchain/include/c++/v1 \\',
            runtime,
        )

    def test_dockerfile_frontend_is_digest_locked(self):
        sources = json.loads(SOURCE_LOCK.read_text(encoding="utf-8"))
        self.assertEqual(
            sources["linux"]["docker"]["dockerfile_frontend"],
            DOCKERFILE_FRONTEND,
        )
        for relative in (
            ".github/docker/Dockerfile.gcc16",
            ".github/docker/Dockerfile.p2996",
        ):
            first_line = (ROOT / relative).read_text(encoding="utf-8").splitlines()[0]
            self.assertEqual(first_line, f"# syntax={DOCKERFILE_FRONTEND}")

    def test_archive_validator_rejects_escaping_links_and_special_files(self):
        verify_script = (
            ROOT / ".github/scripts/verify-p2996-toolchain.sh"
        ).read_text(encoding="utf-8")
        match = re.search(
            r"# BEGIN TOOLCHAIN ARCHIVE VALIDATOR\n"
            r"archive_validator=[^\n]+\n"
            r"cat >[^\n]+ <<'PY'\n(.*?)\nPY\n"
            r"# END TOOLCHAIN ARCHIVE VALIDATOR",
            verify_script,
            re.DOTALL,
        )
        self.assertIsNotNone(match, "archive validator must be an executable unit")
        validator = match.group(1)

        def validate(*members, source=validator):
            archive = io.BytesIO()
            with tarfile.open(fileobj=archive, mode="w") as output:
                for member in members:
                    output.addfile(member)
            return subprocess.run(
                [sys.executable, "-c", source],
                input=archive.getvalue(),
                capture_output=True,
                check=False,
            )

        root = tarfile.TarInfo("p2996-toolchain")
        root.type = tarfile.DIRTYPE
        regular = tarfile.TarInfo("p2996-toolchain/bin/clang++")
        regular.size = 0
        self.assertEqual(validate(root, regular).returncode, 0)

        valid_link = tarfile.TarInfo("p2996-toolchain/bin/clang")
        valid_link.type = tarfile.SYMTYPE
        valid_link.linkname = "clang++"
        self.assertEqual(validate(root, regular, valid_link).returncode, 0)

        duplicate = tarfile.TarInfo("p2996-toolchain/bin/./clang++")
        duplicate.size = 0
        duplicate_rejected = validate(root, regular, duplicate)
        self.assertNotEqual(duplicate_rejected.returncode, 0)
        self.assertIn(b"duplicate normalized", duplicate_rejected.stderr)

        unresolved = tarfile.TarInfo("p2996-toolchain/bin/unresolved")
        unresolved.type = tarfile.SYMTYPE
        unresolved.linkname = "missing"
        self_link = tarfile.TarInfo("p2996-toolchain/bin/self")
        self_link.type = tarfile.SYMTYPE
        self_link.linkname = "self"
        cycle_a = tarfile.TarInfo("p2996-toolchain/bin/a")
        cycle_a.type = tarfile.SYMTYPE
        cycle_a.linkname = "b"
        cycle_b = tarfile.TarInfo("p2996-toolchain/bin/b")
        cycle_b.type = tarfile.SYMTYPE
        cycle_b.linkname = "a"
        for links in ((unresolved,), (self_link,), (cycle_a, cycle_b)):
            with self.subTest(links=[link.name for link in links]):
                rejected = validate(root, *links)
                self.assertNotEqual(rejected.returncode, 0, rejected.stderr)
                self.assertRegex(
                    rejected.stderr.decode("utf-8"), r"unresolved|cycle"
                )

        attacks = []
        absolute_link = tarfile.TarInfo("p2996-toolchain/bin/clang++")
        absolute_link.type = tarfile.SYMTYPE
        absolute_link.linkname = "/tmp/unreviewed"
        attacks.append(absolute_link)
        relative_link = tarfile.TarInfo("p2996-toolchain/bin/clang++")
        relative_link.type = tarfile.SYMTYPE
        relative_link.linkname = "../../../tmp/unreviewed"
        attacks.append(relative_link)
        escaping_hardlink = tarfile.TarInfo("p2996-toolchain/bin/clang++")
        escaping_hardlink.type = tarfile.LNKTYPE
        escaping_hardlink.linkname = "outside-toolchain"
        attacks.append(escaping_hardlink)
        fifo = tarfile.TarInfo("p2996-toolchain/unsafe-fifo")
        fifo.type = tarfile.FIFOTYPE
        attacks.append(fifo)
        for attack in attacks:
            with self.subTest(type=attack.type, link=attack.linkname):
                rejected = validate(root, attack)
                self.assertNotEqual(rejected.returncode, 0, rejected.stderr)

        oversized_pax = tarfile.TarInfo("pax-header")
        oversized_pax.type = tarfile.XHDTYPE
        oversized_pax.size = 1048577
        pax_rejected = validate(oversized_pax)
        self.assertNotEqual(pax_rejected.returncode, 0)
        self.assertIn(b"extended tar header", pax_rejected.stderr)

        for sparse_headers in (
            {"GNU.sparse.map": "0,0", "GNU.sparse.size": "0"},
            {"SCHILY.filetype": "sparse", "SCHILY.realsize": "1024"},
        ):
            with self.subTest(sparse_headers=sparse_headers):
                archive = io.BytesIO()
                with tarfile.open(
                    fileobj=archive, mode="w", format=tarfile.PAX_FORMAT
                ) as output:
                    output.addfile(root)
                    sparse = tarfile.TarInfo("p2996-toolchain/sparse")
                    sparse.size = 0
                    sparse.pax_headers = sparse_headers
                    output.addfile(sparse)
                sparse_rejected = subprocess.run(
                    [sys.executable, "-c", validator],
                    input=archive.getvalue(),
                    capture_output=True,
                    check=False,
                )
                self.assertNotEqual(
                    sparse_rejected.returncode, 0, sparse_rejected.stderr
                )
                self.assertIn(b"sparse", sparse_rejected.stderr.lower())

        oversized = tarfile.TarInfo("p2996-toolchain/oversized")
        oversized.size = 8589934593
        self.assertNotEqual(validate(root, oversized).returncode, 0)
        one_member_validator = validator.replace(
            "MAX_MEMBERS = 200000", "MAX_MEMBERS = 1"
        )
        self.assertNotEqual(
            validate(root, regular, source=one_member_validator).returncode,
            0,
        )
        self.assertIn(
            'archive_size="$(stat -f \'%z\' "${archive}")"', verify_script
        )
        self.assertIn("archive_size >= 2147483648", verify_script)
        self.assertIn("--max-filesize 2147483647", verify_script)

    def test_locked_flag_expander_preserves_spaceful_paths_as_nul_tokens(self):
        verify_script = (
            ROOT / ".github/scripts/verify-p2996-toolchain.sh"
        ).read_text(encoding="utf-8")
        match = re.search(
            r"# BEGIN LOCKED FLAG EXPANDER\n(.*?)\n"
            r"# END LOCKED FLAG EXPANDER",
            verify_script,
            re.DOTALL,
        )
        self.assertIsNotNone(match, "flag expander must be an executable unit")
        template = (
            "-nostdinc++ -isystem ${TOOLCHAIN_ROOT}/include/c++/v1 "
            "-isysroot ${SDKROOT} -L ${TOOLCHAIN_ROOT}/lib "
            "-Wl,-rpath,${TOOLCHAIN_ROOT}/lib/${TARGET_TRIPLE}"
        )
        completed = subprocess.run(
            [
                sys.executable,
                "-c",
                match.group(1),
                template,
                "/tmp/toolchain root",
                "/tmp/SDK Root",
                "arm64-apple-macosx15.0.0",
            ],
            capture_output=True,
            check=False,
        )
        self.assertEqual(completed.returncode, 0, completed.stderr)

        self.assertEqual(
            completed.stdout.rstrip(b"\0").split(b"\0"),
            [
                b"-nostdinc++",
                b"-isystem",
                b"/tmp/toolchain root/include/c++/v1",
                b"-isysroot",
                b"/tmp/SDK Root",
                b"-L",
                b"/tmp/toolchain root/lib",
                b"-Wl,-rpath,/tmp/toolchain root/lib/arm64-apple-macosx15.0.0",
            ],
        )

    def test_macos_runtime_load_validator_tracks_system_libcxx_transitions(self):
        verify_script = (
            ROOT / ".github/scripts/verify-p2996-toolchain.sh"
        ).read_text(encoding="utf-8")
        match = re.search(
            r"# BEGIN MACOS RUNTIME LOAD VALIDATOR\n(.*?)\n"
            r"# END MACOS RUNTIME LOAD VALIDATOR",
            verify_script,
            re.DOTALL,
        )
        self.assertIsNotNone(match, "runtime validator must be executable")
        library_dir = self.root / "toolchain root/lib"
        library_dir.mkdir(parents=True)
        expected = []
        for name in ("libc++.1.dylib", "libc++abi.1.dylib", "libunwind.1.dylib"):
            real_path = library_dir / name.replace(".1.dylib", ".1.0.dylib")
            real_path.write_bytes(b"runtime")
            alias = library_dir / name
            alias.symlink_to(real_path.name)
            expected.append(str(real_path.resolve()))
        dyld = self.root / "dyld.txt"
        dyld.write_text(
            "\n".join(f"dyld[1]: {path}" for path in expected)
            + "\n"
            + "dyld[1]: /usr/lib/libc++.1.dylib\n"
            + "dyld[1]: move loaded to delayed: XPCSupport\n"
            + "dyld[1]: move loaded to delayed: libc++.1.dylib\n",
            encoding="utf-8",
        )
        command = [
            sys.executable,
            "-c",
            match.group(1),
            str(dyld),
            str(library_dir),
        ]
        completed = subprocess.run(
            command, capture_output=True, text=True, check=False
        )
        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertIn(
            "MACOS RUNTIME TRACE dyld[1]: /usr/lib/libc++.1.dylib",
            completed.stderr,
        )
        self.assertIn(
            "MACOS RUNTIME TRACE dyld[1]: move loaded to delayed: "
            "libc++.1.dylib",
            completed.stderr,
        )

        dyld.write_text(
            dyld.read_text(encoding="utf-8")
            + "dyld[1]: move delayed to loaded: libc++.1.dylib\n",
            encoding="utf-8",
        )
        completed = subprocess.run(
            command, capture_output=True, text=True, check=False
        )
        self.assertEqual(completed.returncode, 0, completed.stderr)

    def test_macos_runtime_load_validator_tracks_system_libcxxabi_transitions(self):
        verify_script = (
            ROOT / ".github/scripts/verify-p2996-toolchain.sh"
        ).read_text(encoding="utf-8")
        match = re.search(
            r"# BEGIN MACOS RUNTIME LOAD VALIDATOR\n(.*?)\n"
            r"# END MACOS RUNTIME LOAD VALIDATOR",
            verify_script,
            re.DOTALL,
        )
        self.assertIsNotNone(match, "runtime validator must be executable")
        library_dir = self.root / "toolchain root/lib"
        library_dir.mkdir(parents=True)
        expected = []
        for name in ("libc++.1.dylib", "libc++abi.1.dylib", "libunwind.1.dylib"):
            real_path = library_dir / name.replace(".1.dylib", ".1.0.dylib")
            real_path.write_bytes(b"runtime")
            alias = library_dir / name
            alias.symlink_to(real_path.name)
            expected.append(str(real_path.resolve()))
        dyld = self.root / "dyld-libcxxabi.txt"
        active_trace = (
            "\n".join(f"dyld[1]: {path}" for path in expected)
            + "\n"
            + "dyld[1]: /usr/lib/libc++abi.dylib\n"
        )
        command = [
            sys.executable,
            "-c",
            match.group(1),
            str(dyld),
            str(library_dir),
        ]
        dyld.write_text(active_trace, encoding="utf-8")
        completed = subprocess.run(
            command, capture_output=True, text=True, check=False
        )
        self.assertEqual(completed.returncode, 0, completed.stderr)

        dyld.write_text(
            active_trace
            + "dyld[1]: move loaded to delayed: libc++abi.dylib\n",
            encoding="utf-8",
        )
        completed = subprocess.run(
            command, capture_output=True, text=True, check=False
        )
        self.assertEqual(completed.returncode, 0, completed.stderr)

        dyld.write_text(
            dyld.read_text(encoding="utf-8")
            + "dyld[1]: move delayed to loaded: libc++abi.dylib\n",
            encoding="utf-8",
        )
        completed = subprocess.run(
            command, capture_output=True, text=True, check=False
        )
        self.assertEqual(completed.returncode, 0, completed.stderr)

        dyld.write_text(
            "\n".join(
                [
                    f"dyld[1]: {expected[0]}",
                    f"dyld[1]: {expected[2]}",
                    "dyld[1]: /usr/lib/libc++abi.dylib",
                    "dyld[1]: move loaded to delayed: libc++abi.dylib",
                ]
            )
            + "\n",
            encoding="utf-8",
        )
        rejected = subprocess.run(command, capture_output=True, text=True, check=False)
        self.assertNotEqual(rejected.returncode, 0)
        self.assertIn(
            "active runtime libc++abi is not the archive library",
            rejected.stderr,
        )

        dyld.write_text(
            "\n".join(f"dyld[1]: {path}" for path in expected)
            + "\n"
            + "dyld[1]: move loaded to delayed: libc++abi.1.0.dylib\n",
            encoding="utf-8",
        )
        rejected = subprocess.run(command, capture_output=True, text=True, check=False)
        self.assertNotEqual(rejected.returncode, 0)
        self.assertIn("archive runtime became delayed", rejected.stderr)

    def test_macos_runtime_load_validator_rejects_invalid_active_graph(self):
        verify_script = (
            ROOT / ".github/scripts/verify-p2996-toolchain.sh"
        ).read_text(encoding="utf-8")
        match = re.search(
            r"# BEGIN MACOS RUNTIME LOAD VALIDATOR\n(.*?)\n"
            r"# END MACOS RUNTIME LOAD VALIDATOR",
            verify_script,
            re.DOTALL,
        )
        self.assertIsNotNone(match, "runtime validator must be executable")
        library_dir = self.root / "toolchain root/lib"
        library_dir.mkdir(parents=True)
        archive_records = []
        for name in ("libc++.1.dylib", "libc++abi.1.dylib", "libunwind.1.dylib"):
            real_path = library_dir / name.replace(".1.dylib", ".1.0.dylib")
            real_path.write_bytes(b"runtime")
            (library_dir / name).symlink_to(real_path.name)
            archive_records.append(f"dyld[7]: {real_path.resolve()}")
        system_record = "dyld[7]: /usr/lib/libc++.1.dylib"
        system_delay = "dyld[7]: move loaded to delayed: libc++.1.dylib"
        dyld = self.root / "invalid-active-graph.txt"
        command = [
            sys.executable,
            "-c",
            match.group(1),
            str(dyld),
            str(library_dir),
        ]

        cases = {
            "system transition precedes image": archive_records
            + [system_delay, system_record],
            "duplicate delay transition": archive_records
            + [system_record, system_delay, system_delay],
            "archive becomes delayed": archive_records
            + [
                "dyld[7]: move loaded to delayed: libc++.1.0.dylib",
            ],
            "host libcxxabi": archive_records
            + ["dyld[7]: /usr/lib/libc++abi.1.dylib"],
            "multiple pids": archive_records + ["dyld[8]: /usr/lib/libSystem.B.dylib"],
            "duplicate image": archive_records + [archive_records[0]],
            "weak-def system runtime participates": archive_records
            + [
                system_record,
                "dyld[7]: libc++.1.dylib has weak-def (or flat lookup) "
                "symbol used by platform-probe, so cannot be delayed",
            ],
            "interposing system runtime participates": archive_records
            + [
                system_record,
                "dyld[7]: has interposing tuples so cannot be delayed: "
                "libc++.1.dylib",
            ],
            "weak-def system runtime cannot become delayed": archive_records
            + [
                system_record,
                "dyld[7]: libc++.1.dylib has weak-def (or flat lookup) "
                "symbol used by platform-probe, so cannot be delayed",
                system_delay,
            ],
            "interposing system runtime cannot become delayed": archive_records
            + [
                system_record,
                "dyld[7]: has interposing tuples so cannot be delayed: "
                "libc++.1.dylib",
                system_delay,
            ],
        }
        for name, lines in cases.items():
            with self.subTest(name=name):
                dyld.write_text("\n".join(lines) + "\n", encoding="utf-8")
                rejected = subprocess.run(
                    command, capture_output=True, text=True, check=False
                )
                self.assertNotEqual(rejected.returncode, 0, rejected.stdout)

        accepted = archive_records + [
            system_record,
            "dyld[7]: libc++.1.0.dylib has weak-def (or flat lookup) "
            "symbol used by platform-probe, so cannot be delayed",
            "dyld[7]: has interposing tuples so cannot be delayed: XPCSupport",
            "dyld[7]: move loaded to delayed: XPCSupport",
            "dyld[7]: move delayed to loaded: XPCSupport",
            system_delay,
        ]
        dyld.write_text("\n".join(accepted) + "\n", encoding="utf-8")
        completed = subprocess.run(
            command, capture_output=True, text=True, check=False
        )
        self.assertEqual(completed.returncode, 0, completed.stderr)

    def test_macos_runtime_link_chain_validator_is_exact_and_two_level(self):
        verify_script = (
            ROOT / ".github/scripts/verify-p2996-toolchain.sh"
        ).read_text(encoding="utf-8")
        match = re.search(
            r"# BEGIN MACOS RUNTIME LINK CHAIN VALIDATOR\n(.*?)\n"
            r"# END MACOS RUNTIME LINK CHAIN VALIDATOR",
            verify_script,
            re.DOTALL,
        )
        self.assertIsNotNone(match, "runtime link-chain validator must be executable")

        reports = {
            "probe": [
                ("LC_LOAD_DYLIB", "@rpath/libc++.1.dylib"),
                ("LC_LOAD_DYLIB", "@rpath/libunwind.1.dylib"),
                ("LC_LOAD_DYLIB", "/usr/lib/libSystem.B.dylib"),
            ],
            "libcxx": [
                ("LC_ID_DYLIB", "@rpath/libc++.1.dylib"),
                ("LC_REEXPORT_DYLIB", "@rpath/libc++abi.1.dylib"),
                ("LC_LOAD_DYLIB", "@rpath/libunwind.1.dylib"),
                ("LC_LOAD_DYLIB", "/usr/lib/libSystem.B.dylib"),
            ],
            "libcxxabi": [
                ("LC_ID_DYLIB", "@rpath/libc++abi.1.dylib"),
                ("LC_REEXPORT_DYLIB", "@rpath/libunwind.1.dylib"),
                ("LC_LOAD_DYLIB", "/usr/lib/libSystem.B.dylib"),
            ],
            "libunwind": [
                ("LC_ID_DYLIB", "@rpath/libunwind.1.dylib"),
                ("LC_LOAD_DYLIB", "/usr/lib/libSystem.B.dylib"),
            ],
        }
        load_paths = {name: self.root / f"{name}.loads.txt" for name in reports}
        header_paths = {
            name: self.root / f"{name}.header.txt" for name in reports
        }

        def write_load_report(name, commands):
            load_paths[name].write_text(
                "".join(
                    f"Load command {index}\n"
                    f"          cmd {command}\n"
                    "      cmdsize 56\n"
                    f"         name {install_name} (offset 24)\n"
                    "   time stamp 2 Thu Jan  1 08:00:02 1970\n"
                    "      current version 1.0.0\n"
                    "compatibility version 1.0.0\n"
                    for index, (command, install_name) in enumerate(commands)
                ),
                encoding="utf-8",
            )

        def validate(
            overrides=None,
            flags=None,
            filetypes=None,
            raw_headers=None,
        ):
            values = {name: list(entries) for name, entries in reports.items()}
            values.update(overrides or {})
            header_flags = {
                name: "NOUNDEFS DYLDLINK TWOLEVEL PIE" for name in reports
            }
            header_flags.update(flags or {})
            header_filetypes = {
                "probe": "EXECUTE",
                "libcxx": "DYLIB",
                "libcxxabi": "DYLIB",
                "libunwind": "DYLIB",
            }
            header_filetypes.update(filetypes or {})
            for name, commands in values.items():
                write_load_report(name, commands)
                if raw_headers and name in raw_headers:
                    header_text = raw_headers[name]
                else:
                    header_text = (
                        f"/tmp/{name}:\n"
                        "Mach header\n"
                        "      magic cputype cpusubtype caps filetype ncmds "
                        "sizeofcmds flags\n"
                        f"MH_MAGIC_64 ARM64 ALL 0x00 "
                        f"{header_filetypes[name]} 20 2048 "
                        f"{header_flags[name]}\n"
                    )
                header_paths[name].write_text(header_text, encoding="utf-8")
            return subprocess.run(
                [
                    sys.executable,
                    "-c",
                    match.group(1),
                    *(str(load_paths[name]) for name in reports),
                    *(str(header_paths[name]) for name in reports),
                ],
                capture_output=True,
                text=True,
                check=False,
            )

        completed = validate()
        self.assertEqual(completed.returncode, 0, completed.stderr)

        invalid_cases = {
            "host probe runtime": {
                "probe": [
                    ("LC_LOAD_DYLIB", "@rpath/libc++.1.dylib"),
                    ("LC_LOAD_DYLIB", "/usr/lib/libc++.1.dylib"),
                    ("LC_LOAD_DYLIB", "/usr/lib/libSystem.B.dylib"),
                ]
            },
            "suffix spoof": {
                "probe": [
                    ("LC_LOAD_DYLIB", "@rpath/libc++.1.dylib"),
                    ("LC_LOAD_DYLIB", "@rpath/libc++.1.dylib.evil"),
                    ("LC_LOAD_DYLIB", "/usr/lib/libSystem.B.dylib"),
                ]
            },
            "wrong libcxx id": {
                "libcxx": [
                    ("LC_ID_DYLIB", "/usr/lib/libc++.1.dylib"),
                    ("LC_REEXPORT_DYLIB", "@rpath/libc++abi.1.dylib"),
                    ("LC_LOAD_DYLIB", "/usr/lib/libSystem.B.dylib"),
                ]
            },
            "weak libcxx runtime edge": {
                "libcxx": [
                    ("LC_ID_DYLIB", "@rpath/libc++.1.dylib"),
                    ("LC_LOAD_WEAK_DYLIB", "@rpath/libc++abi.1.dylib"),
                    ("LC_LOAD_DYLIB", "/usr/lib/libSystem.B.dylib"),
                ]
            },
            "weak direct libcxx unwind edge": {
                "libcxx": [
                    ("LC_ID_DYLIB", "@rpath/libc++.1.dylib"),
                    ("LC_REEXPORT_DYLIB", "@rpath/libc++abi.1.dylib"),
                    ("LC_LOAD_WEAK_DYLIB", "@rpath/libunwind.1.dylib"),
                    ("LC_LOAD_DYLIB", "/usr/lib/libSystem.B.dylib"),
                ]
            },
            "host direct libcxx unwind edge": {
                "libcxx": [
                    ("LC_ID_DYLIB", "@rpath/libc++.1.dylib"),
                    ("LC_REEXPORT_DYLIB", "@rpath/libc++abi.1.dylib"),
                    ("LC_LOAD_DYLIB", "/usr/lib/libunwind.1.dylib"),
                    ("LC_LOAD_DYLIB", "/usr/lib/libSystem.B.dylib"),
                ]
            },
            "missing libcxxabi edge": {
                "libcxx": [
                    ("LC_ID_DYLIB", "@rpath/libc++.1.dylib"),
                    ("LC_LOAD_DYLIB", "@rpath/libunwind.1.dylib"),
                    ("LC_LOAD_DYLIB", "/usr/lib/libSystem.B.dylib"),
                ]
            },
            "missing direct libcxx unwind edge": {
                "libcxx": [
                    ("LC_ID_DYLIB", "@rpath/libc++.1.dylib"),
                    ("LC_REEXPORT_DYLIB", "@rpath/libc++abi.1.dylib"),
                    ("LC_LOAD_DYLIB", "/usr/lib/libSystem.B.dylib"),
                ]
            },
            "duplicate probe runtime edge": {
                "probe": [
                    ("LC_LOAD_DYLIB", "@rpath/libc++.1.dylib"),
                    ("LC_LOAD_DYLIB", "@rpath/libc++.1.dylib"),
                    ("LC_LOAD_DYLIB", "/usr/lib/libSystem.B.dylib"),
                ]
            },
            "probe reexports runtime": {
                "probe": [
                    ("LC_REEXPORT_DYLIB", "@rpath/libc++.1.dylib"),
                    ("LC_LOAD_DYLIB", "/usr/lib/libSystem.B.dylib"),
                ]
            },
        }
        for name, override in invalid_cases.items():
            with self.subTest(name=name):
                rejected = validate(override)
                self.assertNotEqual(rejected.returncode, 0)

        rejected = validate(flags={"probe": "NOUNDEFS DYLDLINK PIE"})
        self.assertNotEqual(rejected.returncode, 0)
        self.assertIn("two-level", rejected.stderr)

        rejected = validate(
            flags={"libcxx": "NOUNDEFS DYLDLINK TWOLEVEL FORCE_FLAT PIE"}
        )
        self.assertNotEqual(rejected.returncode, 0)
        self.assertIn("two-level", rejected.stderr)

        rejected = validate(filetypes={"libcxx": "EXECUTE"})
        self.assertNotEqual(rejected.returncode, 0)
        self.assertIn("Mach-O header", rejected.stderr)

        rejected = validate(
            raw_headers={
                "probe": (
                    "TWOLEVEL filename-spoof:\n"
                    "Mach header\n"
                    "magic cputype cpusubtype caps filetype ncmds sizeofcmds flags\n"
                    "MH_MAGIC_64 ARM64 ALL 0x00 EXECUTE 20 2048 "
                    "NOUNDEFS DYLDLINK PIE\n"
                )
            }
        )
        self.assertNotEqual(rejected.returncode, 0)
        self.assertIn("two-level", rejected.stderr)

        duplicate_header = (
            "/tmp/probe:\n"
            "Mach header\n"
            "magic cputype cpusubtype caps filetype ncmds sizeofcmds flags\n"
            "MH_MAGIC_64 ARM64 ALL 0x00 EXECUTE 20 2048 "
            "NOUNDEFS DYLDLINK TWOLEVEL PIE\n"
        )
        rejected = validate(
            raw_headers={"probe": duplicate_header + duplicate_header}
        )
        self.assertNotEqual(rejected.returncode, 0)
        self.assertIn("Mach-O header", rejected.stderr)

    def test_macos_probe_runtime_audit_uses_a_clean_environment(self):
        verify_script = (
            ROOT / ".github/scripts/verify-p2996-toolchain.sh"
        ).read_text(encoding="utf-8")
        self.assertIn(
            'env -i DYLD_PRINT_LIBRARIES=1 "${probe_binary}"', verify_script
        )

    def test_macos_runtime_load_validator_accepts_dyld4_uuid_records(self):
        verify_script = (
            ROOT / ".github/scripts/verify-p2996-toolchain.sh"
        ).read_text(encoding="utf-8")
        match = re.search(
            r"# BEGIN MACOS RUNTIME LOAD VALIDATOR\n(.*?)\n"
            r"# END MACOS RUNTIME LOAD VALIDATOR",
            verify_script,
            re.DOTALL,
        )
        self.assertIsNotNone(match, "runtime validator must be executable")
        library_dir = self.root / "toolchain root/lib"
        library_dir.mkdir(parents=True)
        records = []
        uuids = (
            "01234567-89AB-CDEF-0123-456789ABCDEF",
            "12345678-9ABC-DEF0-1234-56789ABCDEF0",
            "23456789-ABCD-EF01-2345-6789ABCDEF01",
        )
        for uuid, name in zip(
            uuids, ("libc++.1.dylib", "libc++abi.1.dylib", "libunwind.1.dylib")
        ):
            path = library_dir / name
            path.write_bytes(b"runtime")
            records.append(f"dyld[42]: <{uuid}> {path.resolve()}")
        dyld = self.root / "dyld4.txt"
        dyld.write_text("\n".join(records) + "\n", encoding="utf-8")
        completed = subprocess.run(
            [sys.executable, "-c", match.group(1), str(dyld), str(library_dir)],
            capture_output=True,
            text=True,
            check=False,
        )
        self.assertEqual(completed.returncode, 0, completed.stderr)

    def test_macos_runtime_load_validator_accepts_libsystem_unwind_baseline(self):
        verify_script = (
            ROOT / ".github/scripts/verify-p2996-toolchain.sh"
        ).read_text(encoding="utf-8")
        match = re.search(
            r"# BEGIN MACOS RUNTIME LOAD VALIDATOR\n(.*?)\n"
            r"# END MACOS RUNTIME LOAD VALIDATOR",
            verify_script,
            re.DOTALL,
        )
        self.assertIsNotNone(match, "runtime validator must be executable")
        library_dir = self.root / "toolchain root/lib"
        library_dir.mkdir(parents=True)
        archive_records = []
        for name in ("libc++.1.dylib", "libc++abi.1.dylib", "libunwind.1.dylib"):
            path = library_dir / name
            path.write_bytes(b"runtime")
            archive_records.append(f"dyld[42]: {path.resolve()}")
        system_unwind = "dyld[42]: /usr/lib/system/libunwind.dylib"
        dyld = self.root / "dyld4-libsystem-unwind.txt"
        command = [
            sys.executable,
            "-c",
            match.group(1),
            str(dyld),
            str(library_dir),
        ]

        dyld.write_text(
            "\n".join([*archive_records, system_unwind]) + "\n",
            encoding="utf-8",
        )
        completed = subprocess.run(
            command,
            capture_output=True,
            text=True,
            check=False,
        )
        self.assertEqual(completed.returncode, 0, completed.stderr)

        dyld.write_text(
            "\n".join([*archive_records[:-1], system_unwind]) + "\n",
            encoding="utf-8",
        )
        rejected = subprocess.run(
            command,
            capture_output=True,
            text=True,
            check=False,
        )
        self.assertNotEqual(rejected.returncode, 0)
        self.assertIn(
            "active runtime libunwind is not the archive library",
            rejected.stderr,
        )

        dyld.write_text(
            "\n".join(
                [*archive_records, "dyld[42]: /usr/lib/libunwind.1.dylib"]
            )
            + "\n",
            encoding="utf-8",
        )
        rejected = subprocess.run(
            command,
            capture_output=True,
            text=True,
            check=False,
        )
        self.assertNotEqual(rejected.returncode, 0)
        self.assertIn(
            "unexpected runtime library path: /usr/lib/libunwind.1.dylib",
            rejected.stderr,
        )

    def test_macos_runtime_load_validator_accepts_dyld_delay_status_only(self):
        verify_script = (
            ROOT / ".github/scripts/verify-p2996-toolchain.sh"
        ).read_text(encoding="utf-8")
        match = re.search(
            r"# BEGIN MACOS RUNTIME LOAD VALIDATOR\n(.*?)\n"
            r"# END MACOS RUNTIME LOAD VALIDATOR",
            verify_script,
            re.DOTALL,
        )
        self.assertIsNotNone(match, "runtime validator must be executable")
        library_dir = self.root / "toolchain root/lib"
        library_dir.mkdir(parents=True)
        records = []
        for name in ("libc++.1.dylib", "libc++abi.1.dylib", "libunwind.1.dylib"):
            real_path = library_dir / name.replace(".1.dylib", ".1.0.dylib")
            real_path.write_bytes(b"runtime")
            (library_dir / name).symlink_to(real_path.name)
            records.append(f"dyld[42]: {real_path.resolve()}")
        records.append("dyld[42]: move loaded to delayed: XPCSupport")
        dyld = self.root / "dyld4-delay-status.txt"
        dyld.write_text("\n".join(records) + "\n", encoding="utf-8")
        command = [
            sys.executable,
            "-c",
            match.group(1),
            str(dyld),
            str(library_dir),
        ]
        completed = subprocess.run(
            command,
            capture_output=True,
            text=True,
            check=False,
        )
        self.assertEqual(completed.returncode, 0, completed.stderr)

        dyld.write_text(
            dyld.read_text(encoding="utf-8")
            + "dyld[42]: arbitrary status: XPCSupport\n",
            encoding="utf-8",
        )
        rejected = subprocess.run(
            command,
            capture_output=True,
            text=True,
            check=False,
        )
        self.assertNotEqual(rejected.returncode, 0)
        self.assertIn("malformed dyld library record", rejected.stderr)

        dyld.write_text(
            "\n".join(records) + "\n"
            + "dyld[42]: move loaded to delayed: /usr/lib/libc++.1.dylib\n",
            encoding="utf-8",
        )
        rejected = subprocess.run(
            command,
            capture_output=True,
            text=True,
            check=False,
        )
        self.assertNotEqual(rejected.returncode, 0)
        self.assertIn("malformed dyld library record", rejected.stderr)

    def test_macos_runtime_load_validator_accepts_known_shared_cache_runtimes(self):
        verify_script = (
            ROOT / ".github/scripts/verify-p2996-toolchain.sh"
        ).read_text(encoding="utf-8")
        match = re.search(
            r"# BEGIN MACOS RUNTIME LOAD VALIDATOR\n(.*?)\n"
            r"# END MACOS RUNTIME LOAD VALIDATOR",
            verify_script,
            re.DOTALL,
        )
        self.assertIsNotNone(match, "runtime validator must be executable")
        library_dir = self.root / "toolchain root/lib"
        library_dir.mkdir(parents=True)
        records = []
        for name in ("libc++.1.dylib", "libc++abi.1.dylib", "libunwind.1.dylib"):
            real_path = library_dir / name.replace(".1.dylib", ".1.0.dylib")
            real_path.write_bytes(b"runtime")
            (library_dir / name).symlink_to(real_path.name)
            records.append(f"dyld[42]: {real_path.resolve()}")
        records.append(
            "dyld[42]: <3456789A-BCDE-F012-3456-789ABCDEF012> "
            "/usr/lib/libc++.1.dylib"
        )
        records.append(
            "dyld[42]: <456789AB-CDEF-0123-4567-89ABCDEF0123> "
            "/usr/lib/libc++abi.dylib"
        )
        dyld = self.root / "dyld4-mixed.txt"
        dyld.write_text("\n".join(records) + "\n", encoding="utf-8")
        completed = subprocess.run(
            [sys.executable, "-c", match.group(1), str(dyld), str(library_dir)],
            capture_output=True,
            text=True,
            check=False,
        )
        self.assertEqual(completed.returncode, 0, completed.stderr)

    def test_macos_runtime_origin_validator_requires_archive_symbol_origins(self):
        verify_script = (
            ROOT / ".github/scripts/verify-p2996-toolchain.sh"
        ).read_text(encoding="utf-8")
        match = re.search(
            r"# BEGIN MACOS RUNTIME ORIGIN VALIDATOR\n(.*?)\n"
            r"# END MACOS RUNTIME ORIGIN VALIDATOR",
            verify_script,
            re.DOTALL,
        )
        self.assertIsNotNone(match, "runtime origin validator must be executable")
        library_dir = self.root / "toolchain root/lib"
        library_dir.mkdir(parents=True)
        origins = []
        for label, name in (
            ("libc++", "libc++.1.dylib"),
            ("libc++abi", "libc++abi.1.dylib"),
            ("libunwind", "libunwind.1.dylib"),
        ):
            real_path = library_dir / name.replace(".1.dylib", ".1.0.dylib")
            real_path.write_bytes(b"runtime")
            (library_dir / name).symlink_to(real_path.name)
            origins.append(f"{label}\t{real_path.resolve()}")
        origin_output = self.root / "runtime-origins.txt"
        origin_output.write_text("\n".join(origins) + "\n", encoding="utf-8")
        command = [
            sys.executable,
            "-c",
            match.group(1),
            str(origin_output),
            str(library_dir),
        ]
        completed = subprocess.run(
            command,
            capture_output=True,
            text=True,
            check=False,
        )
        self.assertEqual(completed.returncode, 0, completed.stderr)

        host_directory = self.root / "host"
        host_directory.mkdir()
        for index, origin in enumerate(origins):
            label = origin.split("\t", 1)[0]
            with self.subTest(host_origin=label):
                host_runtime = host_directory / f"{label}-{index}.dylib"
                host_runtime.write_bytes(b"host runtime")
                changed = list(origins)
                changed[index] = f"{label}\t{host_runtime.resolve()}"
                origin_output.write_text(
                    "\n".join(changed) + "\n",
                    encoding="utf-8",
                )
                rejected = subprocess.run(
                    command,
                    capture_output=True,
                    text=True,
                    check=False,
                )
                self.assertNotEqual(rejected.returncode, 0)
                self.assertIn("runtime symbol origin mismatch", rejected.stderr)

        origin_output.write_text(
            "\n".join([*origins, origins[0]]) + "\n",
            encoding="utf-8",
        )
        rejected = subprocess.run(
            command,
            capture_output=True,
            text=True,
            check=False,
        )
        self.assertNotEqual(rejected.returncode, 0)
        self.assertIn("runtime origin report labels", rejected.stderr)

    def test_macos_runtime_origin_probe_is_compiled_and_run_clean(self):
        verify_script = (
            ROOT / ".github/scripts/verify-p2996-toolchain.sh"
        ).read_text(encoding="utf-8")
        probe_source = (
            ROOT / ".github/scripts/macos-runtime-origin-probe.cpp"
        ).read_text(encoding="utf-8")
        self.assertIn('print_origin("libc++", &std::cout)', probe_source)
        self.assertIn("&__cxxabiv1::__cxa_demangle", probe_source)
        self.assertIn("&_Unwind_GetIP", probe_source)
        compile_marker = '"${script_dir}/macos-runtime-origin-probe.cpp"'
        run_marker = 'env -i "${runtime_origin_binary}" >"${runtime_origin_output}"'
        validator_marker = "# BEGIN MACOS RUNTIME ORIGIN VALIDATOR"
        self.assertIn(compile_marker, verify_script)
        self.assertIn(run_marker, verify_script)
        self.assertLess(verify_script.index(compile_marker), verify_script.index(run_marker))
        self.assertLess(verify_script.index(run_marker), verify_script.index(validator_marker))

    def test_macos_runtime_load_validator_rejects_malformed_dyld4_record(self):
        verify_script = (
            ROOT / ".github/scripts/verify-p2996-toolchain.sh"
        ).read_text(encoding="utf-8")
        match = re.search(
            r"# BEGIN MACOS RUNTIME LOAD VALIDATOR\n(.*?)\n"
            r"# END MACOS RUNTIME LOAD VALIDATOR",
            verify_script,
            re.DOTALL,
        )
        self.assertIsNotNone(match, "runtime validator must be executable")
        library_dir = self.root / "toolchain root/lib"
        library_dir.mkdir(parents=True)
        records = []
        for name in ("libc++.1.dylib", "libc++abi.1.dylib", "libunwind.1.dylib"):
            path = library_dir / name
            path.write_bytes(b"runtime")
            records.append(f"dyld[42]: {path.resolve()}")
        records.append("dyld[42]: <not-a-uuid> /usr/lib/libc++.1.dylib")
        dyld = self.root / "dyld4-malformed.txt"
        dyld.write_text("\n".join(records) + "\n", encoding="utf-8")
        rejected = subprocess.run(
            [sys.executable, "-c", match.group(1), str(dyld), str(library_dir)],
            capture_output=True,
            text=True,
            check=False,
        )
        self.assertNotEqual(rejected.returncode, 0)
        self.assertIn("malformed dyld library record", rejected.stderr)

    def test_linux_p2996_runtime_load_validator_rejects_mixed_libraries(self):
        dockerfile = (ROOT / ".github/docker/Dockerfile.p2996").read_text(
            encoding="utf-8"
        )
        match = re.search(
            r"# BEGIN LINUX P2996 RUNTIME LOAD VALIDATOR\n(.*?)\n"
            r"# END LINUX P2996 RUNTIME LOAD VALIDATOR",
            dockerfile,
            re.DOTALL,
        )
        self.assertIsNotNone(match, "Linux runtime validator must be executable")
        runtime_dir = self.root / "p2996 runtime"
        runtime_dir.mkdir()
        records = []
        for name in ("libc++.so.1", "libc++abi.so.1", "libunwind.so.1"):
            path = runtime_dir / name
            path.write_bytes(b"runtime")
            records.append(f"{name} => {path.resolve()} (0x1)")
        ldd_output = self.root / "probe.ldd"
        ldd_output.write_text("\n".join(records) + "\n", encoding="utf-8")
        command = [
            sys.executable,
            "-c",
            match.group(1),
            str(ldd_output),
            str(runtime_dir),
        ]
        self.assertEqual(subprocess.run(command, check=False).returncode, 0)
        ldd_output.write_text(
            ldd_output.read_text(encoding="utf-8")
            + "libc++.so.2 => /usr/lib/libc++.so.2 (0x2)\n",
            encoding="utf-8",
        )
        rejected = subprocess.run(command, capture_output=True, text=True, check=False)
        self.assertNotEqual(rejected.returncode, 0)
        self.assertIn("exactly once", rejected.stderr)

    def test_macos_rpath_validator_accepts_resolved_archive_path_only(self):
        verify_script = (
            ROOT / ".github/scripts/verify-p2996-toolchain.sh"
        ).read_text(encoding="utf-8")
        match = re.search(
            r"# BEGIN MACOS RPATH VALIDATOR\n(.*?)\n"
            r"# END MACOS RPATH VALIDATOR",
            verify_script,
            re.DOTALL,
        )
        self.assertIsNotNone(match, "rpath validator must be executable")
        library_dir = self.root / "toolchain root/lib"
        library_dir.mkdir(parents=True)
        report = self.root / "rpaths.txt"

        def validate(path):
            report.write_text(
                "Load command 1\n"
                "          cmd LC_RPATH\n"
                "      cmdsize 80\n"
                f"         path {path} (offset 12)\n",
                encoding="utf-8",
            )
            return subprocess.run(
                [
                    sys.executable,
                    "-c",
                    match.group(1),
                    str(report),
                    str(library_dir),
                ],
                capture_output=True,
                text=True,
                check=False,
            )

        completed = validate(library_dir.resolve())
        self.assertEqual(completed.returncode, 0, completed.stderr)

        redundant_separator = (
            str(library_dir.resolve().parent)
            + os.sep
            + os.sep
            + library_dir.name
        )
        completed = validate(redundant_separator)
        self.assertEqual(completed.returncode, 0, completed.stderr)

        outside = self.root / "host lib"
        outside.mkdir()
        completed = validate(outside.resolve())
        self.assertNotEqual(completed.returncode, 0)

    def test_macos_platform_probe_validator_requires_all_gates_and_admissions(self):
        verify_script = (
            ROOT / ".github/scripts/verify-p2996-toolchain.sh"
        ).read_text(encoding="utf-8")
        match = re.search(
            r"# BEGIN MACOS PLATFORM PROBE VALIDATOR\n(.*?)\n"
            r"# END MACOS PLATFORM PROBE VALIDATOR",
            verify_script,
            re.DOTALL,
        )
        self.assertIsNotNone(match, "platform probe validator must be executable")
        probe = {
            "schema": 1,
            "node": "arm64_macos_clang",
            "probe": {
                "char_bit": 8,
                "pointer_bits": 64,
                "endian": "little",
                "reflection": True,
                "memcpy_object_lifetime": True,
                "memcpy_array_lifetime": True,
            },
            "admission": {
                key: True
                for key in (
                    "WorldSnapshot",
                    "Entity",
                    "EntityRelativePtr",
                    "EntityIndexEntry",
                )
            },
            "compiler": {
                "family": "clang",
                "revision": CLANG_COMMIT,
                "version": "clang version 21.0.0",
                "target": "arm64-apple-macosx15.0.0",
                "stdlib": "libc++-210000",
                "xcode_version": "16.4",
                "xcode_build": "16F6",
                "sdk_version": "15.5",
                "sdk_build": "24F74",
                "deployment_target": "15.0",
                "sdk_locked": True,
            },
            "environment": {"runner": "macos-15", "runner_image": "fixture"},
        }
        probe_path = self.root / "probe.json"
        arguments = [
            sys.executable,
            "-c",
            match.group(1),
            str(ROOT),
            str(probe_path),
            "arm64_macos_clang",
            "macos-15",
            CLANG_COMMIT,
            "arm64-apple-macosx15.0.0",
            "16.4",
            "16F6",
            "15.5",
            "24F74",
            "15.0",
            "true",
        ]
        write_json(probe_path, probe)
        self.assertEqual(subprocess.run(arguments, check=False).returncode, 0)
        for section, key in (("probe", "reflection"), ("admission", "Entity")):
            with self.subTest(section=section, key=key):
                mutated = json.loads(json.dumps(probe))
                mutated[section][key] = False
                write_json(probe_path, mutated)
                rejected = subprocess.run(
                    arguments, capture_output=True, text=True, check=False
                )
                self.assertNotEqual(rejected.returncode, 0)

    def test_candidate_archive_is_copied_once_to_private_storage(self):
        verify_content = (
            ROOT / ".github/scripts/verify-p2996-toolchain.sh"
        ).read_text(encoding="utf-8")
        self.assertNotIn(
            'archive="$(cd "$(dirname "${candidate_archive}")"', verify_content
        )
        self.assertIn(
            'archive="${temporary_root}/toolchain.tar.zst"', verify_content
        )
        self.assertIn('open(sys.argv[1], "rb") as source', verify_content)
        self.assertIn('open(sys.argv[2], "xb") as destination', verify_content)

    def test_candidate_verifier_reaches_controlled_runtime_failure(self):
        script = self.root / ".github/scripts/verify-p2996-toolchain.sh"
        script.write_bytes(
            (ROOT / ".github/scripts/verify-p2996-toolchain.sh").read_bytes()
        )
        validator = self.root / ".github/scripts/validate-toolchain-locks.py"
        validator.write_bytes(VALIDATOR.read_bytes())
        evidence_module = self.root / "tools/relocatable_world_evidence.py"
        evidence_module.parent.mkdir(parents=True, exist_ok=True)
        evidence_module.write_bytes(
            (ROOT / "tools/relocatable_world_evidence.py").read_bytes()
        )
        sources = self.root / "toolchain-sources.lock"
        write_json(sources, self.make_sources())
        missing_candidate = self.root / "missing-candidate.tar.zst"
        shell_setup = """
xcodebuild() { :; }
xcode-select() { :; }
xcrun() { :; }
zstd() { :; }
shasum() { :; }
otool() { :; }
uname() { printf 'x86_64\n'; }
export -f xcodebuild xcode-select xcrun zstd shasum otool uname
"""
        arguments = [
            bash_path(script),
            "--sources",
            bash_path(sources),
            "--node",
            "x86_64_macos_clang",
            "--candidate-archive",
            bash_path(missing_candidate),
            "--candidate-sha256",
            "0" * 64,
            "--allow-unlocked-sdk",
        ]
        shell_setup += "exec bash " + " ".join(
            shlex.quote(argument) for argument in arguments
        )
        command = ["bash", "-c", shell_setup]
        if os.name == "nt":
            command.insert(0, "wsl")

        completed = subprocess.run(
            command,
            cwd=ROOT,
            capture_output=True,
            text=True,
            check=False,
        )

        self.assertNotEqual(completed.returncode, 0)
        self.assertNotIn("unbound variable", completed.stderr)
        self.assertIn("No such file or directory", completed.stderr)

    def test_macos_verification_receipt_uses_probe_compiler_identity(self):
        verify_content = (
            ROOT / ".github/scripts/verify-p2996-toolchain.sh"
        ).read_text(encoding="utf-8")
        self.assertNotIn('compiler_version="$("${cxx}" --version)"', verify_content)
        for field in ("version", "target", "stdlib"):
            self.assertIn(f'compiler["{field}"]', verify_content)
        self.assertIn('[[ "${probe_target}" == "${target}" ]]', verify_content)

    def test_macos_output_mode_binds_probe_identity_to_output_lock(self):
        verify_content = (
            ROOT / ".github/scripts/verify-p2996-toolchain.sh"
        ).read_text(encoding="utf-8")
        for field in ("compiler_version", "target", "stdlib"):
            self.assertIn(f'output["{field}"]', verify_content)
        for observed, expected in (
            ("compiler_version", "locked_compiler_version"),
            ("probe_target", "locked_compiler_target"),
            ("stdlib", "locked_stdlib"),
        ):
            self.assertIn(
                f'[[ "${{{observed}}}" == "${{{expected}}}" ]]', verify_content
            )

    def test_macos_scripts_have_valid_shell_syntax_and_candidate_mode(self):
        build_script = ROOT / ".github/scripts/build-p2996-macos.sh"
        verify_script = ROOT / ".github/scripts/verify-p2996-toolchain.sh"
        for script in (build_script, verify_script):
            syntax = subprocess.run(
                bash_syntax_command(script),
                cwd=ROOT,
                capture_output=True,
                text=True,
                check=False,
            )
            self.assertEqual(syntax.returncode, 0, syntax.stderr)

        help_result = subprocess.run(
            bash_command(verify_script, "--help"),
            cwd=ROOT,
            capture_output=True,
            text=True,
            check=False,
        )
        self.assertEqual(help_result.returncode, 0, help_result.stderr)
        self.assertIn("--candidate-archive", help_result.stdout)
        self.assertIn("--require-locked-sdk", help_result.stdout)
        self.assertIn("--allow-unlocked-sdk", help_result.stdout)
        verify_content = verify_script.read_text(encoding="utf-8")
        self.assertIn('record["flags"]', verify_content)
        self.assertIn("shlex.split(sys.argv[1])", verify_content)

    def test_macos_available_memory_probe_emits_only_decimal_bytes(self):
        build_content = (
            ROOT / ".github/scripts/build-p2996-macos.sh"
        ).read_text(encoding="utf-8")
        match = re.search(
            r"available_memory=\"\$\(vm_stat \| awk '\n(.*?)\n'\)\"",
            build_content,
            re.DOTALL,
        )
        self.assertIsNotNone(match, "missing vm_stat memory probe")
        with tempfile.TemporaryDirectory() as directory:
            temporary = Path(directory)
            awk_script = temporary / "memory.awk"
            vm_stat = temporary / "vm-stat.txt"
            awk_script.write_text(match.group(1), encoding="utf-8")
            vm_stat.write_text(
                "Mach Virtual Memory Statistics: (page size of 4096 bytes)\n"
                "Pages free:                               2.\n"
                "Pages inactive:                           3.\n"
                "Pages speculative:                        1.\n",
                encoding="utf-8",
            )
            command = ["awk", "-f", str(awk_script), str(vm_stat)]
            if os.name == "nt":
                command = [
                    "wsl",
                    "awk",
                    "-f",
                    bash_path(awk_script),
                    bash_path(vm_stat),
                ]
            completed = subprocess.run(
                command,
                text=True,
                capture_output=True,
                check=False,
            )
        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertEqual(completed.stdout, "24576\n")


if __name__ == "__main__":
    unittest.main()
