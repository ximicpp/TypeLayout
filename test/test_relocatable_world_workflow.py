import re
import unittest
from pathlib import Path

import yaml


ROOT = Path(__file__).resolve().parents[1]
WORKFLOW = ROOT / ".github/workflows/relocatable-world-matrix.yml"

NODES = {
    "x86_64_linux_gcc",
    "x86_64_linux_clang",
    "arm64_linux_gcc",
    "arm64_linux_clang",
    "arm64_macos_clang",
    "x86_64_macos_clang",
}
LINUX_NODES = {
    "x86_64_linux_gcc",
    "x86_64_linux_clang",
    "arm64_linux_gcc",
    "arm64_linux_clang",
}
MACOS_NODES = {"arm64_macos_clang", "x86_64_macos_clang"}


class RelocatableWorldWorkflowTests(unittest.TestCase):
    def workflow_text(self):
        self.assertTrue(WORKFLOW.is_file(), "authoritative workflow must exist")
        return WORKFLOW.read_text(encoding="utf-8")

    def workflow(self):
        loaded = yaml.load(self.workflow_text(), Loader=yaml.BaseLoader)
        self.assertIsInstance(loaded, dict)
        return loaded

    def job_text(self, name):
        match = re.search(
            rf"(?ms)^  {re.escape(name)}:\n(.*?)(?=^  [a-zA-Z0-9_]+:\n|\Z)",
            self.workflow_text(),
        )
        self.assertIsNotNone(match, f"missing workflow job {name}")
        return match.group(1)

    @staticmethod
    def matrix_nodes(job):
        include = job["strategy"]["matrix"]["include"]
        return {record["node"] for record in include}

    def test_workflow_has_exact_authoritative_dag(self):
        workflow = self.workflow()
        jobs = workflow["jobs"]
        self.assertEqual(
            set(jobs),
            {
                "producer_linux",
                "producer_macos",
                "consumer_linux",
                "consumer_macos",
                "agreement",
                "closure",
            },
        )
        self.assertEqual(self.matrix_nodes(jobs["producer_linux"]), LINUX_NODES)
        self.assertEqual(self.matrix_nodes(jobs["producer_macos"]), MACOS_NODES)
        self.assertEqual(self.matrix_nodes(jobs["consumer_linux"]), LINUX_NODES)
        self.assertEqual(self.matrix_nodes(jobs["consumer_macos"]), MACOS_NODES)
        for job in (
            "producer_linux",
            "producer_macos",
            "consumer_linux",
            "consumer_macos",
        ):
            self.assertEqual(jobs[job]["strategy"]["fail-fast"], "false")
        expected_needs = {
            "consumer_linux": {"producer_linux", "producer_macos"},
            "consumer_macos": {"producer_linux", "producer_macos"},
            "agreement": {"producer_linux", "producer_macos"},
            "closure": {
                "producer_linux",
                "producer_macos",
                "consumer_linux",
                "consumer_macos",
                "agreement",
            },
        }
        for job, needs in expected_needs.items():
            self.assertEqual(set(jobs[job]["needs"]), needs)
            self.assertEqual(
                jobs[job]["if"], "${{ always() && !cancelled() }}"
            )

    def test_triggers_cover_every_authoritative_input(self):
        workflow = self.workflow()
        events = workflow["on"]
        self.assertIn("workflow_dispatch", events)
        push = events["push"]
        self.assertEqual(
            set(push["branches"]), {"main", "codex/cppcon2026-deck"}
        )
        required = {
            ".gitattributes",
            "CMakeLists.txt",
            "cmake/**",
            "include/**",
            "example/relocatable_world_demo/**",
            "tools/relocatable_world_evidence.py",
            "tools/run-relocatable-world.sh",
            ".github/docker/**",
            ".github/scripts/bootstrap-toolchain-sources.py",
            ".github/scripts/build-p2996-macos.sh",
            ".github/scripts/validate-toolchain-locks.py",
            ".github/scripts/verify-p2996-toolchain.sh",
            ".github/workflows/toolchain-images.yml",
            ".github/workflows/relocatable-world-matrix.yml",
        }
        self.assertTrue(required.issubset(set(push["paths"])))

    def test_permissions_and_action_pins_are_least_privilege(self):
        workflow = self.workflow()
        self.assertEqual(workflow["permissions"], {"contents": "read"})
        jobs = workflow["jobs"]
        for name in ("producer_linux", "consumer_linux"):
            self.assertEqual(
                jobs[name]["permissions"],
                {"contents": "read", "packages": "read"},
            )
        for name in ("producer_macos", "consumer_macos", "agreement", "closure"):
            self.assertNotIn("permissions", jobs[name])
        text = self.workflow_text()
        pins = {
            "actions/checkout": "11d5960a326750d5838078e36cf38b85af677262",
            "actions/upload-artifact": "ea165f8d65b6e75b540449e92b4886f43607fa02",
            "actions/download-artifact": "d3f86a106a0bac45b974a628896c90dbdf5c8093",
            "docker/login-action": "c94ce9fb468520275223c153574b00df6fe4bcc9",
        }
        for action, commit in pins.items():
            self.assertNotRegex(text, rf"uses:\s*{re.escape(action)}@(?!{commit})")
            self.assertIn(f"uses: {action}@{commit}", text)
        self.assertNotRegex(text, r"(?m)^\s*(contents|packages|actions|id-token): write$")

    def test_fallbacks_upload_before_explicit_semantic_gates(self):
        requirements = {
            "producer_linux": ("fallback-provenance", "gate producer result"),
            "producer_macos": ("fallback-provenance", "gate producer result"),
            "consumer_linux": ("fallback-results", "gate consumer passes"),
            "consumer_macos": ("fallback-results", "gate consumer passes"),
            "agreement": ("fallback-agreements", "gate Agreement permits"),
            "closure": ("fallback-closure", "audit-run"),
        }
        for job, (fallback, gate) in requirements.items():
            with self.subTest(job=job):
                block = self.job_text(job)
                self.assertIn(fallback, block)
                upload = block.find("actions/upload-artifact@")
                self.assertGreater(upload, block.find(fallback))
                self.assertGreater(block.find(gate), upload)
                upload_start = block.rfind("\n      - ", 0, upload)
                upload_step = block[
                    upload_start : block.find("\n      - ", upload + 1)
                ]
                self.assertIn("if: ${{ always() }}", upload_step)
                self.assertIn("if-no-files-found: error", upload_step)

    def test_linux_uses_authenticated_digest_only_native_images(self):
        for job in ("producer_linux", "consumer_linux"):
            block = self.job_text(job)
            self.assertIn(
                "docker/login-action@c94ce9fb468520275223c153574b00df6fe4bcc9",
                block,
            )
            self.assertIn("registry: ghcr.io", block)
            self.assertIn("password: ${{ secrets.GITHUB_TOKEN }}", block)
            self.assertIn("imagetools inspect", block)
            self.assertIn("--raw", block)
            self.assertIn("MANIFEST_DIGEST", block)
            self.assertIn('IMAGE_REF="${REPOSITORY}@sha256:${MANIFEST_DIGEST}"', block)
            self.assertIn('docker pull "${IMAGE_REF}"', block)
            self.assertIn('--platform "${PLATFORM}"', block)
            self.assertIn("docker info", block)
        text = self.workflow_text().lower()
        self.assertNotIn(":latest", text)
        self.assertNotIn("merge-multiple", text)
        self.assertNotIn("qemu", text)

    def test_macos_uses_persistent_verified_archive_and_loaded_runtime_checks(self):
        for job in ("producer_macos", "consumer_macos"):
            block = self.job_text(job)
            self.assertIn("verify-p2996-toolchain.sh", block)
            self.assertIn("--require-locked-sdk", block)
            self.assertIn("--extract-dir", block)
            self.assertIn("--metadata-output", block)
            self.assertIn("otool -L", block)
            self.assertIn("otool -l", block)
            self.assertIn("DYLD_PRINT_LIBRARIES=1", block)
            self.assertIn("libc++.1.dylib", block)
            self.assertIn("libc++abi.1.dylib", block)

    def test_consumers_are_fresh_and_every_intermediate_result_is_gated(self):
        for job in ("consumer_linux", "consumer_macos"):
            block = self.job_text(job)
            self.assertIn("relocatable_world_platform_probe", block)
            self.assertIn("--expect-source-sha", block)
            self.assertIn("--expect-workflow-run", block)
            self.assertIn("--toolchain-artifact-sha256", block)
            self.assertIn("producer_provenance_sha256", block)
            self.assertRegex(
                block, r'transfer\["status"\]\s*!=\s*[\'\"]PASS[\'\"]'
            )
        agreement = self.job_text("agreement")
        self.assertIn("c++ -std=c++20", agreement)
        self.assertRegex(
            agreement, r'decision\["status"\]\s*!=\s*[\'\"]PERMIT[\'\"]'
        )
        closure = self.job_text("closure")
        self.assertIn("c++ -std=c++20", closure)
        self.assertIn("build/relocatable-world/matrix-input/agreements.json", closure)
        self.assertNotIn('--agreements "${AUDIT}/agreements.json"', closure)
        self.assertIn(
            'shutil.copyfile(audit / "agreements.json", matrix_input)',
            closure,
        )
        self.assertIn("--expect-nodes 6", closure)
        self.assertIn("--expect-pairs 15", closure)
        self.assertIn("--expect-named-permits 60", closure)
        self.assertIn("--expect-transfers 30", closure)
        self.assertIn("EXPECTED_FLAT_FILE_COUNT=28", closure)

    def test_macos_builds_preserve_the_locked_compiler_target(self):
        for job in ("producer_macos", "consumer_macos"):
            block = self.job_text(job)
            self.assertIn('"COMPILER_TARGET": policy["target"]', block)
            self.assertIn(
                '-DCMAKE_CXX_COMPILER_TARGET="${COMPILER_TARGET}"', block
            )
            self.assertIn('-DCMAKE_OSX_ARCHITECTURES="${ARCHITECTURE}"', block)

    def test_every_authoritative_node_runs_the_complete_demo_contract(self):
        for job in ("producer_linux", "producer_macos"):
            with self.subTest(job=job):
                block = self.job_text(job)
                commands = re.sub(r"\\\n\s*", " ", block)
                self.assertRegex(
                    commands,
                    r"--target\s+relocatable_world_demo\s+"
                    r"relocatable_world_platform_probe\s+"
                    r"relocatable_world_producer",
                )
                self.assertIn(
                    'ctest --test-dir "build/relocatable-world/', block
                )
                self.assertIn(
                    '-R "^relocatable_world_demo$" --output-on-failure',
                    block,
                )
                self.assertIn("--no-tests=error", block)

    def test_exact_run_identity_and_forbidden_shortcuts(self):
        text = self.workflow_text()
        self.assertGreaterEqual(text.count("${{ github.sha }}"), 6)
        self.assertGreaterEqual(text.count("${{ github.run_id }}.${{ github.run_attempt }}"), 6)
        self.assertGreaterEqual(text.count("toolchain-sources.lock"), 6)
        self.assertGreaterEqual(text.count("toolchains.lock"), 6)
        lowered = text.lower()
        for forbidden in (
            "--fixture-context",
            "xoffset",
            "submodule",
            "runner_image ==",
            "needs.producer_linux.outputs",
            "needs.producer_macos.outputs",
        ):
            self.assertNotIn(forbidden, lowered)


if __name__ == "__main__":
    unittest.main()
