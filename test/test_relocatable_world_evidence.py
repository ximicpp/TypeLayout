import hashlib
import json
from pathlib import Path
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
                },
                "p2996": {
                    "repository": "https://github.com/bloomberg/clang-p2996.git",
                    "commit": "060be17654102019e14810c3f948ef85a490755f",
                    "compiler_family": "clang",
                    "compiler_revision": (
                        "060be17654102019e14810c3f948ef85a490755f"
                    ),
                    "flags": "-O3 -fstrict-aliasing -stdlib=libc++",
                },
                "linux": {
                    "platforms": {
                        "linux/amd64": {"architecture": "x86_64"},
                        "linux/arm64": {"architecture": "arm64"},
                    },
                    "docker": {
                        "client_version": "27.5.1",
                        "server_version": "27.5.1",
                        "buildx_version": "0.24.0",
                        "buildkit_image": "moby/buildkit@sha256:" + "9" * 64,
                    },
                },
                "macos": {
                    "nodes": {
                        "arm64_macos_clang": {
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
                    }
                },
                "actions": {},
                "recipes": {},
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
                        "compiler_version": "gcc 16.2.0",
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
        }
        admission = {key: True for key in evidence.KEYS}
        compiler_family = "gcc" if node.endswith("_gcc") else "clang"
        revision = (
            "16.2.0"
            if compiler_family == "gcc"
            else "060be17654102019e14810c3f948ef85a490755f"
        )
        compiler_version = (
            "gcc 16.2.0"
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
                "schema": 1,
                "node": node,
                "probe": {
                    "char_bit": 8,
                    "pointer_bits": 64,
                    "endian": "little",
                    "reflection": True,
                    "memcpy_object_lifetime": True,
                    "memcpy_array_lifetime": True,
                },
                "admission": admission,
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
                "schema": 1,
                "node": node,
                "admission": admission,
                "signatures": signatures,
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

        region = self.directory / f"{node}.region"
        region.write_bytes(b"TLWORLD\0fixture")
        output = self.directory / f"{node}.provenance.json"
        return {
            "node": node,
            "profile": "authoritative",
            "execution": "native",
            "probe": probe,
            "facts": facts,
            "signature": signature,
            "region": region,
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
                "admission",
                "signatures",
                "compiler",
                "build",
                "locks",
                "artifacts",
            },
        )
        self.assertEqual(list(record["admission"]), list(evidence.KEYS))
        self.assertEqual(list(record["signatures"]), list(evidence.KEYS))
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
                "region": {
                    "filename": bundle["region"].name,
                    "sha256": self.sha256(bundle["region"]),
                },
            },
        )
        evidence.validate_provenance(bundle["output"])
        with bundle["region"].open("ab") as stream:
            stream.write(b"x")
        with self.assertRaisesRegex(evidence.EvidenceError, "region.*SHA256"):
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
        self.assertEqual(set(record["admission"]), set(evidence.KEYS))
        self.assertEqual(set(record["signatures"]), set(evidence.KEYS))
        self.assertGreater(bundle["region"].stat().st_size, 0)

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
        bundle["region"].write_bytes(b"")

        with self.assertRaisesRegex(evidence.EvidenceError, "region.*empty"):
            evidence.validate_producer_artifacts(
                bundle["node"], self.directory
            )

    def test_ready_provenance_requires_exact_contract_keys(self):
        bundle = self.make_ready_bundle()
        self.seal(bundle)
        record = json.loads(bundle["output"].read_text(encoding="utf-8"))
        record["admission"]["Unexpected"] = True
        self.write_json(bundle["output"], record)
        with self.assertRaisesRegex(evidence.EvidenceError, "admission.*keys"):
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
        record["artifacts"]["region"]["sha256"] = "ABC123"
        self.write_json(bundle["output"], record)
        with self.assertRaisesRegex(evidence.EvidenceError, "SHA256"):
            evidence.validate_provenance(bundle["output"])

    def test_artifact_filename_escape_is_rejected(self):
        bundle = self.make_ready_bundle()
        self.seal(bundle)
        record = json.loads(bundle["output"].read_text(encoding="utf-8"))
        record["artifacts"]["region"]["filename"] = "../escaped.region"
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
        record["admission"]["Entity"] = False
        self.write_json(bundle["output"], record)
        with self.assertRaisesRegex(evidence.EvidenceError, "REJECT.*artifacts"):
            evidence.validate_provenance(bundle["output"])

    def test_incomplete_provenance_cannot_invent_signatures(self):
        provenance = self.directory / "arm64_linux_gcc.provenance.json"
        self.write_json(
            provenance,
            {
                "schema": 1,
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
            record["admission"]["Entity"] = False
            self.write_json(bundle[path_key], record)
        bundle["signature"].unlink()
        bundle["region"].unlink()
        del bundle["signature"]
        del bundle["region"]

        record = self.seal(bundle)

        self.assertEqual(record["status"], "REJECT")
        self.assertFalse(record["admission"]["Entity"])
        self.assertEqual(record["artifacts"], {})
        self.assertEqual(record["signatures"]["Entity"], "[64-le]entity")

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
            policy, _, _ = evidence._load_lock_policy(
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
                "--region",
                str(bundle["region"]),
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
        bundle = self.make_ready_bundle("arm64_linux_gcc")
        bundle["profile"] = "local-arm64-macos"
        bundle["workflow_run"] = (
            "local-111111111111-20260828T051234Z-4242"
        )
        self.assertEqual(self.seal(bundle)["status"], "READY")

        invalid_values = (
            "123456789.1",
            "local-111111111111-20260828T051234Z-0",
            "local-222222222222-20260828T051234Z-4242",
            "local-111111111111-20260828T051234Z-4242-extra",
            "local-111111111111-20260828T051234Z-4242 ",
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
        with self.assertRaisesRegex(evidence.EvidenceError, "exactly four contract"):
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
            all(transfer["status"] == "INCOMPLETE" for transfer in record["transfers"])
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
                [decision["key"] for decision in pair["decisions"]]
                == list(evidence.KEYS)
                for pair in record["pairs"]
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
                "schema": 1,
                "node": "x86_64_linux_clang",
                "status": "INCOMPLETE",
                "error": "probe did not run",
            },
        )


if __name__ == "__main__":
    unittest.main()
