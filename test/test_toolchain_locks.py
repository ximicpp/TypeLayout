import hashlib
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
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


class ToolchainLockTests(unittest.TestCase):
    maxDiff = None

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
            elif relative.endswith("Dockerfile.gcc16"):
                path.write_text(
                    "fixture\n"
                    + "\n".join(
                        PACKAGE_LOCKS["gcc_builder"]
                        + PACKAGE_LOCKS["gcc_runtime"]
                    )
                    + "\n",
                    encoding="utf-8",
                )
            elif relative.endswith("Dockerfile.p2996"):
                path.write_text(
                    "fixture\n"
                    + "\n".join(
                        PACKAGE_LOCKS["p2996_builder"]
                        + PACKAGE_LOCKS["p2996_runtime"]
                    )
                    + "\n",
                    encoding="utf-8",
                )
            else:
                path.write_text(f"fixture for {relative}\n", encoding="utf-8")

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
            f"clang={CLANG_COMMIT} recipes=7",
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
        self.assertEqual(lock["p2996"]["projects"], ["clang"])
        self.assertEqual(
            lock["p2996"]["runtimes"], ["libcxx", "libcxxabi", "libunwind"]
        )
        self.assertEqual(
            lock["macos"]["nodes"]["arm64_macos_clang"]["sdk_build"],
            "24F74",
        )
        self.assertEqual(set(lock["recipes"]), set(RECIPE_PATHS))

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
        self.assertIn("shlex.split(flags)", verify_content)


if __name__ == "__main__":
    unittest.main()
