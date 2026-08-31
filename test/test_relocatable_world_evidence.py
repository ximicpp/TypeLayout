import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

from tools import relocatable_world_evidence as evidence


class EvidenceTests(unittest.TestCase):
    maxDiff = None

    TOOLCHAIN_ARTIFACTS = {
        "x86_64_linux_gcc": "a" * 64,
        "x86_64_linux_clang": "b" * 64,
        "arm64_linux_gcc": "c" * 64,
        "arm64_linux_clang": "d" * 64,
        "arm64_macos_clang": "e" * 64,
        "x86_64_macos_clang": "f" * 64,
    }

    def setUp(self):
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.directory = Path(self.temporary_directory.name)

    def tearDown(self):
        self.temporary_directory.cleanup()

    @staticmethod
    def write_json(path, value):
        path.write_text(
            json.dumps(value, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )

    @staticmethod
    def sha256(path):
        return hashlib.sha256(path.read_bytes()).hexdigest()

    @staticmethod
    def posix_path(path):
        path = Path(path).resolve()
        if os.name != "nt":
            return str(path)
        drive = path.drive.rstrip(":").lower()
        suffix = path.as_posix()[2:]
        return f"/mnt/{drive}{suffix}"

    @staticmethod
    def write_executable(path, text):
        path.write_bytes(text.encode("utf-8"))
        path.chmod(0o755)

    @staticmethod
    def launcher_text():
        return (
            Path(__file__).resolve().parents[1]
            / "tools"
            / "run-relocatable-world.sh"
        ).read_text(encoding="utf-8")

    @classmethod
    def launcher_python_block(cls, name):
        text = cls.launcher_text()
        start_marker = f"# BEGIN TASK7 {name}"
        end_marker = f"# END TASK7 {name}"
        start = text.index(start_marker) + len(start_marker)
        end = text.index(end_marker, start)
        return text[start:end].strip() + "\n"

    def make_launcher_repository(self):
        source_repository = Path(__file__).resolve().parents[1]
        repository = self.directory / "launcher repository"
        fake_bin = self.directory / "launcher fake bin"
        repository.mkdir()
        fake_bin.mkdir()

        for relative in (
            "tools/relocatable_world_evidence.py",
            "tools/run-relocatable-world.sh",
        ):
            destination = repository / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source_repository / relative, destination)

        sources_lock, outputs_lock = self.make_lock_files()
        lock_directory = repository / ".github" / "docker"
        lock_directory.mkdir(parents=True)
        shutil.copy2(sources_lock, lock_directory / "toolchain-sources.lock")
        shutil.copy2(outputs_lock, lock_directory / "toolchains.lock")

        validator = repository / ".github" / "scripts" / "validate-toolchain-locks.py"
        verifier = repository / ".github" / "scripts" / "verify-p2996-toolchain.sh"
        runtime_probe = (
            repository / ".github" / "scripts" / "macos-runtime-origin-probe.cpp"
        )
        validator.parent.mkdir(parents=True)
        runtime_probe.write_text("// launcher fixture\n", encoding="utf-8")
        self.write_executable(
            validator,
            "#!/usr/bin/env python3\n"
            "import pathlib, sys\n"
            "if '--outputs' not in sys.argv:\n"
            "    raise SystemExit('output lock is required')\n"
            "for option in ('--sources', '--outputs'):\n"
            "    path = pathlib.Path(sys.argv[sys.argv.index(option) + 1])\n"
            "    if not path.is_file():\n"
            "        raise SystemExit(f'missing lock: {path}')\n",
        )
        self.write_executable(
            verifier,
            "#!/usr/bin/env bash\n"
            "printf 'verify %s\\n' \"$*\" >>\"${TYPELAYOUT_FAKE_TOOL_LOG:?}\"\n"
            "if [[ \"${TYPELAYOUT_FAKE_VERIFIER_SUCCEED-}\" != 1 ]]; then\n"
            "  exit 97\n"
            "fi\n"
            "extract=\n"
            "metadata=\n"
            "while (($#)); do\n"
            "  case \"$1\" in\n"
            "    --extract-dir) extract=${2-}; shift 2 ;;\n"
            "    --metadata-output) metadata=${2-}; shift 2 ;;\n"
            "    *) shift ;;\n"
            "  esac\n"
            "done\n"
            "mkdir -p \"${extract}/p2996-toolchain/lib\"\n"
            "python3 - \"${metadata}\" \"${extract}/p2996-toolchain\" <<'PY'\n"
            "import json\n"
            "from pathlib import Path\n"
            "import sys\n"
            "record = {\n"
            "    'schema': 1,\n"
            "    'node': 'arm64_macos_clang',\n"
            "    'architecture': 'arm64',\n"
            "    'archive_sha256': 'e' * 64,\n"
            "    'compiler_revision': "
            "'060be17654102019e14810c3f948ef85a490755f',\n"
            "    'compiler_version': 'Bloomberg clang 21.0.0',\n"
            "    'target': 'arm64-apple-macosx15.0.0',\n"
            "    'stdlib': 'libc++-210000',\n"
            "    'xcode_version': '16.4',\n"
            "    'xcode_build': '16F6',\n"
            "    'sdk_version': '15.5',\n"
            "    'sdk_build': '24F74',\n"
            "    'deployment_target': '15.0',\n"
            "    'sdk_locked': True,\n"
            "    'observed_runner': {'image_os': '', 'image_version': ''},\n"
            "    'environment': {\n"
            "        'developer_dir': "
            "'/Applications/Xcode.app/Contents/Developer',\n"
            "        'sdkroot': '/SDKs/MacOSX.sdk',\n"
            "        'toolchain_root': sys.argv[2],\n"
            "    },\n"
            "    'flags': '-O3 -fstrict-aliasing -stdlib=libc++ "
            "-mmacosx-version-min=15.0',\n"
            "}\n"
            "Path(sys.argv[1]).write_text(\n"
            "    json.dumps(record, indent=2) + '\\n', encoding='utf-8'\n"
            ")\n"
            "PY\n"
            "if [[ \"${TYPELAYOUT_DIRTY_BEFORE_EVIDENCE-}\" == 1 ]]; then\n"
            "  printf '// persistent test modification\\n' >>"
            "include/boost/typelayout/placeholder.hpp\n"
            "fi\n",
        )

        tracked_placeholders = {
            ".gitattributes": (
                ".github/scripts/** text eol=lf\n"
                "tools/*.py text eol=lf\n"
                "tools/*.sh text eol=lf\n"
            ),
            "CMakeLists.txt": "cmake_minimum_required(VERSION 3.25)\n",
            "cmake/placeholder.cmake": "# tracked launcher fixture\n",
            "include/boost/typelayout/placeholder.hpp": "#pragma once\n",
            "include/boost/typelayout.hpp": "#pragma once\n",
            "example/relocatable_world_demo/placeholder.cpp": "int placeholder;\n",
        }
        for relative, text in tracked_placeholders.items():
            destination = repository / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes(text.encode("utf-8"))

        tool_log = self.directory / "launcher-tool.log"
        self.write_executable(
            fake_bin / "uname",
            "#!/usr/bin/env bash\n"
            "case \"${1-}\" in\n"
            "  -s) printf 'Darwin\\n' ;;\n"
            "  -m) printf 'arm64\\n' ;;\n"
            "  *) printf 'Darwin launcher-test\\n' ;;\n"
            "esac\n",
        )
        self.write_executable(
            fake_bin / "dirname",
            "#!/usr/bin/env bash\n"
            "if [[ -n \"${TYPELAYOUT_GHCR_USER-}\" "
            "|| -n \"${TYPELAYOUT_GHCR_TOKEN-}\" "
            "|| -n \"${explicit_token-}\" "
            "|| -n \"${ghcr_token-}\" ]]; then\n"
            "  printf 'dirname inherited credential environment\\n' >>"
            "\"${TYPELAYOUT_FAKE_TOOL_LOG:?}\"\n"
            "  exit 43\n"
            "fi\n"
            "exec /usr/bin/dirname \"$@\"\n",
        )
        self.write_executable(
            fake_bin / "xcode-select",
            "#!/usr/bin/env bash\nprintf '/Applications/Xcode.app/Contents/Developer\\n'\n",
        )
        self.write_executable(
            fake_bin / "xcodebuild",
            "#!/usr/bin/env bash\nprintf 'Xcode 16.4\\nBuild version 16F6\\n'\n",
        )
        self.write_executable(
            fake_bin / "xcrun",
            "#!/usr/bin/env bash\n"
            "case \"$*\" in\n"
            "  *--show-sdk-version*) printf '15.5\\n' ;;\n"
            "  *--show-sdk-build-version*) printf '24F74\\n' ;;\n"
            "  *--show-sdk-path*) printf '/SDKs/MacOSX.sdk\\n' ;;\n"
            "  *) exit 2 ;;\n"
            "esac\n",
        )
        self.write_executable(
            fake_bin / "sw_vers",
            "#!/usr/bin/env bash\n"
            "case \"${1-}\" in\n"
            "  -productVersion) printf '15.5\\n' ;;\n"
            "  -buildVersion) printf '24F74\\n' ;;\n"
            "  *) printf 'ProductName: macOS\\n' ;;\n"
            "esac\n",
        )
        self.write_executable(
            fake_bin / "gh",
            "#!/usr/bin/env bash\nexit 1\n",
        )
        self.write_executable(
            fake_bin / "curl",
            "#!/usr/bin/env bash\n"
            "if [[ -n \"${TYPELAYOUT_GHCR_TOKEN-}\" "
            "|| -n \"${explicit_token-}\" "
            "|| -n \"${ghcr_token-}\" ]]; then\n"
            "  printf 'curl inherited credential environment\\n' >>"
            "\"${TYPELAYOUT_FAKE_TOOL_LOG:?}\"\n"
            "  exit 43\n"
            "fi\n"
            "headers=\nbody=\n"
            "while (($#)); do\n"
            "  case \"$1\" in\n"
            "    --dump-header) headers=${2-}; shift 2 ;;\n"
            "    --output) body=${2-}; shift 2 ;;\n"
            "    *) shift ;;\n"
            "  esac\n"
            "done\n"
            "cat >/dev/null\n"
            "printf 'HTTP/2 200\\r\\nx-oauth-scopes: repo, read:packages\\r\\n\\r\\n' >\"$headers\"\n"
            "printf '{\"login\":\"test-user\"}\\n' >\"$body\"\n",
        )
        self.write_executable(
            fake_bin / "docker",
            "#!/usr/bin/env bash\n"
            "if [[ -n \"${TYPELAYOUT_GHCR_TOKEN-}\" "
            "|| -n \"${explicit_token-}\" "
            "|| -n \"${ghcr_token-}\" ]]; then\n"
            "  printf 'docker inherited credential environment\\n' >>"
            "\"${TYPELAYOUT_FAKE_TOOL_LOG:?}\"\n"
            "  exit 43\n"
            "fi\n"
            "printf 'docker' >>\"${TYPELAYOUT_FAKE_TOOL_LOG:?}\"\n"
            "printf ' %q' \"$@\" >>\"${TYPELAYOUT_FAKE_TOOL_LOG}\"\n"
            "printf '\\n' >>\"${TYPELAYOUT_FAKE_TOOL_LOG}\"\n"
            "case \"${1-}\" in\n"
            "  info) printf 'aarch64\\n' ;;\n"
            "  version) printf 'Docker Desktop test\\n' ;;\n"
            "  login) cat >/dev/null ;;\n"
            "  buildx)\n"
            "    if [[ \"${2-}\" == version ]]; then\n"
            "      printf 'github.com/docker/buildx v0.24.0\\n'\n"
            "      exit 0\n"
            "    fi\n"
            "    case \"$*\" in\n"
            "      *@sha256:1111111111111111111111111111111111111111111111111111111111111111*)\n"
            "        printf '%s\\n' '{\"schemaVersion\":2,\"manifests\":[{\"digest\":\"sha256:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\",\"platform\":{\"os\":\"linux\",\"architecture\":\"amd64\"}},{\"digest\":\"sha256:cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc\",\"platform\":{\"os\":\"linux\",\"architecture\":\"arm64\"}}]}' ;;\n"
            "      *@sha256:3333333333333333333333333333333333333333333333333333333333333333*)\n"
            "        printf '%s\\n' '{\"schemaVersion\":2,\"manifests\":[{\"digest\":\"sha256:bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\",\"platform\":{\"os\":\"linux\",\"architecture\":\"amd64\"}},{\"digest\":\"sha256:dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd\",\"platform\":{\"os\":\"linux\",\"architecture\":\"arm64\"}}]}' ;;\n"
            "      *) exit 31 ;;\n"
            "    esac ;;\n"
            "  pull) : ;;\n"
            "  run)\n"
            "    if [[ \"$*\" == *TYPELAYOUT_SMOKE_NODE=x86_64_linux_clang* "
            "&& \"${TYPELAYOUT_FAIL_SECOND_SMOKE-}\" == 1 ]]; then\n"
            "      exit 32\n"
            "    fi ;;\n"
            "  *) exit 33 ;;\n"
            "esac\n",
        )
        for command in ("cmake", "ninja", "otool", "shasum", "zstd"):
            self.write_executable(
                fake_bin / command,
                "#!/usr/bin/env bash\n"
                f"printf '{command} %s\\n' \"$*\" >>"
                "\"${TYPELAYOUT_FAKE_TOOL_LOG:?}\"\n",
            )

        subprocess.run(
            ["git", "init", "-b", "main"],
            cwd=repository,
            check=True,
            capture_output=True,
            text=True,
        )
        subprocess.run(
            ["git", "config", "user.email", "launcher-test@example.invalid"],
            cwd=repository,
            check=True,
        )
        subprocess.run(
            ["git", "config", "user.name", "Launcher Test"],
            cwd=repository,
            check=True,
        )
        subprocess.run(
            ["git", "-c", "core.autocrlf=false", "add", "--all"],
            cwd=repository,
            check=True,
        )
        subprocess.run(
            [
                "git",
                "update-index",
                "--chmod=+x",
                "tools/run-relocatable-world.sh",
                "tools/relocatable_world_evidence.py",
                ".github/scripts/verify-p2996-toolchain.sh",
            ],
            cwd=repository,
            check=True,
        )
        subprocess.run(
            ["git", "commit", "-m", "launcher fixture"],
            cwd=repository,
            check=True,
            capture_output=True,
            text=True,
        )
        head = subprocess.run(
            ["git", "rev-parse", "HEAD"],
            cwd=repository,
            check=True,
            capture_output=True,
            text=True,
        ).stdout.strip()
        return repository, fake_bin, tool_log, head

    def run_launcher(
        self,
        repository,
        fake_bin,
        tool_log,
        *arguments,
        environment=None,
        xtrace=False,
        allexport=False,
    ):
        repository_posix = self.posix_path(repository)
        fake_bin_posix = self.posix_path(fake_bin)
        launcher_posix = self.posix_path(
            repository / "tools" / "run-relocatable-world.sh"
        )
        launcher_command = shlex.join((launcher_posix, *arguments))
        child_environment = os.environ.copy()
        launcher_environment = {
            "TYPELAYOUT_FAKE_TOOL_LOG": self.posix_path(tool_log),
            "GIT_CONFIG_COUNT": "1",
            "GIT_CONFIG_KEY_0": "safe.directory",
            "GIT_CONFIG_VALUE_0": repository_posix,
        }
        for name in ("TYPELAYOUT_GHCR_USER", "TYPELAYOUT_GHCR_TOKEN"):
            child_environment.pop(name, None)
        if environment:
            launcher_environment.update(environment)
        exports = "; ".join(
            f"export {name}={shlex.quote(value)}"
            for name, value in launcher_environment.items()
        )
        command = (
            f"{exports}; "
            f"export PATH={shlex.quote(fake_bin_posix)}:/usr/local/bin:/usr/bin:/bin; "
            f"cd {shlex.quote(repository_posix)}; "
            f"exec bash {'-a ' if allexport else ''}"
            f"{'-x ' if xtrace else ''}{launcher_command}"
        )
        return subprocess.run(
            ["bash", "-lc", command],
            capture_output=True,
            text=True,
            check=False,
            env=child_environment,
        )

    def make_lock_files(self):
        sources_lock = self.directory / "toolchain-sources.lock"
        self.write_json(
            sources_lock,
            {
                "schema": 1,
                "gcc": {
                    "version": "16.2.0",
                    "compiler_family": "gcc",
                    "compiler_revision": "16.2.0",
                    "flags": "-O3 -fstrict-aliasing",
                    "source": {
                        "url": (
                            "https://ftp.gnu.org/gnu/gcc/gcc-16.2.0/"
                            "gcc-16.2.0.tar.xz"
                        ),
                        "filename": "gcc-16.2.0.tar.xz",
                        "sha512": "1" * 128,
                    },
                    "prerequisites": {
                        "gmp": {
                            "version": "6.3.0",
                            "url": "https://gmplib.org/download/gmp/gmp-6.3.0.tar.xz",
                            "filename": "gmp-6.3.0.tar.xz",
                            "sha512": "2" * 128,
                        },
                        "mpfr": {
                            "version": "4.2.2",
                            "url": "https://www.mpfr.org/mpfr-4.2.2/mpfr-4.2.2.tar.xz",
                            "filename": "mpfr-4.2.2.tar.xz",
                            "sha512": "3" * 128,
                        },
                        "mpc": {
                            "version": "1.3.1",
                            "url": "https://ftp.gnu.org/gnu/mpc/mpc-1.3.1.tar.gz",
                            "filename": "mpc-1.3.1.tar.gz",
                            "sha512": "4" * 128,
                        },
                        "isl": {
                            "version": "0.27",
                            "url": "https://libisl.sourceforge.io/isl-0.27.tar.xz",
                            "filename": "isl-0.27.tar.xz",
                            "sha512": "5" * 128,
                        },
                    },
                    "configure_flags": [
                        "--disable-multilib",
                        "--disable-nls",
                        "--enable-languages=c,c++",
                    ],
                },
                "p2996": {
                    "repository": "https://github.com/bloomberg/clang-p2996.git",
                    "commit": "060be17654102019e14810c3f948ef85a490755f",
                    "compiler_family": "clang",
                    "compiler_revision": (
                        "060be17654102019e14810c3f948ef85a490755f"
                    ),
                    "flags": "-O3 -fstrict-aliasing -stdlib=libc++",
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
                        "gcc_builder": "ubuntu:24.04@sha256:" + "6" * 64,
                        "gcc_runtime": "ubuntu:24.04@sha256:" + "7" * 64,
                        "p2996_builder": "ubuntu:24.04@sha256:" + "8" * 64,
                        "p2996_runtime": "ubuntu:24.04@sha256:" + "9" * 64,
                    },
                    "apt": {
                        "snapshot": "20260827T000000Z",
                        "suites": ["noble", "noble-updates", "noble-security"],
                        "components": ["main", "universe"],
                    },
                    "packages": {
                        "gcc_builder": [
                            "build-essential=12.10ubuntu1",
                            "xz-utils=5.6.1+really5.4.5-1ubuntu0.2",
                        ],
                        "gcc_runtime": ["libstdc++6=16.2.0-1"],
                        "p2996_builder": [
                            "cmake=3.28.3-1build7",
                            "ninja-build=1.11.1-2",
                        ],
                        "p2996_runtime": ["libc6=2.39-0ubuntu8.6"],
                    },
                    "docker": {
                        "runner_images_commit": (
                            "564e58dbe650c507ccba1171f6159c12f26820c8"
                        ),
                        "runners": {
                            "ubuntu-24.04": {
                                "client_version": "27.5.1",
                                "server_version": "27.5.1",
                            },
                            "ubuntu-24.04-arm": {
                                "client_version": "27.5.1",
                                "server_version": "27.5.1",
                            },
                        },
                        "buildx_version": "0.24.0",
                        "buildkit_image": "moby/buildkit@sha256:" + "a" * 64,
                        "dockerfile_frontend": (
                            "docker.io/docker/dockerfile:1.7@sha256:" + "b" * 64
                        ),
                    },
                },
                "macos": {
                    "runner_images_repository": (
                        "https://github.com/actions/runner-images.git"
                    ),
                    "runner_images_commit": (
                        "564e58dbe650c507ccba1171f6159c12f26820c8"
                    ),
                    "nodes": {
                        "arm64_macos_clang": {
                            "runner": "macos-15",
                            "architecture": "arm64",
                            "llvm_target": "AArch64",
                            "flags": (
                                "-O3 -fstrict-aliasing -stdlib=libc++ "
                                "-mmacosx-version-min=15.0"
                            ),
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
                            "flags": (
                                "-O3 -fstrict-aliasing -stdlib=libc++ "
                                "-mmacosx-version-min=15.0"
                            ),
                            "xcode_version": "16.4",
                            "xcode_build": "16F6",
                            "sdk_version": "15.5",
                            "sdk_build": "24F74",
                            "deployment_target": "15.0",
                        },
                    },
                },
                "actions": {
                    "checkout": "1" * 40,
                    "upload_artifact": "2" * 40,
                    "download_artifact": "3" * 40,
                    "docker_login": "4" * 40,
                    "setup_buildx": "5" * 40,
                    "build_push": "6" * 40,
                    "github_release": "7" * 40,
                },
                "recipes": {
                    ".gitattributes": "1" * 64,
                    ".github/docker/Dockerfile.gcc16": "2" * 64,
                    ".github/docker/Dockerfile.p2996": "3" * 64,
                    ".github/docker/docker-bake.hcl": "4" * 64,
                    ".github/scripts/build-p2996-macos.sh": "5" * 64,
                    ".github/scripts/macos-runtime-origin-probe.cpp": "6" * 64,
                    ".github/scripts/verify-p2996-toolchain.sh": "7" * 64,
                    ".github/workflows/toolchain-images.yml": "8" * 64,
                },
            },
        )

        sources_digest = self.sha256(sources_lock)
        release_url = (
            "https://github.com/ximicpp/TypeLayout/releases/download/"
            f"typelayout-toolchains-{sources_digest}"
        )
        clang_revision = "060be17654102019e14810c3f948ef85a490755f"
        outputs_lock = self.directory / "toolchains.lock"
        self.write_json(
            outputs_lock,
            {
                "schema": 1,
                "sources_sha256": sources_digest,
                "source_sha": "2" * 40,
                "workflow_run": "987654321.1",
                "linux": {
                    "gcc": {
                        "repository": "ghcr.io/ximicpp/typelayout-gcc16",
                        "index_digest": "sha256:" + "1" * 64,
                        "compiler_revision": "16.2.0",
                        "compiler_version": "16.2.0",
                        "stdlib": "libstdc++-20260801",
                        "platforms": {
                            "linux/amd64": {
                                "manifest_digest": "sha256:" + "a" * 64,
                                "target": "x86_64-unknown-linux-gnu",
                            },
                            "linux/arm64": {
                                "manifest_digest": "sha256:" + "c" * 64,
                                "target": "aarch64-unknown-linux-gnu",
                            },
                        },
                    },
                    "p2996": {
                        "repository": "ghcr.io/ximicpp/typelayout-p2996",
                        "index_digest": "sha256:" + "3" * 64,
                        "compiler_revision": (
                            "060be17654102019e14810c3f948ef85a490755f"
                        ),
                        "compiler_version": "Bloomberg clang 21.0.0",
                        "stdlib": "libc++-210000",
                        "platforms": {
                            "linux/amd64": {
                                "manifest_digest": "sha256:" + "b" * 64,
                                "target": "x86_64-unknown-linux-gnu",
                            },
                            "linux/arm64": {
                                "manifest_digest": "sha256:" + "d" * 64,
                                "target": "aarch64-unknown-linux-gnu",
                            },
                        },
                    },
                },
                "macos": {
                    "arm64_macos_clang": {
                        "url": (
                            f"{release_url}/p2996-macos-arm64-"
                            f"{clang_revision}.tar.zst"
                        ),
                        "archive_sha256": "e" * 64,
                        "compiler_revision": (
                            "060be17654102019e14810c3f948ef85a490755f"
                        ),
                        "compiler_version": "Bloomberg clang 21.0.0",
                        "target": "arm64-apple-macosx15.0.0",
                        "stdlib": "libc++-210000",
                        "xcode_version": "16.4",
                        "xcode_build": "16F6",
                        "sdk_version": "15.5",
                        "sdk_build": "24F74",
                        "deployment_target": "15.0",
                        "observed_runner": {
                            "image_os": "macos15",
                            "image_version": "20260818.1",
                        },
                    },
                    "x86_64_macos_clang": {
                        "url": (
                            f"{release_url}/p2996-macos-x86_64-"
                            f"{clang_revision}.tar.zst"
                        ),
                        "archive_sha256": "f" * 64,
                        "compiler_revision": (
                            "060be17654102019e14810c3f948ef85a490755f"
                        ),
                        "compiler_version": "Bloomberg clang 21.0.0",
                        "target": "x86_64-apple-macosx15.0.0",
                        "stdlib": "libc++-210000",
                        "xcode_version": "16.4",
                        "xcode_build": "16F6",
                        "sdk_version": "15.5",
                        "sdk_build": "24F74",
                        "deployment_target": "15.0",
                        "observed_runner": {
                            "image_os": "macos15",
                            "image_version": "20260818.1",
                        },
                    },
                },
            },
        )
        return sources_lock, outputs_lock

    def make_ready_bundle(self, node="arm64_linux_gcc"):
        signatures = {
            "WorldSnapshot": "[64-le]world",
            "Entity": "[64-le]entity",
            "EntityRelativePtr": "[64-le]relative",
            "EntityIndexEntry": "[64-le]index",
            "UnitSnapshot": "[64-le]unit",
            "Effect": "[64-le]effect",
            "EffectRelativePtr": "[64-le]effect-relative",
            "AttributeEntry": "[64-le]attribute",
        }
        contracts = {
            scenario: {
                "admission": {
                    key: True for key in evidence.SCENARIO_KEYS[scenario]
                },
                "signatures": {
                    key: signatures[key]
                    for key in evidence.SCENARIO_KEYS[scenario]
                },
            }
            for scenario in evidence.SCENARIOS
        }
        compiler_family = "gcc" if node.endswith("_gcc") else "clang"
        revision = (
            "16.2.0"
            if compiler_family == "gcc"
            else "060be17654102019e14810c3f948ef85a490755f"
        )
        compiler_version = (
            "16.2.0"
            if compiler_family == "gcc"
            else "Bloomberg clang 21.0.0"
        )
        if node == "arm64_macos_clang":
            target = "arm64-apple-macosx15.0.0"
        elif node == "x86_64_macos_clang":
            target = "x86_64-apple-macosx15.0.0"
        elif node.startswith("arm64_linux"):
            target = "aarch64-unknown-linux-gnu"
        else:
            target = "x86_64-unknown-linux-gnu"
        stdlib = "libstdc++-20260801" if compiler_family == "gcc" else "libc++-210000"
        if node == "arm64_macos_clang":
            runner = "macos-15"
            runner_image = "macos15-20260818.1"
        elif node == "x86_64_macos_clang":
            runner = "macos-15-intel"
            runner_image = "macos15-intel-20260818.1"
        elif node.startswith("arm64_"):
            runner = "ubuntu-24.04-arm"
            runner_image = "ubuntu-24.04-arm-20260818.1"
        else:
            runner = "ubuntu-24.04"
            runner_image = "ubuntu-24.04-20260818.1"

        sources_lock, outputs_lock = self.make_lock_files()
        apple = {
            "xcode_version": "16.4" if "_macos_" in node else "none",
            "xcode_build": "16F6" if "_macos_" in node else "none",
            "sdk_version": "15.5" if "_macos_" in node else "none",
            "sdk_build": "24F74" if "_macos_" in node else "none",
            "deployment_target": "15.0" if "_macos_" in node else "none",
        }

        probe = self.directory / "probe.json"
        self.write_json(
            probe,
            {
                "schema": 2,
                "node": node,
                "probe": {
                    "char_bit": 8,
                    "pointer_bits": 64,
                    "endian": "little",
                    "reflection": True,
                    "memcpy_object_lifetime": True,
                    "memcpy_array_lifetime": True,
                },
                "contracts": {
                    scenario: contracts[scenario]["admission"]
                    for scenario in evidence.SCENARIOS
                },
                "compiler": {
                    "family": compiler_family,
                    "revision": revision,
                    "version": compiler_version,
                    "target": target,
                    "stdlib": stdlib,
                    **apple,
                    "sdk_locked": True,
                },
                "environment": {
                    "runner": runner,
                    "runner_image": runner_image,
                },
            },
        )

        facts = self.directory / f"{node}.producer-facts.json"
        self.write_json(
            facts,
            {
                "schema": 2,
                "node": node,
                "contracts": contracts,
            },
        )

        signature = self.directory / f"{node}.sig.hpp"
        signature_lines = [
            "// deterministic test fixture",
            f"namespace {node} {{",
        ]
        for key in evidence.KEYS:
            signature_lines.extend(
                (
                    f"inline constexpr const char {key}_layout[] =",
                    f"    {json.dumps(signatures[key])};",
                    f"inline constexpr bool {key}_byte_copy_safe = true;",
                )
            )
        signature_lines.append(f"}} // namespace {node}")
        signature.write_text("\n".join(signature_lines) + "\n", encoding="utf-8")

        world_region = self.directory / f"{node}.world.region"
        world_region.write_bytes(b"TLWORLD\0fixture")
        unit_region = self.directory / f"{node}.unit.region"
        unit_region.write_bytes(b"TLUNIT\0\0fixture")
        output = self.directory / f"{node}.provenance.json"
        return {
            "node": node,
            "profile": "authoritative",
            "execution": "native",
            "probe": probe,
            "facts": facts,
            "signature": signature,
            "world_region": world_region,
            "unit_region": unit_region,
            "sources_lock": sources_lock,
            "outputs_lock": outputs_lock,
            "runner": runner,
            "source_sha": "1" * 40,
            "workflow_run": "123456789.1",
            "toolchain_artifact_sha256": self.TOOLCHAIN_ARTIFACTS[node],
            "output": output,
        }

    @staticmethod
    def seal(bundle):
        return evidence.seal_producer(**bundle)

    def test_all_and_only_fixed_nodes_are_accepted(self):
        for node in (
            "x86_64_linux_gcc",
            "x86_64_linux_clang",
            "arm64_linux_gcc",
            "arm64_linux_clang",
            "arm64_macos_clang",
            "x86_64_macos_clang",
        ):
            self.assertEqual(evidence.validate_node(node), node)
        with self.assertRaises(evidence.EvidenceError):
            evidence.validate_node("linux_latest")

    def test_ready_bundle_hashes_are_bound(self):
        bundle = self.make_ready_bundle()
        record = self.seal(bundle)
        self.assertEqual(
            set(record),
            {
                "schema",
                "node",
                "status",
                "probe",
                "contracts",
                "compiler",
                "build",
                "locks",
                "artifacts",
            },
        )
        self.assertEqual(list(record["contracts"]), list(evidence.SCENARIOS))
        for scenario in evidence.SCENARIOS:
            self.assertEqual(
                list(record["contracts"][scenario]["admission"]),
                list(evidence.SCENARIO_KEYS[scenario]),
            )
        self.assertEqual(
            set(record["build"]),
            {
                "profile",
                "execution",
                "runner",
                "runner_image",
                "source_sha",
                "flags",
                "workflow_run",
                "toolchain_artifact_sha256",
            },
        )
        self.assertEqual(
            record["artifacts"],
            {
                "signature": {
                    "filename": bundle["signature"].name,
                    "sha256": self.sha256(bundle["signature"]),
                },
                "world": {
                    "filename": bundle["world_region"].name,
                    "sha256": self.sha256(bundle["world_region"]),
                },
                "unit_handoff": {
                    "filename": bundle["unit_region"].name,
                    "sha256": self.sha256(bundle["unit_region"]),
                },
            },
        )
        evidence.validate_provenance(bundle["output"])
        with bundle["unit_region"].open("ab") as stream:
            stream.write(b"x")
        with self.assertRaisesRegex(evidence.EvidenceError, "SHA256"):
            evidence.validate_provenance(bundle["output"])

    def test_sealer_requires_node_named_producer_facts(self):
        bundle = self.make_ready_bundle()
        bundle["facts"] = bundle["facts"].rename(
            self.directory / "facts-from-somewhere.json"
        )
        with self.assertRaisesRegex(
            evidence.EvidenceError, "producer facts filename"
        ):
            self.seal(bundle)

    def test_producer_artifact_validator_checks_exact_ready_bundle(self):
        bundle = self.make_ready_bundle()

        record = evidence.validate_producer_artifacts(
            bundle["node"], self.directory
        )

        self.assertEqual(record["node"], bundle["node"])
        self.assertEqual(set(record["contracts"]), set(evidence.SCENARIOS))
        self.assertGreater(bundle["world_region"].stat().st_size, 0)
        self.assertGreater(bundle["unit_region"].stat().st_size, 0)

    def test_producer_artifact_validator_rejects_wrong_signature_namespace(self):
        bundle = self.make_ready_bundle()
        text = bundle["signature"].read_text(encoding="utf-8")
        bundle["signature"].write_text(
            text.replace(
                f"namespace {bundle['node']} {{", "namespace wrong_node {"
            ),
            encoding="utf-8",
        )

        with self.assertRaisesRegex(evidence.EvidenceError, "namespace"):
            evidence.validate_producer_artifacts(
                bundle["node"], self.directory
            )

    def test_producer_artifact_validator_rejects_empty_region(self):
        bundle = self.make_ready_bundle()
        bundle["unit_region"].write_bytes(b"")

        with self.assertRaisesRegex(evidence.EvidenceError, "region.*empty"):
            evidence.validate_producer_artifacts(
                bundle["node"], self.directory
            )

    def test_ready_provenance_requires_exact_contract_keys(self):
        bundle = self.make_ready_bundle()
        self.seal(bundle)
        record = json.loads(bundle["output"].read_text(encoding="utf-8"))
        record["contracts"]["world"]["admission"]["Unexpected"] = True
        self.write_json(bundle["output"], record)
        with self.assertRaisesRegex(evidence.EvidenceError, "admission.*keys"):
            evidence.validate_provenance(bundle["output"])

    def test_schema_1_and_missing_unit_scenario_are_rejected(self):
        bundle = self.make_ready_bundle()
        self.seal(bundle)
        record = evidence.load_json(bundle["output"])

        record["schema"] = 1
        self.write_json(bundle["output"], record)
        with self.assertRaisesRegex(evidence.EvidenceError, "schema.*integer 2"):
            evidence.validate_provenance(bundle["output"])

        record["schema"] = 2
        del record["contracts"]["unit_handoff"]
        self.write_json(bundle["output"], record)
        with self.assertRaisesRegex(evidence.EvidenceError, "contracts.*keys"):
            evidence.validate_provenance(bundle["output"])

    def test_duplicate_json_key_is_rejected(self):
        provenance = self.directory / "duplicate.provenance.json"
        provenance.write_text(
            '{"schema":1,"schema":1,"node":"arm64_linux_gcc",'
            '"status":"INCOMPLETE","error":"duplicate"}\n',
            encoding="utf-8",
        )
        with self.assertRaisesRegex(evidence.EvidenceError, "duplicate.*schema"):
            evidence.validate_provenance(provenance)

    def test_non_finite_json_constants_are_rejected(self):
        for constant in ("NaN", "Infinity", "-Infinity"):
            with self.subTest(constant=constant):
                document = self.directory / "non-finite.json"
                document.write_text(
                    f'{{"value": {constant}}}\n', encoding="utf-8"
                )
                with self.assertRaisesRegex(
                    evidence.EvidenceError, "non-finite"
                ):
                    evidence.load_json(document)

    def test_atomic_writer_does_not_require_path_write_text_newline(self):
        output = self.directory / "arm64_linux_gcc.provenance.json"
        with mock.patch.object(
            Path,
            "write_text",
            side_effect=TypeError("newline is unavailable"),
        ):
            evidence.write_fallback_provenance(
                "arm64_linux_gcc", "fixture failure", output
            )

        self.assertEqual(
            json.loads(output.read_text(encoding="utf-8"))["status"],
            "INCOMPLETE",
        )

    def test_malformed_artifact_digest_is_rejected(self):
        bundle = self.make_ready_bundle()
        self.seal(bundle)
        record = json.loads(bundle["output"].read_text(encoding="utf-8"))
        record["artifacts"]["world"]["sha256"] = "ABC123"
        self.write_json(bundle["output"], record)
        with self.assertRaisesRegex(evidence.EvidenceError, "SHA256"):
            evidence.validate_provenance(bundle["output"])

    def test_artifact_filename_escape_is_rejected(self):
        bundle = self.make_ready_bundle()
        self.seal(bundle)
        record = json.loads(bundle["output"].read_text(encoding="utf-8"))
        record["artifacts"]["world"]["filename"] = "../escaped.region"
        self.write_json(bundle["output"], record)
        with self.assertRaisesRegex(evidence.EvidenceError, "filename"):
            evidence.validate_provenance(bundle["output"])

    def test_provenance_filename_must_bind_its_node(self):
        bundle = self.make_ready_bundle()
        self.seal(bundle)
        renamed = bundle["output"].with_name("different.provenance.json")
        bundle["output"].rename(renamed)
        with self.assertRaisesRegex(evidence.EvidenceError, "provenance filename"):
            evidence.validate_provenance(renamed)

    def test_ready_provenance_without_both_artifacts_is_rejected(self):
        bundle = self.make_ready_bundle()
        self.seal(bundle)
        record = json.loads(bundle["output"].read_text(encoding="utf-8"))
        del record["artifacts"]["signature"]
        self.write_json(bundle["output"], record)
        with self.assertRaisesRegex(evidence.EvidenceError, "artifacts.*keys"):
            evidence.validate_provenance(bundle["output"])

    def test_reject_provenance_cannot_name_payload_artifacts(self):
        bundle = self.make_ready_bundle()
        self.seal(bundle)
        record = json.loads(bundle["output"].read_text(encoding="utf-8"))
        record["status"] = "REJECT"
        record["contracts"]["world"]["admission"]["Entity"] = False
        self.write_json(bundle["output"], record)
        with self.assertRaisesRegex(evidence.EvidenceError, "inadmitted scenario"):
            evidence.validate_provenance(bundle["output"])

    def test_incomplete_provenance_cannot_invent_signatures(self):
        provenance = self.directory / "arm64_linux_gcc.provenance.json"
        self.write_json(
            provenance,
            {
                "schema": 2,
                "node": "arm64_linux_gcc",
                "status": "INCOMPLETE",
                "error": "compiler failed",
                "signatures": {"Entity": "invented"},
            },
        )
        with self.assertRaisesRegex(evidence.EvidenceError, "top-level.*keys"):
            evidence.validate_provenance(provenance)

    def test_sealer_rejects_a_failed_memcpy_array_probe(self):
        bundle = self.make_ready_bundle()
        probe = json.loads(bundle["probe"].read_text(encoding="utf-8"))
        probe["probe"]["memcpy_array_lifetime"] = False
        self.write_json(bundle["probe"], probe)
        with self.assertRaisesRegex(evidence.EvidenceError, "memcpy_array_lifetime"):
            self.seal(bundle)

    def test_sealer_preserves_evaluated_admission_rejection_without_payload(self):
        bundle = self.make_ready_bundle()
        for path_key in ("probe", "facts"):
            record = json.loads(bundle[path_key].read_text(encoding="utf-8"))
            if path_key == "probe":
                record["contracts"]["world"]["Entity"] = False
            else:
                record["contracts"]["world"]["admission"]["Entity"] = False
            self.write_json(bundle[path_key], record)
        bundle["signature"].unlink()
        bundle["world_region"].unlink()
        del bundle["signature"]
        del bundle["world_region"]

        record = self.seal(bundle)

        self.assertEqual(record["status"], "REJECT")
        self.assertFalse(record["contracts"]["world"]["admission"]["Entity"])
        self.assertEqual(set(record["artifacts"]), {"unit_handoff"})
        self.assertEqual(
            record["contracts"]["world"]["signatures"]["Entity"],
            "[64-le]entity",
        )

    def test_sealer_rejects_lock_and_probe_compiler_mismatch(self):
        bundle = self.make_ready_bundle()
        probe = json.loads(bundle["probe"].read_text(encoding="utf-8"))
        probe["compiler"]["revision"] = "16.1.0"
        self.write_json(bundle["probe"], probe)
        with self.assertRaisesRegex(evidence.EvidenceError, "revision"):
            self.seal(bundle)

    def test_future_lock_shape_maps_every_node_to_one_artifact(self):
        expected_policy_keys = {
            "node",
            "compiler_family",
            "compiler_revision",
            "compiler_version",
            "target",
            "stdlib",
            "flags",
            "toolchain_artifact_sha256",
            "xcode_version",
            "xcode_build",
            "sdk_version",
            "sdk_build",
            "deployment_target",
        }
        sources_lock, outputs_lock = self.make_lock_files()
        expected = {
            "x86_64_linux_gcc": (
                "gcc",
                "16.2.0",
                "-O3 -fstrict-aliasing",
                "a" * 64,
            ),
            "x86_64_linux_clang": (
                "clang",
                "060be17654102019e14810c3f948ef85a490755f",
                "-O3 -fstrict-aliasing -stdlib=libc++",
                "b" * 64,
            ),
            "arm64_linux_gcc": (
                "gcc",
                "16.2.0",
                "-O3 -fstrict-aliasing",
                "c" * 64,
            ),
            "arm64_linux_clang": (
                "clang",
                "060be17654102019e14810c3f948ef85a490755f",
                "-O3 -fstrict-aliasing -stdlib=libc++",
                "d" * 64,
            ),
            "arm64_macos_clang": (
                "clang",
                "060be17654102019e14810c3f948ef85a490755f",
                (
                    "-O3 -fstrict-aliasing -stdlib=libc++ "
                    "-mmacosx-version-min=15.0"
                ),
                "e" * 64,
            ),
            "x86_64_macos_clang": (
                "clang",
                "060be17654102019e14810c3f948ef85a490755f",
                (
                    "-O3 -fstrict-aliasing -stdlib=libc++ "
                    "-mmacosx-version-min=15.0"
                ),
                "f" * 64,
            ),
        }
        actual = {}
        for node in expected:
            policy, _, _ = evidence.load_node_toolchain_policy(
                sources_lock, outputs_lock, node
            )
            self.assertEqual(set(policy), expected_policy_keys)
            self.assertEqual(policy["node"], node)
            actual[node] = (
                policy["compiler_family"],
                policy["compiler_revision"],
                policy["flags"],
                policy["toolchain_artifact_sha256"],
            )

        self.assertEqual(actual, expected)
        self.assertEqual(len({entry[3] for entry in actual.values()}), 6)

    def test_lock_rejects_unknown_field_in_unselected_toolchain(self):
        bundle = self.make_ready_bundle("arm64_linux_gcc")
        outputs = json.loads(
            bundle["outputs_lock"].read_text(encoding="utf-8")
        )
        outputs["linux"]["p2996"]["platforms"]["linux/amd64"][
            "unexpected"
        ] = True
        self.write_json(bundle["outputs_lock"], outputs)

        with self.assertRaisesRegex(evidence.EvidenceError, "output lock"):
            self.seal(bundle)

    def test_lock_rejects_missing_field_in_unselected_macos_node(self):
        bundle = self.make_ready_bundle("arm64_linux_gcc")
        sources = json.loads(
            bundle["sources_lock"].read_text(encoding="utf-8")
        )
        del sources["macos"]["nodes"]["x86_64_macos_clang"]["sdk_build"]
        self.write_json(bundle["sources_lock"], sources)
        outputs = json.loads(
            bundle["outputs_lock"].read_text(encoding="utf-8")
        )
        outputs["sources_sha256"] = self.sha256(bundle["sources_lock"])
        self.write_json(bundle["outputs_lock"], outputs)

        with self.assertRaisesRegex(evidence.EvidenceError, "source lock"):
            self.seal(bundle)

    def test_lock_rejects_invalid_type_in_unselected_source_section(self):
        bundle = self.make_ready_bundle("arm64_linux_gcc")
        sources = json.loads(
            bundle["sources_lock"].read_text(encoding="utf-8")
        )
        sources["linux"]["docker"]["runners"]["ubuntu-24.04"][
            "client_version"
        ] = 27
        self.write_json(bundle["sources_lock"], sources)
        outputs = json.loads(
            bundle["outputs_lock"].read_text(encoding="utf-8")
        )
        outputs["sources_sha256"] = self.sha256(bundle["sources_lock"])
        self.write_json(bundle["outputs_lock"], outputs)

        with self.assertRaisesRegex(evidence.EvidenceError, "source lock"):
            self.seal(bundle)

    def test_lock_rejects_bad_digest_in_unselected_platform(self):
        bundle = self.make_ready_bundle("x86_64_linux_gcc")
        outputs = json.loads(
            bundle["outputs_lock"].read_text(encoding="utf-8")
        )
        outputs["linux"]["gcc"]["platforms"]["linux/arm64"][
            "manifest_digest"
        ] = "sha256:not-a-digest"
        self.write_json(bundle["outputs_lock"], outputs)

        with self.assertRaisesRegex(evidence.EvidenceError, "output lock"):
            self.seal(bundle)

    def test_lock_rejects_unknown_source_action_entry(self):
        bundle = self.make_ready_bundle("arm64_linux_gcc")
        sources = json.loads(
            bundle["sources_lock"].read_text(encoding="utf-8")
        )
        sources["actions"]["unknown_action"] = "mutable"
        self.write_json(bundle["sources_lock"], sources)
        outputs = json.loads(
            bundle["outputs_lock"].read_text(encoding="utf-8")
        )
        outputs["sources_sha256"] = self.sha256(bundle["sources_lock"])
        self.write_json(bundle["outputs_lock"], outputs)

        with self.assertRaisesRegex(evidence.EvidenceError, "source lock"):
            self.seal(bundle)

    def test_lock_rejects_noncanonical_p2996_projects(self):
        bundle = self.make_ready_bundle("arm64_linux_gcc")
        sources = json.loads(
            bundle["sources_lock"].read_text(encoding="utf-8")
        )
        sources["p2996"]["projects"] = ["clang", "clang-tools-extra"]
        self.write_json(bundle["sources_lock"], sources)
        outputs = json.loads(
            bundle["outputs_lock"].read_text(encoding="utf-8")
        )
        outputs["sources_sha256"] = self.sha256(bundle["sources_lock"])
        self.write_json(bundle["outputs_lock"], outputs)

        with self.assertRaisesRegex(evidence.EvidenceError, "projects"):
            self.seal(bundle)

    def test_lock_rejects_unpinned_package_version(self):
        bundle = self.make_ready_bundle("arm64_linux_gcc")
        sources = json.loads(
            bundle["sources_lock"].read_text(encoding="utf-8")
        )
        sources["linux"]["packages"]["p2996_builder"] = ["cmake"]
        self.write_json(bundle["sources_lock"], sources)
        outputs = json.loads(
            bundle["outputs_lock"].read_text(encoding="utf-8")
        )
        outputs["sources_sha256"] = self.sha256(bundle["sources_lock"])
        self.write_json(bundle["outputs_lock"], outputs)

        with self.assertRaisesRegex(evidence.EvidenceError, "packages"):
            self.seal(bundle)

    def test_lock_rejects_malformed_recipe_digest(self):
        bundle = self.make_ready_bundle("arm64_linux_gcc")
        sources = json.loads(
            bundle["sources_lock"].read_text(encoding="utf-8")
        )
        sources["recipes"][".gitattributes"] = "not-a-digest"
        self.write_json(bundle["sources_lock"], sources)
        outputs = json.loads(
            bundle["outputs_lock"].read_text(encoding="utf-8")
        )
        outputs["sources_sha256"] = self.sha256(bundle["sources_lock"])
        self.write_json(bundle["outputs_lock"], outputs)

        with self.assertRaisesRegex(evidence.EvidenceError, "recipes"):
            self.seal(bundle)

    def test_lock_rejects_duplicate_node_artifact_digest(self):
        bundle = self.make_ready_bundle("arm64_linux_gcc")
        outputs = json.loads(
            bundle["outputs_lock"].read_text(encoding="utf-8")
        )
        outputs["macos"]["arm64_macos_clang"]["archive_sha256"] = "a" * 64
        self.write_json(bundle["outputs_lock"], outputs)

        with self.assertRaisesRegex(evidence.EvidenceError, "artifact digest"):
            self.seal(bundle)

    def test_output_lock_rejects_wrong_target_for_every_platform_kind(self):
        attacks = (
            (
                ("linux", "gcc", "platforms", "linux/amd64", "target"),
                "aarch64-linux-gnu",
            ),
            (
                ("linux", "gcc", "platforms", "linux/arm64", "target"),
                "x86_64-linux-gnu",
            ),
            (
                ("linux", "p2996", "platforms", "linux/amd64", "target"),
                "x86_64-linux-gnu",
            ),
            (
                ("linux", "p2996", "platforms", "linux/arm64", "target"),
                "arm64-unknown-linux-gnu",
            ),
            (
                ("macos", "arm64_macos_clang", "target"),
                "arm64-apple-macosx15.1.0",
            ),
            (
                ("macos", "x86_64_macos_clang", "target"),
                "x86_64-apple-macosx14.0.0",
            ),
        )
        for path, replacement in attacks:
            with self.subTest(path=path):
                bundle = self.make_ready_bundle("arm64_linux_gcc")
                outputs = json.loads(
                    bundle["outputs_lock"].read_text(encoding="utf-8")
                )
                target = outputs
                for key in path[:-1]:
                    target = target[key]
                target[path[-1]] = replacement
                self.write_json(bundle["outputs_lock"], outputs)

                with self.assertRaisesRegex(evidence.EvidenceError, "target"):
                    evidence.load_node_toolchain_policy(
                        bundle["sources_lock"],
                        bundle["outputs_lock"],
                        "arm64_linux_gcc",
                    )

    def test_all_eight_output_artifact_digests_must_be_globally_unique(self):
        attacks = (
            (
                ("linux", "gcc", "index_digest"),
                ("linux", "p2996", "index_digest"),
            ),
            (
                ("linux", "gcc", "index_digest"),
                (
                    "linux",
                    "gcc",
                    "platforms",
                    "linux/amd64",
                    "manifest_digest",
                ),
            ),
            (
                (
                    "linux",
                    "p2996",
                    "platforms",
                    "linux/arm64",
                    "manifest_digest",
                ),
                ("macos", "arm64_macos_clang", "archive_sha256"),
            ),
            (
                ("macos", "arm64_macos_clang", "archive_sha256"),
                ("macos", "x86_64_macos_clang", "archive_sha256"),
            ),
        )
        for destination_path, source_path in attacks:
            with self.subTest(
                destination_path=destination_path, source_path=source_path
            ):
                bundle = self.make_ready_bundle("arm64_linux_gcc")
                outputs = json.loads(
                    bundle["outputs_lock"].read_text(encoding="utf-8")
                )

                source = outputs
                for key in source_path:
                    source = source[key]
                destination = outputs
                for key in destination_path[:-1]:
                    destination = destination[key]
                if destination_path[-1] == "archive_sha256":
                    source = source.removeprefix("sha256:")
                elif not source.startswith("sha256:"):
                    source = "sha256:" + source
                destination[destination_path[-1]] = source
                self.write_json(bundle["outputs_lock"], outputs)

                with self.assertRaisesRegex(
                    evidence.EvidenceError, "artifact digest"
                ):
                    evidence.load_node_toolchain_policy(
                        bundle["sources_lock"],
                        bundle["outputs_lock"],
                        "arm64_linux_gcc",
                    )

    def test_sealer_rejects_wrong_linux_manifest_digest(self):
        bundle = self.make_ready_bundle("arm64_linux_gcc")
        bundle["toolchain_artifact_sha256"] = "0" * 64
        with self.assertRaisesRegex(evidence.EvidenceError, "toolchain artifact"):
            self.seal(bundle)

    def test_sealer_rejects_wrong_macos_archive_digest(self):
        bundle = self.make_ready_bundle("arm64_macos_clang")
        bundle["toolchain_artifact_sha256"] = "0" * 64
        with self.assertRaisesRegex(evidence.EvidenceError, "toolchain artifact"):
            self.seal(bundle)

    def test_runner_image_change_does_not_change_admission_or_sealing(self):
        bundle = self.make_ready_bundle("arm64_linux_gcc")
        probe = json.loads(bundle["probe"].read_text(encoding="utf-8"))
        probe["environment"]["runner_image"] = "ubuntu-observed-after-refresh"
        self.write_json(bundle["probe"], probe)

        record = self.seal(bundle)

        self.assertEqual(record["status"], "READY")
        self.assertEqual(
            record["build"]["runner_image"], "ubuntu-observed-after-refresh"
        )

    def test_sealer_rejects_apple_xcode_build_mismatch(self):
        bundle = self.make_ready_bundle("arm64_macos_clang")
        probe = json.loads(bundle["probe"].read_text(encoding="utf-8"))
        probe["compiler"]["xcode_build"] = "16F7"
        self.write_json(bundle["probe"], probe)
        with self.assertRaisesRegex(evidence.EvidenceError, "xcode_build"):
            self.seal(bundle)

    def test_probe_rejects_obsolete_merged_apple_fields(self):
        bundle = self.make_ready_bundle()
        probe = json.loads(bundle["probe"].read_text(encoding="utf-8"))
        compiler = probe["compiler"]
        probe["compiler"] = {
            "family": compiler["family"],
            "revision": compiler["revision"],
            "version": compiler["version"],
            "target": compiler["target"],
            "stdlib": compiler["stdlib"],
            "xcode": "none",
            "sdk": "none",
            "deployment_target": "none",
            "sdk_locked": True,
        }
        self.write_json(bundle["probe"], probe)
        with self.assertRaisesRegex(evidence.EvidenceError, "compiler.*keys"):
            evidence.validate_probe(bundle["probe"])

    def test_seal_cli_requires_toolchain_artifact_digest(self):
        bundle = self.make_ready_bundle()
        completed = subprocess.run(
            [
                sys.executable,
                str(Path("tools/relocatable_world_evidence.py")),
                "seal-producer",
                "--node",
                bundle["node"],
                "--profile",
                bundle["profile"],
                "--execution",
                bundle["execution"],
                "--probe",
                str(bundle["probe"]),
                "--facts",
                str(bundle["facts"]),
                "--signature",
                str(bundle["signature"]),
                "--world-region",
                str(bundle["world_region"]),
                "--unit-region",
                str(bundle["unit_region"]),
                "--sources-lock",
                str(bundle["sources_lock"]),
                "--outputs-lock",
                str(bundle["outputs_lock"]),
                "--runner",
                bundle["runner"],
                "--source-sha",
                bundle["source_sha"],
                "--workflow-run",
                bundle["workflow_run"],
                "--output",
                str(bundle["output"]),
            ],
            cwd=Path(__file__).resolve().parents[1],
            capture_output=True,
            text=True,
            check=False,
        )
        self.assertEqual(completed.returncode, 2)
        self.assertIn("--toolchain-artifact-sha256", completed.stderr)

    def test_authoritative_workflow_run_is_canonical_run_and_attempt(self):
        invalid_values = (
            "123456789",
            "0.1",
            "123456789.0",
            "123456789.1.1",
            "123456789.1 ",
            " 123456789.1",
            "01.1",
        )
        for workflow_run in invalid_values:
            with self.subTest(workflow_run=workflow_run):
                bundle = self.make_ready_bundle()
                bundle["workflow_run"] = workflow_run
                with self.assertRaisesRegex(
                    evidence.EvidenceError, "workflow_run"
                ):
                    self.seal(bundle)

    def test_local_workflow_run_uses_distinct_invocation_format(self):
        accepted_values = (
            "local-111111111111-20260828T051234Z-4242",
            "manual-arm64-mac-1",
            "manual.arm64.v1",
        )
        for workflow_run in accepted_values:
            with self.subTest(workflow_run=workflow_run):
                bundle = self.make_ready_bundle("arm64_linux_gcc")
                bundle["profile"] = "local-arm64-macos"
                bundle["workflow_run"] = workflow_run
                record = self.seal(bundle)
                self.assertEqual(
                    record["build"]["workflow_run"], workflow_run
                )

        invalid_values = (
            "123456789.1",
            "0.1",
            "01.1",
            "123456789.0",
            "",
            "manual/arm64",
            "manual\\arm64",
            "manual arm64",
            "manual\narm64",
            "manual..arm64",
            ".hidden",
            "-option",
            "manual-",
            "a" * 129,
        )
        for workflow_run in invalid_values:
            with self.subTest(workflow_run=workflow_run):
                bundle = self.make_ready_bundle("arm64_linux_gcc")
                bundle["profile"] = "local-arm64-macos"
                bundle["workflow_run"] = workflow_run
                with self.assertRaisesRegex(
                    evidence.EvidenceError, "workflow_run"
                ):
                    self.seal(bundle)

    def test_probe_ctest_defaults_mark_placeholder_macos_sdk_unlocked(self):
        module = (
            Path(__file__).resolve().parents[1]
            / "cmake"
            / "RelocatableWorldProbeDefaults.cmake"
        )
        for system_name, expected in (("Darwin", "false"), ("Linux", "true")):
            with self.subTest(system_name=system_name):
                script = self.directory / f"probe-{system_name}.cmake"
                script.write_text(
                    "\n".join(
                        (
                            f'set(CMAKE_SYSTEM_NAME "{system_name}")',
                            f'include("{module.as_posix()}")',
                            "if(NOT TYPELAYOUT_PROBE_SDK_LOCKED "
                            f'STREQUAL "{expected}")',
                            "  message(FATAL_ERROR \"wrong sdk lock state\")",
                            "endif()",
                            "",
                        )
                    ),
                    encoding="utf-8",
                )
                completed = subprocess.run(
                    ["cmake", "-P", str(script)],
                    capture_output=True,
                    text=True,
                    check=False,
                )
                self.assertEqual(
                    completed.returncode,
                    0,
                    completed.stdout + completed.stderr,
                )

    def test_output_lock_requires_authoritative_workflow_run(self):
        bundle = self.make_ready_bundle()
        outputs = json.loads(
            bundle["outputs_lock"].read_text(encoding="utf-8")
        )
        outputs["workflow_run"] = "987654321"
        self.write_json(bundle["outputs_lock"], outputs)
        with self.assertRaisesRegex(
            evidence.EvidenceError, "output lock.workflow_run"
        ):
            self.seal(bundle)

    def test_sealer_rejects_signature_header_disagreement(self):
        bundle = self.make_ready_bundle()
        text = bundle["signature"].read_text(encoding="utf-8")
        bundle["signature"].write_text(
            text.replace("[64-le]entity", "[64-le]packed"),
            encoding="utf-8",
        )
        with self.assertRaisesRegex(evidence.EvidenceError, "Entity.*signature"):
            self.seal(bundle)

    def test_sealer_rejects_signature_header_with_an_extra_contract_type(self):
        bundle = self.make_ready_bundle()
        text = bundle["signature"].read_text(encoding="utf-8")
        bundle["signature"].write_text(
            text.replace(
                f"}} // namespace {bundle['node']}",
                "inline constexpr const char Extra_layout[] = \"extra\";\n"
                "inline constexpr bool Extra_byte_copy_safe = true;\n"
                f"}} // namespace {bundle['node']}",
            ),
            encoding="utf-8",
        )
        with self.assertRaisesRegex(evidence.EvidenceError, "exactly eight contract"):
            self.seal(bundle)

    def test_fallback_results_preserve_every_directed_identity(self):
        output = self.directory / "fallback.results.json"
        evidence.write_fallback_results(
            "authoritative",
            "arm64_linux_gcc",
            "producer unavailable",
            output,
        )
        record = evidence.load_json(output)
        self.assertEqual(record["consumer"], "arm64_linux_gcc")
        self.assertEqual(
            [transfer["producer"] for transfer in record["transfers"]],
            [node for node in evidence.NODES if node != "arm64_linux_gcc"],
        )
        self.assertTrue(
            all(
                outcome["status"] == "INCOMPLETE"
                for transfer in record["transfers"]
                for outcome in transfer["scenarios"].values()
            )
        )

    def test_fallback_provenance_refuses_a_misnamed_output(self):
        with self.assertRaisesRegex(evidence.EvidenceError, "provenance filename"):
            evidence.write_fallback_provenance(
                "arm64_linux_gcc",
                "probe unavailable",
                self.directory / "wrong.provenance.json",
            )

    def test_fallback_agreements_preserve_all_pairs_and_keys(self):
        output = self.directory / "agreements.json"
        evidence.write_fallback_agreements(
            "authoritative", "producer jobs unavailable", output
        )
        record = evidence.load_json(output)
        self.assertEqual(len(record["pairs"]), 15)
        self.assertTrue(
            all(
                [decision["key"] for decision in pair["scenarios"][scenario]]
                == list(evidence.SCENARIO_KEYS[scenario])
                for pair in record["pairs"]
                for scenario in evidence.SCENARIOS
            )
        )

    def test_fallback_cli_writes_strict_incomplete_provenance(self):
        output = self.directory / "x86_64_linux_clang.provenance.json"
        completed = subprocess.run(
            [
                sys.executable,
                str(Path("tools/relocatable_world_evidence.py")),
                "fallback-provenance",
                "--node",
                "x86_64_linux_clang",
                "--reason",
                "probe did not run",
                "--output",
                str(output),
            ],
            cwd=Path(__file__).resolve().parents[1],
            capture_output=True,
            text=True,
            check=False,
        )
        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertEqual(
            evidence.validate_provenance(output),
            {
                "schema": 2,
                "node": "x86_64_linux_clang",
                "status": "INCOMPLETE",
                "error": "probe did not run",
            },
        )

    def make_complete_matrix_run(self, profile="authoritative"):
        nodes = evidence.profile_nodes(profile)
        source_sha = "1" * 40
        workflow_run = (
            "123456789.1"
            if profile == "authoritative"
            else "local-111111111111-20260828T051234Z-4242"
        )
        run_directory = self.directory / f"{profile} matrix run with spaces"
        run_directory.mkdir()
        provenance = {}
        for node in nodes:
            bundle = self.make_ready_bundle(node)
            bundle["profile"] = profile
            bundle["workflow_run"] = workflow_run
            if profile == "local-arm64-macos" and node.startswith("x86_64_"):
                bundle["execution"] = "emulated"
            record = self.seal(bundle)
            for suffix in (
                ".provenance.json", ".sig.hpp", ".world.region", ".unit.region"
            ):
                shutil.copy2(
                    self.directory / f"{node}{suffix}",
                    run_directory / f"{node}{suffix}",
                )
            provenance[node] = record

        sources_lock = self.directory / "toolchain-sources.lock"
        outputs_lock = self.directory / "toolchains.lock"
        run_identity = {
            "source_sha": source_sha,
            "workflow_run": workflow_run,
            "sources_sha256": self.sha256(sources_lock),
            "outputs_sha256": self.sha256(outputs_lock),
        }
        provenance_digests = {
            node: self.sha256(run_directory / f"{node}.provenance.json")
            for node in nodes
        }
        region_digests = {
            node: {
                scenario: provenance[node]["artifacts"][scenario]["sha256"]
                for scenario in evidence.SCENARIOS
            }
            for node in nodes
        }

        pairs = []
        for left_index, left in enumerate(nodes):
            for right in nodes[left_index + 1:]:
                pairs.append(
                    {
                        "left": left,
                        "right": right,
                        "scenarios": {
                            scenario: [
                                {
                                    "key": key,
                                    "status": "PERMIT",
                                    "reason": "Admission and signature agree",
                                }
                                for key in evidence.SCENARIO_KEYS[scenario]
                            ]
                            for scenario in evidence.SCENARIOS
                        },
                    }
                )
        agreements = {
            "schema": 2,
            "profile": profile,
            "producer_provenance_sha256": provenance_digests,
            "pairs": pairs,
        }
        agreements_path = run_directory / "agreements.json"
        agreements_path.write_text(
            json.dumps(agreements, indent=2, sort_keys=False) + "\n",
            encoding="utf-8",
        )

        for consumer in nodes:
            producer_build = provenance[consumer]["build"]
            compiler = provenance[consumer]["compiler"]
            locks = provenance[consumer]["locks"]
            build = {
                "source_sha": producer_build["source_sha"],
                "workflow_run": producer_build["workflow_run"],
                "sources_sha256": locks["sources_sha256"],
                "outputs_sha256": locks["outputs_sha256"],
                "execution": producer_build["execution"],
                "runner": producer_build["runner"],
                "runner_image": producer_build["runner_image"],
                "toolchain_artifact_sha256": producer_build[
                    "toolchain_artifact_sha256"
                ],
                "compiler_family": compiler["family"],
                "compiler_revision": compiler["revision"],
                "compiler_version": compiler["version"],
                "target": compiler["target"],
                "stdlib": compiler["stdlib"],
                "flags": producer_build["flags"],
                "xcode_version": compiler["xcode_version"],
                "xcode_build": compiler["xcode_build"],
                "sdk_version": compiler["sdk_version"],
                "sdk_build": compiler["sdk_build"],
                "deployment_target": compiler["deployment_target"],
                "sdk_locked": compiler["sdk_locked"],
            }
            results = {
                "schema": 2,
                "profile": profile,
                "consumer": consumer,
                "consumer_provenance_sha256": provenance_digests[consumer],
                "build": build,
                "transfers": [
                    {
                        "producer": producer,
                        "producer_provenance_sha256": provenance_digests[producer],
                        "scenarios": {
                            scenario: {
                                "status": "PASS",
                                "reason": (
                                    f"{scenario} checkpoint loaded and validated"
                                ),
                                "region_sha256": region_digests[producer][scenario],
                            }
                            for scenario in evidence.SCENARIOS
                        },
                    }
                    for producer in nodes
                    if producer != consumer
                ],
            }
            self.write_json(
                run_directory / f"{consumer}.results.json", results
            )

        pair_identities = [
            {
                "left": pair["left"], "right": pair["right"],
                "scenario": scenario,
            }
            for scenario in evidence.SCENARIOS
            for pair in pairs
        ]
        named_identities = [
            {
                "left": pair["left"],
                "right": pair["right"],
                "scenario": scenario,
                "key": decision["key"],
            }
            for scenario in evidence.SCENARIOS
            for pair in pairs
            for decision in pair["scenarios"][scenario]
        ]
        transfers = [
            {
                "consumer": consumer, "producer": producer,
                "scenario": scenario,
            }
            for scenario in evidence.SCENARIOS
            for consumer in nodes
            for producer in nodes
            if consumer != producer
        ]
        identities = {
            "nodes": list(nodes),
            "pairs": pair_identities,
            "named_decisions": named_identities,
            "consumers": list(nodes),
            "transfers": transfers,
        }
        closure = {
            "schema": 2,
            "profile": profile,
            "authoritative": profile == "authoritative",
            "run": run_identity,
            "agreements_sha256": self.sha256(agreements_path),
            "expected": identities,
            "counts": {
                "nodes": len(nodes),
                "consumers": len(nodes),
                "scenarios": {
                    scenario: {
                        "pairs": len(pairs),
                        "named_decisions": (
                            len(pairs) * len(evidence.SCENARIO_KEYS[scenario])
                        ),
                        "named_permits": (
                            len(pairs) * len(evidence.SCENARIO_KEYS[scenario])
                        ),
                        "transfers": len(transfers) // len(evidence.SCENARIOS),
                        "passes": len(transfers) // len(evidence.SCENARIOS),
                    }
                    for scenario in evidence.SCENARIOS
                },
            },
            "missing": {key: [] for key in identities},
            "duplicates": {key: [] for key in identities},
            "status": "PASS",
            "error": None,
        }
        (run_directory / "closure.json").write_text(
            json.dumps(closure, indent=2, sort_keys=False) + "\n",
            encoding="utf-8",
        )
        (run_directory / "source-sha.txt").write_text(
            source_sha + "\n", encoding="utf-8"
        )
        run_metadata_name = (
            "workflow-run.txt" if profile == "authoritative" else "run-id.txt"
        )
        (run_directory / run_metadata_name).write_text(
            workflow_run + "\n", encoding="utf-8"
        )
        return {
            "profile": profile,
            "directory": run_directory,
            "sources_lock": sources_lock,
            "outputs_lock": outputs_lock,
            "source_sha": source_sha,
            "workflow_run": workflow_run,
            "provenance": provenance,
        }

    def write_consumer_probe_from_provenance(self, record, output):
        self.write_json(
            output,
            {
                "schema": 2,
                "node": record["node"],
                "probe": record["probe"],
                "contracts": {
                    scenario: record["contracts"][scenario]["admission"]
                    for scenario in evidence.SCENARIOS
                },
                "compiler": record["compiler"],
                "environment": {
                    "runner": record["build"]["runner"],
                    "runner_image": record["build"]["runner_image"],
                },
            },
        )

    def test_task3_strict_validators_accept_only_fixed_identity_shapes(self):
        fixture = self.make_complete_matrix_run()
        run_directory = fixture["directory"]
        for node in evidence.NODES:
            evidence.validate_results(run_directory / f"{node}.results.json")
        evidence.validate_agreements(run_directory / "agreements.json")
        closure = evidence.validate_closure(run_directory / "closure.json")
        self.assertEqual(
            tuple(closure["expected"]),
            ("nodes", "pairs", "named_decisions", "consumers", "transfers"),
        )
        self.assertEqual(tuple(closure["missing"]), tuple(closure["expected"]))
        self.assertEqual(tuple(closure["duplicates"]), tuple(closure["expected"]))

        malformed = evidence.load_json(run_directory / "closure.json")
        malformed["expected"]["keys"] = list(evidence.KEYS)
        self.write_json(run_directory / "closure.json", malformed)
        with self.assertRaisesRegex(evidence.EvidenceError, "expected keys"):
            evidence.validate_closure(run_directory / "closure.json")

    def test_task3_fixture_preparation_is_empty_isolated_and_marked(self):
        empty_producers = self.directory / "empty-producers"
        empty_results = self.directory / "empty-results"
        fallback = self.directory / "fallback"
        generated = self.directory / "generated"
        for directory in (empty_producers, empty_results, fallback, generated):
            directory.mkdir()
        repository = Path(__file__).resolve().parents[1]
        tool = repository / "tools" / "relocatable_world_evidence.py"

        def run_cli(*arguments):
            completed = subprocess.run(
                [sys.executable, str(tool), *map(str, arguments)],
                cwd=repository,
                capture_output=True,
                text=True,
                check=False,
            )
            self.assertEqual(
                completed.returncode,
                0,
                completed.stdout + completed.stderr,
            )

        agreements = fallback / "agreements.json"
        run_cli(
            "fallback-agreements",
            "--profile",
            "authoritative",
            "--reason",
            "fixture has no producers",
            "--output",
            agreements,
        )

        consumer_header = generated / "relocatable_world_consumer_input.hpp"
        agreement_header = generated / "relocatable_world_agreement_input.hpp"
        matrix_header = generated / "relocatable_world_matrix_input.hpp"
        evidence.prepare_consumer(
            profile="authoritative",
            consumer="x86_64_linux_clang",
            evidence=empty_producers,
            fixture_context=True,
            output_header=consumer_header,
        )
        evidence.prepare_agreements(
            profile="authoritative",
            evidence=empty_producers,
            fixture_context=True,
            output_header=agreement_header,
        )
        run_cli(
            "prepare-matrix",
            "--profile",
            "authoritative",
            "--evidence",
            empty_producers,
            "--results",
            empty_results,
            "--agreements",
            agreements,
            "--fixture-context",
            "--output-header",
            matrix_header,
        )
        for header in (consumer_header, agreement_header, matrix_header):
            text = header.read_text(encoding="utf-8")
            self.assertIn("fixture_context = true", text)
            self.assertIn("std::array<char,", text)
            self.assertNotIn('"fixture has no producers"', text)

        with self.assertRaisesRegex(evidence.EvidenceError, "outside"):
            evidence.prepare_agreements(
                profile="authoritative",
                evidence=empty_producers,
                fixture_context=True,
                output_header=(
                    empty_producers / "relocatable_world_agreement_input.hpp"
                ),
            )

        (empty_producers / "unexpected.provenance.json").write_text(
            "{}\n", encoding="utf-8"
        )
        with self.assertRaisesRegex(evidence.EvidenceError, "must be empty"):
            evidence.prepare_agreements(
                profile="authoritative",
                evidence=empty_producers,
                fixture_context=True,
                output_header=agreement_header,
            )
        with self.assertRaisesRegex(evidence.EvidenceError, "mutually exclusive"):
            evidence.prepare_consumer(
                profile="authoritative",
                consumer="x86_64_linux_clang",
                evidence=self.directory / "another-empty",
                fixture_context=True,
                expect_source_sha="1" * 40,
                output_header=consumer_header,
            )

        local_producers = self.directory / "local-empty-producers"
        local_results = self.directory / "local-empty-results"
        local_fallback = self.directory / "local-fallback"
        local_generated = self.directory / "local-generated"
        for directory in (
            local_producers,
            local_results,
            local_fallback,
            local_generated,
        ):
            directory.mkdir()
        local_agreement = local_fallback / "agreements.json"
        evidence.write_fallback_agreements(
            "local-arm64-macos", "local fixture", local_agreement
        )
        local_header = local_generated / "relocatable_world_matrix_input.hpp"
        evidence.prepare_matrix(
            profile="local-arm64-macos",
            evidence=local_producers,
            results=local_results,
            agreements=local_agreement,
            fixture_context=True,
            output_header=local_header,
        )
        self.assertIn(
            "std::array<matrix::transfer_record, 20>",
            local_header.read_text(encoding="utf-8"),
        )

    def test_task3_production_preparation_binds_run_not_output_lock_run(self):
        fixture = self.make_complete_matrix_run()
        run_directory = fixture["directory"]
        generated = self.directory / "generated production"
        generated.mkdir()
        producer_directory = self.directory / "production producers"
        results_directory = self.directory / "production results"
        agreement_directory = self.directory / "production agreement"
        for directory in (
            producer_directory,
            results_directory,
            agreement_directory,
        ):
            directory.mkdir()
        for node in evidence.NODES:
            for suffix in (
                ".provenance.json", ".sig.hpp", ".world.region", ".unit.region"
            ):
                shutil.copy2(
                    run_directory / f"{node}{suffix}",
                    producer_directory / f"{node}{suffix}",
                )
            shutil.copy2(
                run_directory / f"{node}.results.json",
                results_directory / f"{node}.results.json",
            )
        shutil.copy2(
            run_directory / "agreements.json",
            agreement_directory / "agreements.json",
        )
        consumer = "arm64_linux_clang"
        probe = self.directory / "consumer-probe.json"
        self.write_consumer_probe_from_provenance(
            fixture["provenance"][consumer], probe
        )
        common = {
            "profile": "authoritative",
            "evidence": producer_directory,
            "expect_source_sha": fixture["source_sha"],
            "expect_workflow_run": fixture["workflow_run"],
            "sources_lock": fixture["sources_lock"],
            "outputs_lock": fixture["outputs_lock"],
        }
        consumer_header = generated / "relocatable_world_consumer_input.hpp"
        evidence.prepare_consumer(
            **common,
            consumer=consumer,
            consumer_probe=probe,
            toolchain_artifact_sha256=self.TOOLCHAIN_ARTIFACTS[consumer],
            output_header=consumer_header,
        )
        self.assertIn(
            "fixture_context = false",
            consumer_header.read_text(encoding="utf-8"),
        )

        agreement_header = generated / "relocatable_world_agreement_input.hpp"
        evidence.prepare_agreements(
            **common, output_header=agreement_header
        )
        matrix_header = generated / "relocatable_world_matrix_input.hpp"
        evidence.prepare_matrix(
            **common,
            results=results_directory,
            agreements=agreement_directory / "agreements.json",
            output_header=matrix_header,
        )
        self.assertIn(
            "fixture_context = false",
            matrix_header.read_text(encoding="utf-8"),
        )

    def test_task3_generated_strings_are_length_aware_numeric_arrays(self):
        node = "arm64_linux_clang"
        bundle = self.make_ready_bundle(node)
        hostile = 'layout";\n#error injected\\path\u2603'
        facts = evidence.load_json(bundle["facts"])
        facts["contracts"]["world"]["signatures"]["Entity"] = hostile
        self.write_json(bundle["facts"], facts)
        signature_text = bundle["signature"].read_text(encoding="utf-8")
        bundle["signature"].write_text(
            signature_text.replace(
                json.dumps("[64-le]entity"), json.dumps(hostile)
            ),
            encoding="utf-8",
        )
        record = self.seal(bundle)

        producer_directory = self.directory / "hostile producer evidence"
        generated = self.directory / "hostile generated"
        producer_directory.mkdir()
        generated.mkdir()
        for suffix in (
            ".provenance.json", ".sig.hpp", ".world.region", ".unit.region"
        ):
            shutil.copy2(
                self.directory / f"{node}{suffix}",
                producer_directory / f"{node}{suffix}",
            )
        probe = self.directory / "hostile-consumer-probe.json"
        self.write_consumer_probe_from_provenance(record, probe)
        output = generated / "relocatable_world_consumer_input.hpp"
        evidence.prepare_consumer(
            profile="authoritative",
            consumer=node,
            evidence=producer_directory,
            consumer_probe=probe,
            toolchain_artifact_sha256=self.TOOLCHAIN_ARTIFACTS[node],
            expect_source_sha="1" * 40,
            expect_workflow_run="123456789.1",
            sources_lock=bundle["sources_lock"],
            outputs_lock=bundle["outputs_lock"],
            output_header=output,
        )
        header = output.read_text(encoding="utf-8")
        self.assertNotIn(hostile, header)
        self.assertNotIn("#error injected", header)
        self.assertIn("static_cast<char>(0x", header)
        self.assertIn("std::string_view", header)

    def test_task3_audit_checks_flat_complete_authoritative_run(self):
        fixture = self.make_complete_matrix_run()
        result = evidence.audit_run(
            directory=fixture["directory"],
            expect_source_sha=fixture["source_sha"],
            expect_workflow_run=fixture["workflow_run"],
            sources_lock=fixture["sources_lock"],
            outputs_lock=fixture["outputs_lock"],
            expect_nodes=6,
            expect_pairs=15,
            expect_named_permits=60,
            expect_transfers=30,
        )
        self.assertEqual(result["status"], "PASS")
        with self.assertRaisesRegex(evidence.EvidenceError, "counts"):
            evidence.audit_run(
                directory=fixture["directory"],
                expect_source_sha=fixture["source_sha"],
                expect_workflow_run=fixture["workflow_run"],
                sources_lock=fixture["sources_lock"],
                outputs_lock=fixture["outputs_lock"],
                expect_nodes=6,
                expect_pairs=14,
                expect_named_permits=60,
                expect_transfers=30,
            )

        (fixture["directory"] / "workflow-run.txt").write_text(
            fixture["workflow_run"] + "\nextra\n", encoding="utf-8"
        )
        with self.assertRaisesRegex(evidence.EvidenceError, "metadata"):
            evidence.audit_run(
                directory=fixture["directory"],
                expect_source_sha=fixture["source_sha"],
                expect_workflow_run=fixture["workflow_run"],
                sources_lock=fixture["sources_lock"],
                outputs_lock=fixture["outputs_lock"],
                expect_nodes=6,
                expect_pairs=15,
                expect_named_permits=60,
                expect_transfers=30,
            )
        (fixture["directory"] / "workflow-run.txt").write_text(
            fixture["workflow_run"] + "\n", encoding="utf-8"
        )

        (fixture["directory"] / "nested").mkdir()
        with self.assertRaisesRegex(evidence.EvidenceError, "flat"):
            evidence.audit_run(
                directory=fixture["directory"],
                expect_source_sha=fixture["source_sha"],
                expect_workflow_run=fixture["workflow_run"],
                sources_lock=fixture["sources_lock"],
                outputs_lock=fixture["outputs_lock"],
                expect_nodes=6,
                expect_pairs=15,
                expect_named_permits=60,
                expect_transfers=30,
            )

    def test_task3_audit_requires_profile_specific_run_metadata(self):
        profile_counts = {
            "authoritative": (6, 15, 60, 30),
            "local-arm64-macos": (5, 10, 40, 20),
        }
        for profile, counts in profile_counts.items():
            fixture = self.make_complete_matrix_run(profile)
            run_name = (
                "workflow-run.txt"
                if profile == "authoritative"
                else "run-id.txt"
            )
            forbidden_run_name = (
                "run-id.txt"
                if profile == "authoritative"
                else "workflow-run.txt"
            )

            def audit(directory):
                return evidence.audit_run(
                    directory=directory,
                    expect_source_sha=fixture["source_sha"],
                    expect_workflow_run=fixture["workflow_run"],
                    sources_lock=fixture["sources_lock"],
                    outputs_lock=fixture["outputs_lock"],
                    expect_nodes=counts[0],
                    expect_pairs=counts[1],
                    expect_named_permits=counts[2],
                    expect_transfers=counts[3],
                )

            self.assertEqual(audit(fixture["directory"])["status"], "PASS")
            cases = {
                "missing-both": "missing fixed files",
                "missing-source": "missing fixed files",
                "missing-run": "missing fixed files",
                "wrong-source": "metadata",
                "wrong-run": "metadata",
                "forbidden-run-name": "unexpected flat files",
            }
            for case, error_pattern in cases.items():
                with self.subTest(profile=profile, case=case):
                    directory = self.directory / f"{profile}-{case}"
                    shutil.copytree(fixture["directory"], directory)
                    if case in ("missing-both", "missing-source"):
                        (directory / "source-sha.txt").unlink()
                    if case in ("missing-both", "missing-run"):
                        (directory / run_name).unlink()
                    if case == "wrong-source":
                        (directory / "source-sha.txt").write_text(
                            "2" * 40 + "\n", encoding="utf-8"
                        )
                    if case == "wrong-run":
                        (directory / run_name).write_text(
                            "wrong-run\n", encoding="utf-8"
                        )
                    if case == "forbidden-run-name":
                        (directory / forbidden_run_name).write_text(
                            fixture["workflow_run"] + "\n", encoding="utf-8"
                        )
                    with self.assertRaisesRegex(
                        evidence.EvidenceError, error_pattern
                    ):
                        audit(directory)

    def test_task3_audit_rejects_each_cross_artifact_mismatch(self):
        fixture = self.make_complete_matrix_run()
        baseline = fixture["directory"]
        cases = {}

        def alter_json(name, relative, mutation):
            destination = self.directory / name
            shutil.copytree(baseline, destination)
            path = destination / relative
            record = evidence.load_json(path)
            mutation(record)
            self.write_json(path, record)
            cases[name] = destination

        alter_json(
            "mixed source",
            "arm64_linux_gcc.provenance.json",
            lambda record: record["build"].__setitem__("source_sha", "3" * 40),
        )
        alter_json(
            "mixed attempt",
            "arm64_linux_gcc.provenance.json",
            lambda record: record["build"].__setitem__(
                "workflow_run", "123456789.2"
            ),
        )
        alter_json(
            "wrong manifest",
            "arm64_linux_gcc.results.json",
            lambda record: record["build"].__setitem__(
                "toolchain_artifact_sha256", "0" * 64
            ),
        )
        alter_json(
            "wrong lock hash",
            "arm64_linux_gcc.results.json",
            lambda record: record["build"].__setitem__(
                "sources_sha256", "0" * 64
            ),
        )
        alter_json(
            "wrong mac archive",
            "arm64_macos_clang.results.json",
            lambda record: record["build"].__setitem__(
                "toolchain_artifact_sha256", "0" * 64
            ),
        )
        alter_json(
            "compiler mismatch",
            "x86_64_linux_clang.results.json",
            lambda record: record["build"].__setitem__(
                "compiler_version", "wrong compiler"
            ),
        )
        alter_json(
            "flags mismatch",
            "x86_64_linux_clang.results.json",
            lambda record: record["build"].__setitem__("flags", "-O0"),
        )
        alter_json(
            "xcode build mismatch",
            "arm64_macos_clang.results.json",
            lambda record: record["build"].__setitem__("xcode_build", "wrong"),
        )
        alter_json(
            "sdk build mismatch",
            "arm64_macos_clang.results.json",
            lambda record: record["build"].__setitem__("sdk_build", "wrong"),
        )
        alter_json(
            "duplicate transfer",
            "arm64_linux_clang.results.json",
            lambda record: record["transfers"][1].__setitem__(
                "producer", record["transfers"][0]["producer"]
            ),
        )
        alter_json(
            "non permit",
            "agreements.json",
            lambda record: record["pairs"][0]["scenarios"]["world"][0].__setitem__(
                "status", "REJECT"
            ),
        )
        alter_json(
            "stale agreement",
            "agreements.json",
            lambda record: record["pairs"][0]["scenarios"]["world"][0].__setitem__(
                "reason", "stale decision"
            ),
        )
        alter_json(
            "non pass",
            "arm64_linux_clang.results.json",
            lambda record: record["transfers"][0].__setitem__(
                "status", "REJECT_GRAPH"
            ),
        )
        alter_json(
            "closure mismatch",
            "closure.json",
            lambda record: record["counts"].__setitem__("passes", 29),
        )

        altered_region = self.directory / "altered region"
        shutil.copytree(baseline, altered_region)
        (altered_region / "x86_64_linux_gcc.unit.region").write_bytes(b"altered")
        cases["altered region"] = altered_region
        missing_agreement = self.directory / "missing agreement"
        shutil.copytree(baseline, missing_agreement)
        (missing_agreement / "agreements.json").unlink()
        cases["missing agreement"] = missing_agreement

        for name, directory in cases.items():
            with self.subTest(name=name), self.assertRaises(evidence.EvidenceError):
                evidence.audit_run(
                    directory=directory,
                    expect_source_sha=fixture["source_sha"],
                    expect_workflow_run=fixture["workflow_run"],
                    sources_lock=fixture["sources_lock"],
                    outputs_lock=fixture["outputs_lock"],
                    expect_nodes=6,
                    expect_pairs=15,
                    expect_named_permits=60,
                    expect_transfers=30,
                )

    def test_task7_local_profile_is_exact_complete_non_authoritative_graph(self):
        expected_nodes = (
            "x86_64_linux_gcc",
            "x86_64_linux_clang",
            "arm64_linux_gcc",
            "arm64_linux_clang",
            "arm64_macos_clang",
        )
        self.assertEqual(evidence.profile_nodes("local-arm64-macos"), expected_nodes)
        self.assertEqual(set(evidence.NODES) - set(expected_nodes), {"x86_64_macos_clang"})

        fallback_path = self.directory / "local-fallback-closure.json"
        evidence.write_fallback_closure(
            "local-arm64-macos", "local run not evaluated", fallback_path
        )
        fallback = evidence.validate_closure(fallback_path)
        self.assertFalse(fallback["authoritative"])
        self.assertEqual(len(fallback["expected"]["nodes"]), 5)
        self.assertEqual(len(fallback["expected"]["pairs"]), 20)
        self.assertEqual(len(fallback["expected"]["named_decisions"]), 80)
        self.assertEqual(len(fallback["expected"]["transfers"]), 40)

        fixture = self.make_complete_matrix_run("local-arm64-macos")

        def audit(directory):
            return evidence.audit_run(
                directory=directory,
                expect_source_sha=fixture["source_sha"],
                expect_workflow_run=fixture["workflow_run"],
                sources_lock=fixture["sources_lock"],
                outputs_lock=fixture["outputs_lock"],
                expect_nodes=5,
                expect_pairs=10,
                expect_named_permits=40,
                expect_transfers=20,
            )

        closure = audit(fixture["directory"])
        self.assertEqual(closure["status"], "PASS")
        self.assertFalse(closure["authoritative"])
        for scenario in evidence.SCENARIOS:
            self.assertEqual(
                closure["counts"]["scenarios"][scenario]["named_permits"], 40
            )
            self.assertEqual(
                closure["counts"]["scenarios"][scenario]["passes"], 20
            )

        for node in expected_nodes:
            with self.subTest(missing=node):
                path = fixture["directory"] / f"{node}.provenance.json"
                saved = path.read_bytes()
                path.unlink()
                try:
                    with self.assertRaisesRegex(evidence.EvidenceError, "missing fixed files"):
                        audit(fixture["directory"])
                finally:
                    path.write_bytes(saved)

        unexpected = fixture["directory"] / "x86_64_macos_clang.provenance.json"
        unexpected.write_text("{}\n", encoding="utf-8")
        try:
            with self.assertRaisesRegex(evidence.EvidenceError, "unexpected flat files"):
                audit(fixture["directory"])
        finally:
            unexpected.unlink()

        rejected_agreement = self.directory / "local rejected agreement"
        shutil.copytree(fixture["directory"], rejected_agreement)
        agreement_path = rejected_agreement / "agreements.json"
        agreement = evidence.load_json(agreement_path)
        agreement["pairs"][0]["scenarios"]["unit_handoff"][0]["status"] = "REJECT"
        agreement["pairs"][0]["scenarios"]["unit_handoff"][0]["reason"] = "test rejection"
        self.write_json(agreement_path, agreement)
        with self.assertRaisesRegex(evidence.EvidenceError, "Agreement"):
            audit(rejected_agreement)

        rejected_transfer = self.directory / "local rejected transfer"
        shutil.copytree(fixture["directory"], rejected_transfer)
        result_path = rejected_transfer / "arm64_linux_gcc.results.json"
        result = evidence.load_json(result_path)
        result["transfers"][0]["scenarios"]["unit_handoff"]["status"] = "REJECT_GRAPH"
        result["transfers"][0]["scenarios"]["unit_handoff"]["reason"] = "test rejection"
        self.write_json(result_path, result)
        with self.assertRaisesRegex(evidence.EvidenceError, "non-PASS"):
            audit(rejected_transfer)

    def test_task7_launcher_static_contract_is_immutable_five_node_profile(self):
        launcher = (
            Path(__file__).resolve().parents[1]
            / "tools"
            / "run-relocatable-world.sh"
        )
        text = launcher.read_text(encoding="utf-8")
        lowered = text.lower()
        for forbidden in (":latest", "--fixture-context", "xoffset", "submodule"):
            self.assertNotIn(forbidden, lowered)

        mapping_match = re.search(
            r"readonly -a LINUX_NODE_SPECS=\(\n(?P<body>.*?)\n\)", text, re.DOTALL
        )
        self.assertIsNotNone(mapping_match)
        specs = re.findall(r'^\s*"([^\"]+)"\s*$', mapping_match.group("body"), re.MULTILINE)
        self.assertEqual(
            specs,
            [
                "x86_64_linux_gcc|linux/amd64|gcc|emulated",
                "x86_64_linux_clang|linux/amd64|p2996|emulated",
                "arm64_linux_gcc|linux/arm64|gcc|native",
                "arm64_linux_clang|linux/arm64|p2996|native",
            ],
        )
        for required_path in (
            ".gitattributes",
            "CMakeLists.txt",
            "cmake",
            "include/boost/typelayout",
            "include/boost/typelayout.hpp",
            "example/relocatable_world_demo",
            "tools/relocatable_world_evidence.py",
            "tools/run-relocatable-world.sh",
            ".github/docker/toolchain-sources.lock",
            ".github/docker/toolchains.lock",
            ".github/scripts/validate-toolchain-locks.py",
            ".github/scripts/macos-runtime-origin-probe.cpp",
            ".github/scripts/verify-p2996-toolchain.sh",
        ):
            self.assertIn(required_path, text)
        for required in (
            "git diff --quiet HEAD --",
            "git ls-files --eol --",
            "git ls-files -s --",
            "100755",
            "docker buildx version",
            "xcode-select -p",
            "xcodebuild -version",
            "xcrun --sdk macosx --show-sdk-path",
            'python3 "${LOCK_VALIDATOR}"',
            'bash -n "${MACOS_VERIFIER}"',
            "read:packages",
            "curl --config -",
            "--password-stdin",
            "--allow-unlocked-sdk",
            "shlex.split",
            "shlex.join",
            "macOS verifier identity field",
            "macOS verifier environment shape is not exact",
            "--profile local-arm64-macos",
            "--expect-nodes 5",
            "--expect-pairs 10",
            "--expect-named-permits 40",
            "--expect-transfers 20",
            "fallback-provenance",
            "fallback-results",
            "fallback-agreements",
            "fallback-closure",
            "DYLD_PRINT_LIBRARIES=1",
            "otool -L",
            "otool -l",
        ):
            self.assertIn(required, text)
        self.assertGreaterEqual(text.count("--allow-unlocked-sdk"), 2)
        self.assertIn(
            "LOCAL COVERAGE 5/6: 3 native-architecture + 2 Docker-emulated; "
            "WORLD Agreement 40/40 transfers 20/20; "
            "UNIT Agreement 40/40 handoffs 20/20; "
            "authoritative closure unavailable",
            text,
        )

    def test_task7_launcher_copies_validated_agreement_outside_evidence_for_matrix(self):
        text = self.launcher_text()
        self.assertIn("# BEGIN TASK7 AGREEMENT BYTE COPY", text)
        script = self.launcher_python_block("AGREEMENT BYTE COPY")

        evidence_directory = self.directory / "evidence output with spaces"
        source = evidence_directory / "agreements.json"
        destination = (
            self.directory / "independent preflight with spaces" / "agreements.json"
        )
        evidence_directory.mkdir()
        original = b'{"schema":1,"sentinel":"exact bytes"}\n'
        source.write_bytes(original)

        completed = subprocess.run(
            [sys.executable, "-c", script, str(source), str(destination)],
            capture_output=True,
            text=True,
            check=False,
        )
        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertEqual(destination.read_bytes(), original)
        self.assertEqual(source.read_bytes(), original)
        self.assertNotEqual(destination.parent, evidence_directory)

        validation = text.index("record = module.validate_agreements(sys.argv[2])")
        copy = text.index("# BEGIN TASK7 AGREEMENT BYTE COPY")
        matrix = text.index('python3 "${EVIDENCE_TOOL}" prepare-matrix')
        self.assertLess(validation, copy)
        self.assertLess(copy, matrix)
        self.assertIn(
            'agreement_matrix_input="${preflight_root}/validated-agreement/agreements.json"',
            text,
        )
        matrix_command = text[matrix : text.index("c++ -std=c++20", matrix)]
        self.assertIn('--agreements "${agreement_matrix_input}"', matrix_command)
        self.assertNotIn(
            '--agreements "${output_directory}/agreements.json"', matrix_command
        )

    def test_task7_macos_final_runtime_validator_rejects_mixed_or_incomplete_logs(self):
        text = self.launcher_text()
        self.assertIn("# BEGIN TASK7 MACOS FINAL RUNTIME VALIDATOR", text)
        script = self.launcher_python_block("MACOS FINAL RUNTIME VALIDATOR")

        library_directory = self.directory / "toolchain root with spaces" / "lib"
        library_directory.mkdir(parents=True)
        expected_paths = []
        for name in ("libc++.1.dylib", "libc++abi.1.dylib", "libunwind.1.dylib"):
            path = library_directory / name
            path.write_bytes(b"runtime")
            expected_paths.append(str(path.resolve()))

        otool_libraries = self.directory / "final binary libraries.txt"
        otool_load_commands = self.directory / "final binary load commands.txt"
        dyld_libraries = self.directory / "final binary dyld.txt"
        valid_otool_libraries = (
            "/tmp/final executable:\n"
            "\t@rpath/libc++.1.dylib (compatibility version 1.0.0, "
            "current version 2100.0.0)\n"
            "\t/usr/lib/libSystem.B.dylib (compatibility version 1.0.0, "
            "current version 1345.100.2)\n"
        )
        valid_otool_load_commands = (
            "Load command 1\n"
            "          cmd LC_RPATH\n"
            "      cmdsize 80\n"
            f"         path {library_directory.resolve()} (offset 12)\n"
        )
        legacy_dyld = "\n".join(
            f"dyld[1]: {path}" for path in expected_paths
        ) + "\n"
        dyld4_uuid = "01234567-89AB-CDEF-0123-456789ABCDEF"
        valid_dyld = "\n".join(
            f"dyld[1]: <{dyld4_uuid}> {path}" for path in expected_paths
        ) + f"\ndyld[1]: <{dyld4_uuid}> /usr/lib/libSystem.B.dylib\n"

        def validate(libraries, load_commands, dyld):
            otool_libraries.write_text(libraries, encoding="utf-8")
            otool_load_commands.write_text(load_commands, encoding="utf-8")
            dyld_libraries.write_text(dyld, encoding="utf-8")
            return subprocess.run(
                [
                    sys.executable,
                    "-c",
                    script,
                    str(otool_libraries),
                    str(otool_load_commands),
                    str(dyld_libraries),
                    str(library_directory),
                ],
                capture_output=True,
                text=True,
                check=False,
            )

        legacy = validate(
            valid_otool_libraries, valid_otool_load_commands, legacy_dyld
        )
        self.assertEqual(legacy.returncode, 0, legacy.stderr)
        dyld4 = validate(
            valid_otool_libraries, valid_otool_load_commands, valid_dyld
        )
        self.assertEqual(dyld4.returncode, 0, dyld4.stderr)

        attacks = (
            (
                valid_otool_libraries
                + "\t/usr/lib/libc++.1.dylib (compatibility version 1.0.0, "
                "current version 1700.0.0)\n",
                valid_otool_load_commands,
                valid_dyld,
            ),
            (
                valid_otool_libraries,
                valid_otool_load_commands
                + "Load command 2\n"
                "          cmd LC_RPATH\n"
                "      cmdsize 48\n"
                "         path /usr/lib (offset 12)\n",
                valid_dyld,
            ),
            (
                valid_otool_libraries,
                valid_otool_load_commands,
                valid_dyld
                + f"dyld[1]: <{dyld4_uuid}> /usr/lib/libc++.1.dylib\n",
            ),
            (
                valid_otool_libraries,
                valid_otool_load_commands,
                valid_dyld
                + "dyld[1]: unexpected-prefix /usr/lib/libc++.1.dylib\n",
            ),
            (
                valid_otool_libraries,
                valid_otool_load_commands,
                valid_dyld
                + "dyld[1]: <not-a-uuid> /usr/lib/libSystem.B.dylib\n",
            ),
            (
                valid_otool_libraries,
                valid_otool_load_commands,
                "\n".join(
                    f"dyld[1]: <{dyld4_uuid}> {path}"
                    for path in expected_paths[:-1]
                )
                + "\n",
            ),
        )
        for attack in attacks:
            with self.subTest(attack=attack):
                rejected = validate(*attack)
                self.assertNotEqual(rejected.returncode, 0, rejected.stderr)

    def test_task7_amd64_smoke_validator_requires_both_exact_contracts(self):
        text = self.launcher_text()
        self.assertIn("# BEGIN TASK7 AMD64 SMOKE VALIDATOR", text)
        script = self.launcher_python_block("AMD64 SMOKE VALIDATOR")
        probe_path = self.directory / "amd64 smoke probe.json"
        environment = os.environ.copy()
        environment.update(
            {
                "TYPELAYOUT_SMOKE_NODE": "x86_64_linux_gcc",
                "COMPILER_FAMILY": "gcc",
                "COMPILER_REVISION": "16.2.0",
                "COMPILER_VERSION": "16.2.0",
                "COMPILER_TARGET": "x86_64-linux-gnu",
                "COMPILER_STDLIB": "libstdc++-20260801",
            }
        )
        base = {
            "node": "x86_64_linux_gcc",
            "probe": {
                "char_bit": 8,
                "pointer_bits": 64,
                "endian": "little",
                "reflection": True,
                "memcpy_object_lifetime": True,
                "memcpy_array_lifetime": True,
            },
            "compiler": {
                "family": "gcc",
                "revision": "16.2.0",
                "version": "16.2.0",
                "target": "x86_64-linux-gnu",
                "stdlib": "libstdc++-20260801",
            },
        }

        def validate(contracts):
            self.write_json(
                probe_path, {**base, "schema": 2, "contracts": contracts}
            )
            return subprocess.run(
                [sys.executable, "-c", script, str(probe_path)],
                capture_output=True,
                text=True,
                check=False,
                env=environment,
            )

        expected = {
            scenario: {
                key: True for key in evidence.SCENARIO_KEYS[scenario]
            }
            for scenario in evidence.SCENARIOS
        }
        accepted = validate(expected)
        self.assertEqual(accepted.returncode, 0, accepted.stderr)
        attacks = (
            {},
            {"world": expected["world"]},
            {**expected, "unexpected": {}},
            {
                **expected,
                "unit_handoff": {
                    **expected["unit_handoff"],
                    "Effect": False,
                },
            },
        )
        for contracts in attacks:
            with self.subTest(contracts=contracts):
                rejected = validate(contracts)
                self.assertNotEqual(rejected.returncode, 0, rejected.stderr)

    def test_task7_launcher_derives_unique_defaults_and_preserves_explicit_identity(self):
        repository, fake_bin, tool_log, head = self.make_launcher_repository()

        first = self.run_launcher(repository, fake_bin, tool_log)
        second = self.run_launcher(repository, fake_bin, tool_log)
        for completed in (first, second):
            combined = completed.stdout + completed.stderr
            self.assertNotEqual(completed.returncode, 0, combined)
            self.assertIn("private GHCR credentials", combined)
            self.assertRegex(combined, rf"(?m)^SOURCE SHA: {head}$")
        first_run = re.search(
            r"(?m)^RUN ID: (local-[0-9a-f]{12}-[0-9]{8}T[0-9]{6}Z-[0-9]+)$",
            first.stdout + first.stderr,
        )
        second_run = re.search(
            r"(?m)^RUN ID: (local-[0-9a-f]{12}-[0-9]{8}T[0-9]{6}Z-[0-9]+)$",
            second.stdout + second.stderr,
        )
        self.assertIsNotNone(first_run)
        self.assertIsNotNone(second_run)
        self.assertNotEqual(first_run.group(1), second_run.group(1))

        explicit_run = "manual-arm64-macos-20260828"
        explicit = self.run_launcher(
            repository,
            fake_bin,
            tool_log,
            "--source-sha",
            head,
            "--run-id",
            explicit_run,
        )
        combined = explicit.stdout + explicit.stderr
        self.assertNotEqual(explicit.returncode, 0, combined)
        self.assertRegex(combined, rf"(?m)^SOURCE SHA: {head}$")
        self.assertRegex(combined, rf"(?m)^RUN ID: {explicit_run}$")

    def test_task7_launcher_rejects_other_commit_before_work_or_evidence(self):
        repository, fake_bin, tool_log, head = self.make_launcher_repository()
        other = "0" * 40 if head != "0" * 40 else "1" * 40
        completed = self.run_launcher(
            repository,
            fake_bin,
            tool_log,
            "--source-sha",
            other,
            "--run-id",
            "mismatched-source",
        )
        combined = completed.stdout + completed.stderr
        self.assertNotEqual(completed.returncode, 0, combined)
        self.assertIn("does not equal current HEAD", combined)
        self.assertFalse((repository / "build" / "relocatable-world-local").exists())
        log = tool_log.read_text(encoding="utf-8") if tool_log.exists() else ""
        for forbidden_action in ("docker login", "docker pull", "docker run", "cmake "):
            self.assertNotIn(forbidden_action, log)

    def test_task7_launcher_rechecks_clean_state_before_evidence_creation(self):
        repository, fake_bin, tool_log, _ = self.make_launcher_repository()
        completed = self.run_launcher(
            repository,
            fake_bin,
            tool_log,
            environment={
                "TYPELAYOUT_GHCR_USER": "test-user",
                "TYPELAYOUT_GHCR_TOKEN": "github_pat_launcher_read_packages_token",
                "TYPELAYOUT_FAKE_VERIFIER_SUCCEED": "1",
                "TYPELAYOUT_DIRTY_BEFORE_EVIDENCE": "1",
            },
        )
        combined = completed.stdout + completed.stderr
        self.assertNotEqual(completed.returncode, 0, combined)
        self.assertRegex(
            combined,
            r"executable source (?:differs from HEAD|contains uncommitted)",
        )
        self.assertFalse((repository / "build" / "relocatable-world-local").exists())
        log = tool_log.read_text(encoding="utf-8")
        self.assertEqual(log.count("verify "), 2, log)

    def test_task7_launcher_rechecks_implementation_at_three_required_phases(self):
        text = self.launcher_text()
        self.assertIn("verify_implementation_state() {", text)
        calls = [
            match.start()
            for match in re.finditer(
                r"(?m)^verify_implementation_state$", text
            )
        ]
        self.assertEqual(len(calls), 3)
        output_check = text.index(
            'output_directory="${repository_root}/${OUTPUT_RELATIVE}"'
        )
        create_output = text.index('mkdir -p "$(dirname -- "${output_directory}")"')
        audit = text.index('python3 "${EVIDENCE_TOOL}" audit-run')
        final_line = text.index("printf '%s\\n' \"${FINAL_LINE}\"")
        self.assertLess(calls[0], output_check)
        self.assertLess(calls[1], create_output)
        self.assertEqual(text[calls[1] + len("verify_implementation_state") : create_output].strip(), "")
        self.assertLess(audit, calls[2])
        self.assertLess(calls[2], final_line)

    def test_task7_launcher_rejects_invalid_explicit_identity_values(self):
        repository, fake_bin, tool_log, head = self.make_launcher_repository()
        cases = (
            (("--source-sha", ""), "--source-sha must be"),
            (
                ("--source-sha", head, "--run-id", ""),
                "--run-id must use",
            ),
            (
                ("--source-sha", head, "--run-id", "123456789.1"),
                "--run-id must not use",
            ),
        )
        for arguments, message in cases:
            with self.subTest(arguments=arguments):
                completed = self.run_launcher(
                    repository, fake_bin, tool_log, *arguments
                )
                combined = completed.stdout + completed.stderr
                self.assertNotEqual(completed.returncode, 0, combined)
                self.assertIn(message, combined)
                self.assertFalse(
                    (repository / "build" / "relocatable-world-local").exists()
                )

    def test_task7_launcher_missing_credential_pair_never_prints_token(self):
        repository, fake_bin, tool_log, _ = self.make_launcher_repository()
        secret = "launcher-secret-must-not-appear"
        completed = self.run_launcher(
            repository,
            fake_bin,
            tool_log,
            environment={"TYPELAYOUT_GHCR_TOKEN": secret},
            xtrace=True,
        )
        combined = completed.stdout + completed.stderr
        self.assertNotEqual(completed.returncode, 0, combined)
        self.assertIn("private GHCR credentials", combined)
        self.assertNotIn(secret, combined)
        log = tool_log.read_text(encoding="utf-8") if tool_log.exists() else ""
        self.assertNotIn(secret, log)
        self.assertNotIn("docker login", log)
        self.assertFalse((repository / "build" / "relocatable-world-local").exists())

    def test_task7_launcher_disables_allexport_before_copying_credentials(self):
        repository, fake_bin, tool_log, _ = self.make_launcher_repository()
        secret = "github_pat_launcher_read_packages_token"
        completed = self.run_launcher(
            repository,
            fake_bin,
            tool_log,
            environment={
                "TYPELAYOUT_GHCR_USER": "test-user",
                "TYPELAYOUT_GHCR_TOKEN": secret,
                "explicit_token": "inherited-explicit-token-name",
                "ghcr_token": "inherited-ghcr-token-name",
            },
            xtrace=True,
            allexport=True,
        )
        combined = completed.stdout + completed.stderr
        self.assertNotEqual(completed.returncode, 0, combined)
        log = tool_log.read_text(encoding="utf-8") if tool_log.exists() else ""
        self.assertNotIn("inherited credential environment", log)
        self.assertIn("verify ", log)
        self.assertNotIn(secret, combined)
        self.assertNotIn(secret, log)

    def test_task7_launcher_rejects_unsafe_explicit_credentials_without_leaking(self):
        repository, fake_bin, tool_log, _ = self.make_launcher_repository()
        secret = "launcher-secret-must-not-appear\ninjected-curl-config"
        completed = self.run_launcher(
            repository,
            fake_bin,
            tool_log,
            environment={
                "TYPELAYOUT_GHCR_USER": "test-user",
                "TYPELAYOUT_GHCR_TOKEN": secret,
            },
            xtrace=True,
        )
        combined = completed.stdout + completed.stderr
        self.assertNotEqual(completed.returncode, 0, combined)
        self.assertIn("private GHCR credential value is unsafe", combined)
        self.assertNotIn(secret, combined)
        self.assertNotIn("injected-curl-config", combined)
        log = tool_log.read_text(encoding="utf-8") if tool_log.exists() else ""
        self.assertNotIn("docker login", log)
        self.assertFalse((repository / "build" / "relocatable-world-local").exists())

    def test_task7_launcher_requires_both_amd64_smokes_before_evidence(self):
        repository, fake_bin, tool_log, _ = self.make_launcher_repository()
        completed = self.run_launcher(
            repository,
            fake_bin,
            tool_log,
            environment={
                "TYPELAYOUT_GHCR_USER": "test-user",
                "TYPELAYOUT_GHCR_TOKEN": "github_pat_launcher_read_packages_token",
                "TYPELAYOUT_FAIL_SECOND_SMOKE": "1",
            },
        )
        combined = completed.stdout + completed.stderr
        self.assertNotEqual(completed.returncode, 0, combined)
        log = tool_log.read_text(encoding="utf-8")
        smoke_lines = [
            line
            for line in log.splitlines()
            if line.startswith("docker run ") and "TYPELAYOUT_SMOKE_NODE=" in line
        ]
        self.assertEqual(len(smoke_lines), 2, log)
        self.assertIn("TYPELAYOUT_SMOKE_NODE=x86_64_linux_gcc", smoke_lines[0])
        self.assertIn("linux/amd64", smoke_lines[0])
        self.assertIn("TYPELAYOUT_SMOKE_NODE=x86_64_linux_clang", smoke_lines[1])
        self.assertIn("linux/amd64", smoke_lines[1])
        self.assertNotIn("verify ", log)
        self.assertFalse((repository / "build" / "relocatable-world-local").exists())


if __name__ == "__main__":
    unittest.main()
