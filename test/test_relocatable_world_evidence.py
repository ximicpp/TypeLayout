import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from tools import relocatable_world_evidence as evidence


class EvidenceTests(unittest.TestCase):
    maxDiff = None

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
            else "Bloomberg clang 20.0.0"
        )
        target = (
            "aarch64-unknown-linux-gnu"
            if node.startswith("arm64_linux")
            else "x86_64-unknown-linux-gnu"
        )
        stdlib = "libstdc++-20260801" if compiler_family == "gcc" else "libc++-200000"
        runner = "ubuntu-24.04-arm" if node.startswith("arm64_") else "ubuntu-24.04"
        runner_image = "ubuntu-24.04-20260818.1"

        sources_lock = self.directory / "toolchain-sources.lock"
        self.write_json(
            sources_lock,
            {
                "schema": 1,
                "nodes": {
                    node: {
                        "compiler_family": compiler_family,
                        "compiler_revision": revision,
                        "flags": "-O3 -fstrict-aliasing",
                    }
                },
            },
        )
        outputs_lock = self.directory / "toolchains.lock"
        self.write_json(
            outputs_lock,
            {
                "schema": 1,
                "sources_sha256": self.sha256(sources_lock),
                "nodes": {
                    node: {
                        "compiler_version": compiler_version,
                        "target": target,
                        "stdlib": stdlib,
                        "runner_image": runner_image,
                        "xcode": "none",
                        "sdk": "none",
                        "deployment_target": "none",
                    }
                },
            },
        )

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
                    "xcode": "none",
                    "sdk": "none",
                    "deployment_target": "none",
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
            "workflow_run": "123456789",
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
