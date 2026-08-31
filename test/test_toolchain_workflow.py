import json
import os
import re
import shlex
import subprocess
import sys
import tempfile
import textwrap
import unittest
from pathlib import Path

import yaml


ROOT = Path(__file__).resolve().parents[1]
TOOLCHAIN_WORKFLOW = ROOT / ".github/workflows/toolchain-images.yml"
CI_WORKFLOW = ROOT / ".github/workflows/ci.yml"
COMPAT_WORKFLOW = ROOT / ".github/workflows/compat-pipeline.yml"
BUILDX_EXACT_MATCHER = (
    'versions = re.findall(r"(?<![0-9])v?([0-9]+\\.[0-9]+\\.[0-9]+)'
    '(?![0-9])", output)'
)


class ToolchainWorkflowTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.workflow = TOOLCHAIN_WORKFLOW.read_text(encoding="utf-8")
        cls.ci_workflow = CI_WORKFLOW.read_text(encoding="utf-8")
        cls.compat_workflow = COMPAT_WORKFLOW.read_text(encoding="utf-8")

    def workflow_job(self, workflow, name):
        match = re.search(
            rf"(?ms)^  {re.escape(name)}:\n(.*?)(?=^  [a-zA-Z0-9_]+:\n|\Z)",
            workflow,
        )
        self.assertIsNotNone(match, f"missing workflow job {name}")
        return match.group(1)

    def job(self, name):
        return self.workflow_job(self.workflow, name)

    def release_planner_script(self):
        match = re.search(
            r"(?ms)^          # BEGIN RELEASE STATE PLANNER\n"
            r"(.*?)"
            r"^          # END RELEASE STATE PLANNER$",
            self.workflow,
        )
        self.assertIsNotNone(match, "missing executable release state planner")
        return textwrap.dedent(match.group(1))

    def candidate_sealer_script(self):
        match = re.search(
            r"(?ms)^          # BEGIN CANDIDATE LOCK SEALER\n"
            r"(.*?)"
            r"^          # END CANDIDATE LOCK SEALER$",
            self.workflow,
        )
        self.assertIsNotNone(match, "missing executable candidate lock sealer")
        return textwrap.dedent(match.group(1))

    def candidate_batch_manifest_script(self):
        match = re.search(
            r"(?ms)^          # BEGIN CANDIDATE BATCH MANIFEST\n"
            r"(.*?)"
            r"^          # END CANDIDATE BATCH MANIFEST$",
            self.workflow,
        )
        self.assertIsNotNone(match, "missing executable candidate batch manifest logic")
        return textwrap.dedent(match.group(1))

    def oci_index_verifier_script(self):
        match = re.search(
            r"(?ms)^          # BEGIN OCI INDEX VERIFIER\n"
            r"(.*?)"
            r"^          # END OCI INDEX VERIFIER$",
            self.workflow,
        )
        self.assertIsNotNone(match, "missing executable OCI index verifier")
        return textwrap.dedent(match.group(1))

    def alias_promotion_transaction_script(self):
        match = re.search(
            r"(?ms)^          # BEGIN ALIAS PROMOTION TRANSACTION\n"
            r"(.*?)"
            r"^          # END ALIAS PROMOTION TRANSACTION$",
            self.workflow,
        )
        self.assertIsNotNone(match, "missing executable alias promotion transaction")
        return textwrap.dedent(match.group(1))

    def p2996_ldd_verifier_script(self):
        match = re.search(
            r"(?ms)^          # BEGIN P2996 LDD VERIFIER\n"
            r"(.*?)"
            r"^          # END P2996 LDD VERIFIER$",
            self.workflow,
        )
        self.assertIsNotNone(match, "missing executable P2996 ldd verifier")
        return textwrap.dedent(match.group(1))

    def buildx_inspect_verifier_scripts(self):
        matches = re.findall(
            r"(?ms)^          # BEGIN BUILDX INSPECT VERIFIER\n"
            r"(.*?)"
            r"^          # END BUILDX INSPECT VERIFIER$",
            self.workflow,
        )
        return [textwrap.dedent(match) for match in matches]

    def run_buildx_inspect_verifier(self, script, inspect_output):
        expected_image = (
            "docker.io/moby/buildkit@sha256:"
            + "a" * 64
        )
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "buildx-inspect.txt"
            path.write_text(inspect_output, encoding="utf-8")
            return subprocess.run(
                [sys.executable, "-", expected_image, str(path)],
                input=script,
                text=True,
                capture_output=True,
            )

    def run_p2996_ldd_verifier(self, text):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "ldd.txt"
            path.write_text(text, encoding="utf-8")
            return subprocess.run(
                [
                    sys.executable,
                    "-",
                    str(path),
                    "/opt/p2996-toolchain/lib/x86_64-unknown-linux-gnu",
                ],
                input=self.p2996_ldd_verifier_script(),
                text=True,
                capture_output=True,
            )

    def run_alias_promotion_transaction(self, fail_write="", fail_rollback=""):
        prelude = (
            f"FAIL_WRITE={shlex.quote(fail_write)}\n"
            f"FAIL_ROLLBACK={shlex.quote(fail_rollback)}\n"
            + r'''
set -u
GCC_REPOSITORY=gcc
P2996_REPOSITORY=p2996
gcc_image=gcc-new
p2996_image=p2996-new
gcc_old_digest=sha256:gcc-old
p2996_old_digest=sha256:p2996-old
write_alias() {
  printf 'write:%s\n' "$1"
  if [ "$1" = "${FAIL_WRITE}" ]; then return 23; fi
}
verify_new_aliases() { printf 'verify:new\n'; }
rollback_alias() {
  printf 'rollback:%s\n' "$1"
  if [ "$1" = "${FAIL_ROLLBACK}" ]; then return 24; fi
}
'''
        )
        environment = os.environ.copy()
        completed = subprocess.run(
            ["bash", "-s"],
            input=(prelude + self.alias_promotion_transaction_script()).encode(),
            capture_output=True,
            env=environment,
        )
        completed.stdout = completed.stdout.decode()
        completed.stderr = completed.stderr.decode()
        return completed

    def run_oci_index_verifier(self, raw_index, expected, reported_digest=None):
        import hashlib

        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            raw_path = root / "index.json"
            expected_path = root / "expected.json"
            raw_bytes = json.dumps(raw_index, separators=(",", ":")).encode()
            raw_path.write_bytes(raw_bytes)
            expected_path.write_text(json.dumps(expected), encoding="utf-8")
            if reported_digest is None:
                reported_digest = "sha256:" + hashlib.sha256(raw_bytes).hexdigest()
            return subprocess.run(
                [
                    sys.executable,
                    "-",
                    str(raw_path),
                    str(expected_path),
                    reported_digest,
                ],
                input=self.oci_index_verifier_script(),
                text=True,
                capture_output=True,
            )

    def run_candidate_sealer(self, receipts, current_attempt="2"):
        with tempfile.TemporaryDirectory() as directory:
            temporary = Path(directory)
            receipt_root = temporary / "receipts"
            for name, receipt in receipts.items():
                path = receipt_root / name / "receipt.json"
                path.parent.mkdir(parents=True)
                path.write_text(json.dumps(receipt), encoding="utf-8")
            output = temporary / "output" / "toolchains.lock"
            environment = os.environ.copy()
            environment.update(
                {
                    "GITHUB_RUN_ATTEMPT": current_attempt,
                    "GITHUB_RUN_ID": "42",
                    "GITHUB_SHA": "1" * 40,
                }
            )
            completed = subprocess.run(
                [sys.executable, "-", str(receipt_root), str(output)],
                input=self.candidate_sealer_script(),
                text=True,
                capture_output=True,
                cwd=ROOT,
                env=environment,
            )
            sealed = None
            if output.exists():
                sealed = json.loads(output.read_text(encoding="utf-8"))
            return completed, sealed

    def run_candidate_batch_manifest(
        self,
        current_attempt,
        existing_origin=None,
        noncanonical_existing=False,
    ):
        import hashlib

        with tempfile.TemporaryDirectory() as directory:
            temporary = Path(directory)
            manifest = temporary / "batch" / "manifest.json"
            output = temporary / "github-output.txt"
            source_sha = "1" * 40
            sources_sha256 = hashlib.sha256(
                (ROOT / ".github/docker/toolchain-sources.lock").read_bytes()
            ).hexdigest()
            candidate_batch_id = f"{source_sha}:{sources_sha256}:42"
            original_bytes = None
            if existing_origin is not None:
                record = {
                    "schema": 1,
                    "source_sha": source_sha,
                    "sources_sha256": sources_sha256,
                    "candidate_batch_id": candidate_batch_id,
                    "workflow_run": existing_origin,
                }
                manifest.parent.mkdir(parents=True)
                if noncanonical_existing:
                    original_bytes = json.dumps(record, sort_keys=True).encode()
                else:
                    original_bytes = (
                        json.dumps(record, indent=2, sort_keys=True) + "\n"
                    ).encode()
                manifest.write_bytes(original_bytes)
            environment = os.environ.copy()
            environment.update(
                {
                    "EXISTING_BATCH": str(existing_origin is not None).lower(),
                    "GITHUB_OUTPUT": str(output),
                    "GITHUB_RUN_ATTEMPT": str(current_attempt),
                    "GITHUB_RUN_ID": "42",
                    "GITHUB_SHA": source_sha,
                }
            )
            completed = subprocess.run(
                [sys.executable, "-", str(manifest), str(output)],
                input=self.candidate_batch_manifest_script(),
                text=True,
                capture_output=True,
                cwd=ROOT,
                env=environment,
            )
            record = None
            if manifest.exists():
                record = json.loads(manifest.read_text(encoding="utf-8"))
            output_values = {}
            if output.exists():
                output_values = dict(
                    line.split("=", 1)
                    for line in output.read_text(encoding="utf-8").splitlines()
                )
            return completed, record, output_values, original_bytes, manifest.read_bytes() if manifest.exists() else None

    def run_release_planner(
        self,
        expected,
        releases,
        tag=None,
        by_tag=None,
        batch_workflow_run="42.1",
    ):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            inputs = {
                "expected": expected,
                "releases": [releases],
            }
            paths = {}
            for name, value in inputs.items():
                path = root / f"{name}.json"
                path.write_text(json.dumps(value), encoding="utf-8")
                paths[name] = str(path)
            for name, value in (("tag", tag), ("by_tag", by_tag)):
                if value is None:
                    paths[name] = "-"
                else:
                    path = root / f"{name}.json"
                    path.write_text(json.dumps(value), encoding="utf-8")
                    paths[name] = str(path)
            output = root / "plan.json"
            environment = os.environ.copy()
            environment.update(
                {
                    "BATCH_WORKFLOW_RUN": batch_workflow_run,
                    "CANDIDATE_BATCH_ID": f"{'1' * 40}:{'e' * 64}:42",
                    "GITHUB_SHA": "1" * 40,
                    "GITHUB_RUN_ATTEMPT": "2",
                    "GITHUB_RUN_ID": "42",
                    "RELEASE_TAG": "typelayout-toolchains-test",
                }
            )
            completed = subprocess.run(
                [
                    sys.executable,
                    "-",
                    paths["expected"],
                    paths["releases"],
                    paths["tag"],
                    paths["by_tag"],
                    str(output),
                ],
                input=self.release_planner_script(),
                text=True,
                capture_output=True,
                env=environment,
            )
            plan = None
            if output.exists():
                plan = json.loads(output.read_text(encoding="utf-8"))
            return completed, plan

    def test_publication_events_are_serial_and_do_not_cancel_queued_runs(self):
        parsed = yaml.safe_load(self.workflow)
        self.assertEqual(
            parsed["concurrency"],
            {
                "group": "typelayout-toolchain-publication",
                "queue": "max",
                "cancel-in-progress": False,
            },
        )
        self.assertIn("  workflow_dispatch:", self.workflow)
        self.assertRegex(self.workflow, r"(?m)^      - main$")
        self.assertIn("group: typelayout-toolchain-publication", self.workflow)
        self.assertIn("queue: max", self.workflow)
        self.assertIn("cancel-in-progress: false", self.workflow)

        required_paths = (
            ".gitattributes",
            ".github/docker/Dockerfile.gcc16",
            ".github/docker/Dockerfile.p2996",
            ".github/docker/docker-bake.hcl",
            ".github/docker/toolchain-sources.lock",
            ".github/docker/toolchains.lock",
            ".github/scripts/bootstrap-toolchain-sources.py",
            ".github/scripts/build-p2996-macos.sh",
            ".github/scripts/validate-toolchain-locks.py",
            ".github/scripts/verify-p2996-toolchain.sh",
            ".github/workflows/toolchain-images.yml",
        )
        for path in required_paths:
            with self.subTest(path=path):
                self.assertIn(f"- {path}", self.workflow)

    def test_dispatch_scope_can_isolate_the_two_native_macos_nodes(self):
        self.assertRegex(
            self.workflow,
            r"(?ms)^  workflow_dispatch:\n"
            r"    inputs:\n"
            r"      scope:\n"
            r"        description: Candidate build scope\n"
            r"        required: true\n"
            r"        default: all\n"
            r"        type: choice\n"
            r"        options:\n"
            r"          - all\n"
            r"          - macos-diagnostic$",
        )
        all_only = (
            "build_gcc_amd64",
            "build_gcc_arm64",
            "build_p2996_amd64",
            "build_p2996_arm64",
            "index_gcc",
            "index_p2996",
            "publish_macos_release",
            "seal_candidate_lock",
        )
        for name in all_only:
            with self.subTest(name=name):
                self.assertIn(
                    "if: github.event_name == 'workflow_dispatch' && "
                    "inputs.scope == 'all'",
                    self.job(name),
                )
        for name in (
            "build_p2996_macos_arm64",
            "build_p2996_macos_x86_64",
        ):
            with self.subTest(name=name):
                self.assertIn(
                    "if: github.event_name == 'workflow_dispatch'",
                    self.job(name),
                )
                self.assertNotIn("inputs.scope == 'all'", self.job(name))

    def test_permissions_are_least_privilege_per_mutation_boundary(self):
        top_permissions = re.search(
            r"(?ms)^permissions:\n(.*?)(?=^[a-zA-Z_][a-zA-Z0-9_-]*:)",
            self.workflow,
        )
        self.assertIsNotNone(top_permissions)
        self.assertEqual(top_permissions.group(1).strip(), "contents: read")

        package_writers = (
            "build_gcc_amd64",
            "build_gcc_arm64",
            "build_p2996_amd64",
            "build_p2996_arm64",
            "index_gcc",
            "index_p2996",
            "promote_legacy_aliases",
        )
        for job in package_writers:
            with self.subTest(job=job):
                block = self.job(job)
                self.assertRegex(
                    block,
                    r"(?ms)^    permissions:\n      contents: read\n      packages: write$",
                )

        release = self.job("publish_macos_release")
        self.assertRegex(
            release, r"(?ms)^    permissions:\n      contents: read$"
        )
        initializer = self.job("initialize_candidate_batch")
        self.assertRegex(
            initializer,
            r"(?ms)^    permissions:\n      actions: read\n      contents: read$",
        )
        for job in (
            "build_p2996_macos_arm64",
            "build_p2996_macos_x86_64",
            "seal_candidate_lock",
        ):
            with self.subTest(job=job):
                self.assertNotIn("permissions:", self.job(job))

        self.assertNotIn("id-token: write", self.workflow)
        self.assertNotRegex(self.workflow, r"(?i)visibility\s*[:=]")

    def test_release_mutations_require_the_protected_environment_token(self):
        release = self.job("publish_macos_release")
        self.assertIn("environment: toolchain-release", release)
        self.assertNotIn("contents: write", release)
        self.assertIn(
            "token: ${{ secrets.TOOLCHAIN_RELEASE_TOKEN }}", release
        )
        self.assertIn(
            "if: steps.release_plan.outputs.action == 'create'", release
        )
        self.assertRegex(
            release,
            r"(?ms)- name: Create the missing lightweight tag.*?"
            r"GH_TOKEN: \$\{\{ secrets\.TOOLCHAIN_RELEASE_TOKEN \}\}.*?"
            r"gh api --method POST",
        )
        self.assertRegex(
            release,
            r"(?ms)- name: Upload only missing assets.*?"
            r"GH_TOKEN: \$\{\{ secrets\.TOOLCHAIN_RELEASE_TOKEN \}\}.*?"
            r"uploads\.github\.com",
        )
        self.assertRegex(
            release,
            r"(?ms)- name: Finalize the verified draft.*?"
            r"GH_TOKEN: \$\{\{ secrets\.TOOLCHAIN_RELEASE_TOKEN \}\}.*?"
            r"gh api --method PATCH",
        )
        self.assertEqual(
            release.count("GH_TOKEN: ${{ secrets.GITHUB_TOKEN }}"), 1
        )
        self.assertEqual(
            self.workflow.count("secrets.TOOLCHAIN_RELEASE_TOKEN"), 6
        )

    def test_release_planner_reuses_exact_bytes_and_uploads_only_missing(self):
        tag_name = "typelayout-toolchains-test"
        source_sha = "1" * 40
        expected = {
            "p2996-macos-arm64-test.tar.zst": {
                "path": "build/arm64.tar.zst",
                "size": 101,
                "sha256": "a" * 64,
            },
            "p2996-macos-x86_64-test.tar.zst": {
                "path": "build/x86_64.tar.zst",
                "size": 202,
                "sha256": "b" * 64,
            },
        }
        assets = [
            {
                "id": 11,
                "name": name,
                "state": "uploaded",
                "size": record["size"],
                "digest": f"sha256:{record['sha256']}",
            }
            for name, record in expected.items()
        ]
        tag = {"object": {"type": "commit", "sha": source_sha}}
        attempt_one_body = json.dumps(
            {
                "batch_workflow_run": "42.1",
                "candidate_batch_id": f"{source_sha}:{'e' * 64}:42",
                "producer_workflow_run": "42.1",
            },
            separators=(",", ":"),
            sort_keys=True,
        )

        def release(*, draft=False, immutable=False, selected_assets=None):
            return {
                "id": 77,
                "tag_name": tag_name,
                "target_commitish": source_sha,
                "name": tag_name,
                "body": attempt_one_body,
                "draft": draft,
                "prerelease": False,
                "immutable": immutable,
                "assets": assets if selected_assets is None else selected_assets,
            }

        completed, plan = self.run_release_planner(expected, [])
        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertEqual(plan["action"], "create")
        self.assertIs(plan["create_tag"], True)
        self.assertEqual(set(plan["missing_assets"]), set(expected))
        self.assertEqual(plan["release_producer_workflow_run"], "42.2")

        published = release()
        completed, plan = self.run_release_planner(
            expected, [published], tag=tag, by_tag=published
        )
        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertEqual(plan["action"], "reuse")
        self.assertIs(plan["finalize"], False)
        self.assertEqual(plan["missing_assets"], [])
        self.assertEqual(plan["release_producer_workflow_run"], "42.1")

        immutable_published = release(immutable=True)
        completed, plan = self.run_release_planner(
            expected,
            [immutable_published],
            tag=tag,
            by_tag=immutable_published,
        )
        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertEqual(plan["action"], "reuse")
        self.assertIs(plan["finalize"], False)
        self.assertEqual(plan["missing_assets"], [])

        partial = release(selected_assets=assets[:1])
        completed, plan = self.run_release_planner(
            expected, [partial], tag=tag, by_tag=partial
        )
        self.assertNotEqual(completed.returncode, 0)
        self.assertIsNone(plan)

        draft = release(draft=True, selected_assets=assets[:1])
        completed, plan = self.run_release_planner(
            expected, [draft], tag=tag
        )
        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertEqual(plan["action"], "upload_missing")
        self.assertEqual(
            plan["missing_assets"], ["p2996-macos-x86_64-test.tar.zst"]
        )
        self.assertIs(plan["finalize"], True)
        self.assertEqual(plan["release_producer_workflow_run"], "42.1")

    def test_release_planner_rejects_wrong_or_immutable_remote_state(self):
        tag_name = "typelayout-toolchains-test"
        source_sha = "1" * 40
        expected = {
            "p2996-macos-arm64-test.tar.zst": {
                "path": "build/arm64.tar.zst",
                "size": 101,
                "sha256": "a" * 64,
            },
            "p2996-macos-x86_64-test.tar.zst": {
                "path": "build/x86_64.tar.zst",
                "size": 202,
                "sha256": "b" * 64,
            },
        }
        exact_asset = {
            "id": 11,
            "name": "p2996-macos-arm64-test.tar.zst",
            "state": "uploaded",
            "size": 101,
            "digest": f"sha256:{'a' * 64}",
        }
        exact_x86_asset = {
            "id": 12,
            "name": "p2996-macos-x86_64-test.tar.zst",
            "state": "uploaded",
            "size": 202,
            "digest": f"sha256:{'b' * 64}",
        }
        tag = {"object": {"type": "commit", "sha": source_sha}}
        attempt_one_body = json.dumps(
            {
                "batch_workflow_run": "42.1",
                "candidate_batch_id": f"{source_sha}:{'e' * 64}:42",
                "producer_workflow_run": "42.1",
            },
            separators=(",", ":"),
            sort_keys=True,
        )
        base = {
            "id": 77,
            "tag_name": tag_name,
            "target_commitish": source_sha,
            "name": tag_name,
            "body": attempt_one_body,
            "draft": False,
            "prerelease": False,
            "immutable": False,
            "assets": [exact_asset],
        }
        cases = {
            "wrong target": {**base, "target_commitish": "2" * 40},
            "wrong name": {**base, "name": "unexpected"},
            "wrong candidate batch": {
                **base,
                "assets": [exact_asset, exact_x86_asset],
                "body": json.dumps(
                    {
                        "batch_workflow_run": "42.1",
                        "candidate_batch_id": "wrong",
                        "producer_workflow_run": "42.1",
                    },
                    separators=(",", ":"),
                    sort_keys=True,
                ),
            },
            "future producer attempt": {
                **base,
                "assets": [exact_asset, exact_x86_asset],
                "body": json.dumps(
                    {
                        "batch_workflow_run": "42.1",
                        "candidate_batch_id": f"{source_sha}:{'e' * 64}:42",
                        "producer_workflow_run": "42.3",
                    },
                    separators=(",", ":"),
                    sort_keys=True,
                ),
            },
            "wrong batch workflow origin": {
                **base,
                "assets": [exact_asset, exact_x86_asset],
                "body": json.dumps(
                    {
                        "batch_workflow_run": "42.2",
                        "candidate_batch_id": f"{source_sha}:{'e' * 64}:42",
                        "producer_workflow_run": "42.1",
                    },
                    separators=(",", ":"),
                    sort_keys=True,
                ),
            },
            "noncanonical body bytes": {
                **base,
                "assets": [exact_asset, exact_x86_asset],
                "body": json.dumps(
                    {
                        "batch_workflow_run": "42.1",
                        "candidate_batch_id": f"{source_sha}:{'e' * 64}:42",
                        "producer_workflow_run": "42.1",
                    }
                ),
            },
            "wrong bytes": {
                **base,
                "assets": [{**exact_asset, "digest": f"sha256:{'c' * 64}"}],
            },
            "wrong size": {
                **base,
                "assets": [{**exact_asset, "size": 999}],
            },
            "unexpected asset": {
                **base,
                "assets": [
                    exact_asset,
                    {
                        "id": 12,
                        "name": "unexpected.tar.zst",
                        "state": "uploaded",
                        "size": 1,
                        "digest": f"sha256:{'d' * 64}",
                    },
                ],
            },
            "immutable partial": {**base, "immutable": True},
        }
        for label, release in cases.items():
            with self.subTest(label=label):
                completed, plan = self.run_release_planner(
                    expected, [release], tag=tag, by_tag=release
                )
                self.assertNotEqual(completed.returncode, 0)
                self.assertIsNone(plan)

        annotated_tag = {"object": {"type": "tag", "sha": source_sha}}
        completed, plan = self.run_release_planner(expected, [], tag=annotated_tag)
        self.assertNotEqual(completed.returncode, 0)
        self.assertIsNone(plan)

        exact_published = {
            **base,
            "assets": [exact_asset, exact_x86_asset],
        }
        completed, plan = self.run_release_planner(
            expected,
            [exact_published, {**exact_published, "id": 78}],
            tag=tag,
            by_tag=exact_published,
        )
        self.assertNotEqual(completed.returncode, 0)
        self.assertIsNone(plan)

        stale_by_tag = {**exact_published, "id": 78}
        completed, plan = self.run_release_planner(
            expected,
            [exact_published],
            tag=tag,
            by_tag=stale_by_tag,
        )
        self.assertNotEqual(completed.returncode, 0)
        self.assertIsNone(plan)

        predating_release = {
            **exact_published,
            "body": json.dumps(
                {
                    "batch_workflow_run": "42.2",
                    "candidate_batch_id": f"{source_sha}:{'e' * 64}:42",
                    "producer_workflow_run": "42.1",
                },
                separators=(",", ":"),
                sort_keys=True,
            ),
        }
        completed, plan = self.run_release_planner(
            expected,
            [predating_release],
            tag=tag,
            by_tag=predating_release,
            batch_workflow_run="42.2",
        )
        self.assertNotEqual(completed.returncode, 0)
        self.assertIsNone(plan)
        self.assertIn("producer workflow mismatch", completed.stderr)

    def test_release_is_verified_as_a_draft_before_finalization(self):
        release = self.job("publish_macos_release")
        self.assertIn("--paginate --slurp", release)
        self.assertIn("releases/tags/${RELEASE_TAG}", release)
        self.assertIn("git/ref/tags/${RELEASE_TAG}", release)
        self.assertIn("id: release_plan", release)
        self.assertIn("id: candidate_identity", release)
        self.assertIn(
            "BATCH_WORKFLOW_RUN: ${{ steps.candidate_identity.outputs.batch_workflow_run }}",
            release,
        )
        self.assertIn('"batch_workflow_run": batch_workflow_run', release)
        self.assertIn(
            "body: ${{ steps.candidate_identity.outputs.release_body }}", release
        )
        self.assertNotIn("steps.source.outputs.release_body", release)
        self.assertIn(
            "if: steps.release_plan.outputs.action == 'create'", release
        )
        self.assertIn("draft: true", release)
        self.assertIn("overwrite_files: false", release)
        self.assertIn(
            "if: steps.release_plan.outputs.action == 'upload_missing'",
            release,
        )
        self.assertIn("id: verified_release", release)
        release_plan = re.search(
            r"(?ms)^      - name: Read and plan the exact release state\n"
            r"(.*?)(?=^      - name:)",
            release,
        )
        self.assertIsNotNone(release_plan)
        self.assertIn(
            "GH_TOKEN: ${{ secrets.TOOLCHAIN_RELEASE_TOKEN }}",
            release_plan.group(1),
        )
        verified_release = re.search(
            r"(?ms)^      - name: Verify complete release bytes before finalization\n"
            r"(.*?)(?=^      - name:)",
            release,
        )
        self.assertIsNotNone(verified_release)
        self.assertIn(
            "GH_TOKEN: ${{ secrets.TOOLCHAIN_RELEASE_TOKEN }}",
            verified_release.group(1),
        )
        self.assertIn('asset.get("url")', verified_release.group(1))
        self.assertIn("/releases/assets/{asset['id']}", verified_release.group(1))
        self.assertNotIn("browser_download_url", verified_release.group(1))
        self.assertIn(
            "if: steps.verified_release.outputs.finalize == 'true'", release
        )
        self.assertIn("-F draft=false", release)
        self.assertNotIn("--clobber", release)
        self.assertNotIn("delete-asset", release)
        self.assertNotIn("overwrite_files: true", release)
        self.assertLess(
            release.index("id: verified_release"),
            release.index("-F draft=false"),
        )

    def test_candidate_builds_are_exactly_six_dispatch_only_native_jobs(self):
        jobs = {
            "build_gcc_amd64": ("ubuntu-24.04", "gcc16-amd64"),
            "build_gcc_arm64": ("ubuntu-24.04-arm", "gcc16-arm64"),
            "build_p2996_amd64": ("ubuntu-24.04", "p2996-amd64"),
            "build_p2996_arm64": ("ubuntu-24.04-arm", "p2996-arm64"),
            "build_p2996_macos_arm64": ("macos-15", "arm64_macos_clang"),
            "build_p2996_macos_x86_64": (
                "macos-15-intel",
                "x86_64_macos_clang",
            ),
        }
        for job, (runner, identity) in jobs.items():
            with self.subTest(job=job):
                block = self.job(job)
                self.assertIn("if: github.event_name == 'workflow_dispatch'", block)
                self.assertIn(f"runs-on: {runner}", block)
                self.assertIn(identity, block)
                self.assertIn("needs: initialize_candidate_batch", block)
                self.assertIn(
                    "BATCH_WORKFLOW_RUN: ${{ needs.initialize_candidate_batch.outputs.workflow_run }}",
                    block,
                )
                self.assertIn(
                    "CANDIDATE_BATCH_ID: ${{ needs.initialize_candidate_batch.outputs.candidate_batch_id }}",
                    block,
                )
        self.assertNotIn("ubuntu-latest", self.workflow)
        self.assertNotRegex(self.workflow, r"(?i)setup-qemu|--platform\s+linux/.*,")

    def test_linux_builds_use_locked_buildx_and_digest_only_bake_targets(self):
        jobs = {
            "build_gcc_amd64": (
                "gcc16-amd64",
                "x86_64_linux_gcc",
                "ubuntu-24.04",
            ),
            "build_gcc_arm64": (
                "gcc16-arm64",
                "arm64_linux_gcc",
                "ubuntu-24.04-arm",
            ),
            "build_p2996_amd64": (
                "p2996-amd64",
                "x86_64_linux_clang",
                "ubuntu-24.04",
            ),
            "build_p2996_arm64": (
                "p2996-arm64",
                "arm64_linux_clang",
                "ubuntu-24.04-arm",
            ),
        }
        for job, (target, node, runner) in jobs.items():
            with self.subTest(job=job):
                block = self.job(job)
                self.assertIn("validate-toolchain-locks.py", block)
                self.assertIn("docker version", block)
                self.assertIn("docker buildx version", block)
                self.assertIn("docker/setup-buildx-action@8d2750c68a42422c14e847fe6c8ac0403b4cbd6f", block)
                self.assertIn("cache-binary: false", block)
                self.assertIn("id: lock", block)
                self.assertIn(
                    "image=${{ steps.lock.outputs.buildkit_image }}", block
                )
                self.assertIn(
                    "version: v${{ steps.lock.outputs.buildx_version }}", block
                )
                self.assertIn("docker buildx bake", block)
                self.assertIn(f'"{target}"', block)
                self.assertIn("--metadata-file", block)
                self.assertIn(
                    "push-by-digest=true,name-canonical=true,oci-mediatypes=true,push=true",
                    block,
                )
                self.assertNotIn("--provenance", block)
                self.assertNotIn("--sbom", block)
                self.assertNotRegex(block, r"(?i)\.(?:attest|provenance|sbom)=")
                self.assertIn("manifest_digest", block)
                self.assertIn("/tmp/toolchain-probe", block)
                self.assertIn("relocatable_world_platform_probe", block)
                self.assertIn("-DTYPELAYOUT_TOOLCHAIN_REVISION", block)
                self.assertIn(node, block)
                self.assertIn(f"--runner {runner}", block)
                self.assertIn("--runner-image", block)
                self.assertIn("platform-probe.json", block)
                self.assertIn("evidence.validate_probe", block)
                self.assertIn('compiler["version"]', block)
                self.assertIn('compiler["target"]', block)
                self.assertIn('compiler["stdlib"]', block)
                self.assertIn('"candidate_batch_id": candidate_batch_id', block)
                self.assertIn('"producer_workflow_run": producer_workflow_run', block)
                self.assertIn('batch_workflow_run = os.environ["BATCH_WORKFLOW_RUN"]', block)
                self.assertIn('"workflow_run": batch_workflow_run', block)
                self.assertEqual(block.count(BUILDX_EXACT_MATCHER), 2)
                self.assertNotIn(
                    'docker buildx version | grep -F "${LOCKED_BUILDX_VERSION}"',
                    block,
                )
                self.assertNotIn("__GLIBCXX__", block)
                self.assertNotIn("_LIBCPP_VERSION", block)

    def test_buildx_inspect_verifier_accepts_aligned_fields_and_fails_closed(self):
        scripts = self.buildx_inspect_verifier_scripts()
        self.assertEqual(len(scripts), 4)
        expected_image = "docker.io/moby/buildkit@sha256:" + "a" * 64
        valid = (
            "Name:          builder\n"
            "Driver:        docker-container\n"
            "Nodes:\n"
            f'Driver Options:        image="{expected_image}"\n'
        )
        invalid_outputs = (
            valid.replace("docker-container", "docker"),
            valid + "Driver:        docker-container\n",
            valid.replace("a" * 64, "b" * 64),
            valid.replace(
                f'image="{expected_image}"',
                f'image="{expected_image}" image="{expected_image}"',
            ),
            valid.replace(
                f'image="{expected_image}"',
                f'image="{expected_image}" network=host',
            ),
        )
        for script in scripts:
            completed = self.run_buildx_inspect_verifier(script, valid)
            self.assertEqual(completed.returncode, 0, completed.stderr)
            for inspect_output in invalid_outputs:
                completed = self.run_buildx_inspect_verifier(script, inspect_output)
                self.assertNotEqual(completed.returncode, 0, inspect_output)

        self.assertNotIn("grep -F 'Driver: docker-container'", self.workflow)

    def test_p2996_runtime_linkage_rejects_mixed_bundled_and_host_libraries(self):
        prefix = "/opt/p2996-toolchain/lib/x86_64-unknown-linux-gnu"
        valid = "\n".join(
            (
                f"libc++.so.1 => {prefix}/libc++.so.1 (0x1)",
                f"libc++abi.so.1 => {prefix}/libc++abi.so.1 (0x2)",
                f"libunwind.so.1 => {prefix}/libunwind.so.1 (0x3)",
                "libc.so.6 => /usr/lib/x86_64-linux-gnu/libc.so.6 (0x4)",
            )
        )
        completed = self.run_p2996_ldd_verifier(valid)
        self.assertEqual(completed.returncode, 0, completed.stderr)

        mixed = valid + "\nlibc++.so.1 => /usr/lib/x86_64-linux-gnu/libc++.so.1 (0x5)\n"
        completed = self.run_p2996_ldd_verifier(mixed)
        self.assertNotEqual(completed.returncode, 0)

        missing = "\n".join(valid.splitlines()[:2])
        completed = self.run_p2996_ldd_verifier(missing)
        self.assertNotEqual(completed.returncode, 0)

        for job in ("build_p2996_amd64", "build_p2996_arm64", "promote_legacy_aliases"):
            with self.subTest(job=job):
                block = self.job(job)
                self.assertIn("P2996 runtime linkage", block)
                self.assertNotRegex(block, r"ldd .*\| grep -F")

    def test_index_jobs_are_the_only_canonical_candidate_tag_writers(self):
        all_candidates = (
            "build_gcc_amd64",
            "build_gcc_arm64",
            "build_p2996_amd64",
            "build_p2996_arm64",
            "build_p2996_macos_arm64",
            "build_p2996_macos_x86_64",
        )
        for job in ("index_gcc", "index_p2996"):
            with self.subTest(job=job):
                block = self.job(job)
                for dependency in all_candidates:
                    self.assertIn(f"- {dependency}", block)
                self.assertIn("source-${SOURCES_SHA256}", block)
                self.assertIn("docker buildx imagetools inspect --raw", block)
                self.assertIn("docker buildx imagetools create", block)
                self.assertIn("exact_index_map", block)
                self.assertIn("index_digest", block)
                self.assertNotIn("--append", block)
                self.assertIn(
                    "- name: Verify the locked Docker and Buildx identities",
                    block,
                )

        index_offset = self.workflow.find("  index_gcc:")
        self.assertGreaterEqual(index_offset, 0, "missing index_gcc job")
        if index_offset >= 0:
            build_prefix = self.workflow[:index_offset]
            self.assertNotIn("source-${SOURCES_SHA256}", build_prefix)

        release = self.job("publish_macos_release")
        for dependency in (
            "index_gcc",
            "index_p2996",
            "build_p2996_macos_arm64",
            "build_p2996_macos_x86_64",
        ):
            self.assertIn(f"- {dependency}", release)

    def test_oci_index_reuse_requires_exact_canonical_raw_bytes(self):
        descriptor_media = "application/vnd.oci.image.manifest.v1+json"
        expected = [
            {
                "mediaType": descriptor_media,
                "digest": "sha256:" + "a" * 64,
                "size": 101,
                "platform": {"os": "linux", "architecture": "amd64"},
            },
            {
                "mediaType": descriptor_media,
                "digest": "sha256:" + "b" * 64,
                "size": 202,
                "platform": {"os": "linux", "architecture": "arm64"},
            },
        ]
        valid = {
            "schemaVersion": 2,
            "mediaType": "application/vnd.oci.image.index.v1+json",
            "manifests": expected,
        }
        completed = self.run_oci_index_verifier(valid, expected)
        self.assertEqual(completed.returncode, 0, completed.stderr)

        mutations = []
        reordered = json.loads(json.dumps(valid))
        reordered["manifests"].reverse()
        mutations.append(reordered)
        wrong_size = json.loads(json.dumps(valid))
        wrong_size["manifests"][0]["size"] += 1
        mutations.append(wrong_size)
        extra_root = json.loads(json.dumps(valid))
        extra_root["annotations"] = {"test": "forbidden"}
        mutations.append(extra_root)
        extra_descriptor = json.loads(json.dumps(valid))
        extra_descriptor["manifests"][0]["annotations"] = {"test": "forbidden"}
        mutations.append(extra_descriptor)
        wrong_media = json.loads(json.dumps(valid))
        wrong_media["mediaType"] = "application/vnd.docker.distribution.manifest.list.v2+json"
        mutations.append(wrong_media)
        for mutation in mutations:
            with self.subTest(mutation=mutation):
                completed = self.run_oci_index_verifier(mutation, expected)
                self.assertNotEqual(completed.returncode, 0)

        completed = self.run_oci_index_verifier(
            valid, expected, "sha256:" + "f" * 64
        )
        self.assertNotEqual(completed.returncode, 0)

    def test_candidate_artifacts_are_bound_by_exact_artifact_id_across_reruns(self):
        candidate_jobs = (
            "build_gcc_amd64",
            "build_gcc_arm64",
            "build_p2996_amd64",
            "build_p2996_arm64",
            "build_p2996_macos_arm64",
            "build_p2996_macos_x86_64",
            "index_gcc",
            "index_p2996",
            "publish_macos_release",
        )
        for name in candidate_jobs:
            with self.subTest(job=name):
                block = self.job(name)
                self.assertIn(
                    "candidate_artifact_id: ${{ steps.upload_candidate.outputs.artifact-id }}",
                    block,
                )
                self.assertIn("id: upload_candidate", block)
                self.assertRegex(
                    block,
                    r"name: candidate-[a-zA-Z0-9_-]+-\$\{\{ github\.run_attempt \}\}",
                )

        for index, dependencies in (
            ("index_gcc", ("build_gcc_amd64", "build_gcc_arm64")),
            ("index_p2996", ("build_p2996_amd64", "build_p2996_arm64")),
        ):
            block = self.job(index)
            for dependency in dependencies:
                self.assertIn(
                    "artifact-ids: "
                    f"${{{{ needs.{dependency}.outputs.candidate_artifact_id }}}}",
                    block,
                )

        release = self.job("publish_macos_release")
        for dependency in (
            "build_p2996_macos_arm64",
            "build_p2996_macos_x86_64",
        ):
            self.assertIn(
                "artifact-ids: "
                f"${{{{ needs.{dependency}.outputs.candidate_artifact_id }}}}",
                release,
            )

        sealer = self.job("seal_candidate_lock")
        for dependency in ("index_gcc", "index_p2996", "publish_macos_release"):
            self.assertIn(
                "artifact-ids: "
                f"${{{{ needs.{dependency}.outputs.candidate_artifact_id }}}}",
                sealer,
            )
        self.assertEqual(
            self.workflow.count("artifact-ids:"),
            self.workflow.count("merge-multiple: true"),
        )

    def test_index_and_promotion_buildx_identity_is_token_exact(self):
        for job in ("index_gcc", "index_p2996", "promote_legacy_aliases"):
            with self.subTest(job=job):
                block = self.job(job)
                self.assertIn('buildx_output="$(docker buildx version)"', block)
                self.assertIn(BUILDX_EXACT_MATCHER, block)
                self.assertIn("if expected not in versions:", block)
                self.assertNotIn(
                    'docker buildx version | grep -F "${LOCKED_BUILDX_VERSION}"',
                    block,
                )

        pattern = re.compile(
            r"(?<![0-9])v?([0-9]+\.[0-9]+\.[0-9]+)(?![0-9])"
        )
        expected = "0.30.0"
        self.assertIn(expected, pattern.findall("github.com/docker/buildx v0.30.0 abc"))
        for impostor in ("v0.30.01", "v10.30.0", "v0.30.00"):
            with self.subTest(impostor=impostor):
                self.assertNotIn(expected, pattern.findall(impostor))

    def test_macos_candidates_are_verified_before_immutable_release(self):
        for job, node, archive in (
            (
                "build_p2996_macos_arm64",
                "arm64_macos_clang",
                "p2996-macos-arm64-",
            ),
            (
                "build_p2996_macos_x86_64",
                "x86_64_macos_clang",
                "p2996-macos-x86_64-",
            ),
        ):
            with self.subTest(job=job):
                block = self.job(job)
                self.assertIn("build-p2996-macos.sh", block)
                self.assertIn(f"--node {node}", block)
                self.assertIn("--require-locked-sdk", block)
                self.assertIn('"sdk_locked"', block)
                self.assertIn("2147483648", block)
                self.assertIn(archive, block)
                self.assertIn("candidate-receipt.json", block)
                self.assertIn('"candidate_batch_id": candidate_batch_id', block)
                self.assertIn('"producer_workflow_run": producer_workflow_run', block)
                self.assertIn('batch_workflow_run = os.environ["BATCH_WORKFLOW_RUN"]', block)
                self.assertIn('"workflow_run": batch_workflow_run', block)

        release = self.job("publish_macos_release")
        self.assertIn("- build_p2996_macos_arm64", release)
        self.assertIn("- build_p2996_macos_x86_64", release)
        self.assertIn("target_commitish: ${{ github.sha }}", release)
        self.assertIn("overwrite_files: false", release)
        self.assertIn("fail_on_unmatched_files: true", release)
        self.assertIn("make_latest: false", release)
        self.assertIn("verify_release", release)

    def test_large_macos_archives_are_hashed_with_bounded_memory(self):
        self.assertNotRegex(
            self.workflow,
            r"(?:archive|local|downloaded)\.read_bytes\(\)",
        )
        for job in (
            "build_p2996_macos_arm64",
            "build_p2996_macos_x86_64",
            "publish_macos_release",
        ):
            with self.subTest(job=job):
                block = self.job(job)
                self.assertIn("def sha256_file", block)
                self.assertIn(
                    'iter(lambda: stream.read(1024 * 1024), b"")',
                    block,
                )

    def test_candidate_lock_is_sealed_from_three_publication_receipts(self):
        block = self.job("seal_candidate_lock")
        for dependency in ("index_gcc", "index_p2996", "publish_macos_release"):
            self.assertIn(f"- {dependency}", block)
        for dependency in (
            "index_gcc",
            "index_p2996",
            "publish_macos_release",
        ):
            self.assertIn(
                "artifact-ids: "
                f"${{{{ needs.{dependency}.outputs.candidate_artifact_id }}}}",
                block,
            )
        self.assertIn('"workflow_run": workflow_run', block)
        self.assertIn('"source_sha": source_sha', block)
        self.assertIn('"sources_sha256": sources_sha256', block)
        self.assertIn('"candidate_batch_id"', block)
        self.assertIn('workflow_runs = {receipt["workflow_run"] for receipt in receipts.values()}', block)
        self.assertIn('workflow_run = workflow_runs.pop()', block)
        self.assertIn('publication receipts disagree on candidate batch origin', block)
        self.assertNotIn(
            '''"workflow_run": f"{os.environ['GITHUB_RUN_ID']}.{os.environ['GITHUB_RUN_ATTEMPT']}"''',
            block,
        )
        self.assertIn("validate-toolchain-locks.py", block)
        self.assertIn("name: candidate-toolchains-lock", block)
        self.assertIn("path: build/candidate-toolchains-lock/toolchains.lock", block)
        self.assertIn("if-no-files-found: error", block)

    def test_attempt_two_seals_attempt_one_bytes_without_restamping_them(self):
        sources_sha256 = __import__("hashlib").sha256(
            (ROOT / ".github/docker/toolchain-sources.lock").read_bytes()
        ).hexdigest()
        source_sha = "1" * 40
        candidate_batch_id = f"{source_sha}:{sources_sha256}:42"

        def index_receipt(name, repository, revision, digest_prefix):
            return {
                "schema": 1,
                "source_sha": source_sha,
                "sources_sha256": sources_sha256,
                "candidate_batch_id": candidate_batch_id,
                "workflow_run": "42.1",
                "producer_workflow_run": "42.2",
                "platform_producer_workflow_runs": {
                    "linux/amd64": "42.1",
                    "linux/arm64": "42.1",
                },
                "toolchain": name,
                "repository": repository,
                "index_digest": f"sha256:{digest_prefix * 64}",
                "compiler_revision": revision,
                "compiler_version": "test compiler",
                "stdlib": "test stdlib",
                "platforms": {
                    "linux/amd64": {
                        "manifest_digest": "sha256:" + ("a" if name == "gcc" else "c") * 64,
                        "target": "x86_64-unknown-linux-gnu",
                    },
                    "linux/arm64": {
                        "manifest_digest": "sha256:" + ("b" if name == "gcc" else "d") * 64,
                        "target": "aarch64-unknown-linux-gnu",
                    },
                },
            }

        revision = "060be17654102019e14810c3f948ef85a490755f"

        def mac_node(architecture, digest):
            return {
                "url": (
                    "https://github.com/ximicpp/TypeLayout/releases/download/"
                    f"typelayout-toolchains-{sources_sha256}/"
                    f"p2996-macos-{architecture}-{revision}.tar.zst"
                ),
                "archive_sha256": digest * 64,
                "compiler_revision": revision,
                "compiler_version": "test clang",
                "target": f"{architecture}-apple-macosx15.0.0",
                "stdlib": "libc++ test",
                "xcode_version": "16.4",
                "xcode_build": "16F6",
                "sdk_version": "15.5",
                "sdk_build": "24F74",
                "deployment_target": "15.0",
                "observed_runner": {"image_os": "macos15", "image_version": "test"},
            }

        receipts = {
            "gcc": index_receipt(
                "gcc", "ghcr.io/ximicpp/typelayout-gcc16", "16.2.0", "e"
            ),
            "p2996": index_receipt(
                "p2996", "ghcr.io/ximicpp/typelayout-p2996", revision, "f"
            ),
            "macos": {
                "schema": 1,
                "source_sha": source_sha,
                "sources_sha256": sources_sha256,
                "candidate_batch_id": candidate_batch_id,
                "workflow_run": "42.1",
                "producer_workflow_run": "42.2",
                "release_producer_workflow_run": "42.1",
                "candidate_producer_workflow_runs": {
                    "arm64_macos_clang": "42.1",
                    "x86_64_macos_clang": "42.1",
                },
                "nodes": {
                    "arm64_macos_clang": mac_node("arm64", "0"),
                    "x86_64_macos_clang": mac_node("x86_64", "1"),
                },
            },
        }

        attempt_one_receipts = json.loads(json.dumps(receipts))
        for name in ("gcc", "p2996"):
            attempt_one_receipts[name]["producer_workflow_run"] = "42.1"
        attempt_one_receipts["macos"]["producer_workflow_run"] = "42.1"
        completed, sealed = self.run_candidate_sealer(
            attempt_one_receipts,
            current_attempt="1",
        )
        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertEqual(sealed["workflow_run"], "42.1")

        completed, sealed = self.run_candidate_sealer(receipts)
        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertEqual(sealed["workflow_run"], "42.1")

        wrong_batch = json.loads(json.dumps(receipts))
        wrong_batch["macos"]["candidate_batch_id"] = "wrong"
        completed, _ = self.run_candidate_sealer(wrong_batch)
        self.assertNotEqual(completed.returncode, 0)

        future_producer = json.loads(json.dumps(receipts))
        future_producer["gcc"]["platform_producer_workflow_runs"]["linux/amd64"] = "42.3"
        completed, _ = self.run_candidate_sealer(future_producer)
        self.assertNotEqual(completed.returncode, 0)

        second_attempt_origin = json.loads(json.dumps(receipts))
        for receipt in second_attempt_origin.values():
            receipt["workflow_run"] = "42.2"
        second_attempt_origin["gcc"]["platform_producer_workflow_runs"] = {
            "linux/amd64": "42.2",
            "linux/arm64": "42.2",
        }
        second_attempt_origin["p2996"]["platform_producer_workflow_runs"] = {
            "linux/amd64": "42.2",
            "linux/arm64": "42.2",
        }
        second_attempt_origin["macos"]["release_producer_workflow_run"] = "42.2"
        second_attempt_origin["macos"]["candidate_producer_workflow_runs"] = {
            "arm64_macos_clang": "42.2",
            "x86_64_macos_clang": "42.2",
        }
        completed, sealed = self.run_candidate_sealer(second_attempt_origin)
        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertEqual(sealed["workflow_run"], "42.2")

        predating_producer = json.loads(json.dumps(second_attempt_origin))
        predating_producer["gcc"]["platform_producer_workflow_runs"][
            "linux/amd64"
        ] = "42.1"
        completed, _ = self.run_candidate_sealer(predating_producer)
        self.assertNotEqual(completed.returncode, 0)

        mixed_origin = json.loads(json.dumps(receipts))
        mixed_origin["p2996"]["workflow_run"] = "42.2"
        completed, _ = self.run_candidate_sealer(mixed_origin)
        self.assertNotEqual(completed.returncode, 0)

    def test_batch_workflow_identity_is_always_derived_from_the_batch(self):
        self.assertNotRegex(
            self.workflow,
            r'workflow_run\s*=\s*f["\'][^"\']*\}\.1["\']',
        )
        initializer = self.job("initialize_candidate_batch")
        for contract in (
            "if: github.event_name == 'workflow_dispatch'",
            "candidate-batch-manifest",
            "actions/runs/${GITHUB_RUN_ID}/artifacts",
            "artifact-ids: ${{ steps.lookup.outputs.artifact_id }}",
            "id: upload_candidate",
            "if: steps.lookup.outputs.existing == 'false'",
            '"workflow_run": producer_workflow_run',
            "producer_workflow_run =",
            "GITHUB_RUN_ATTEMPT",
        ):
            with self.subTest(initializer_contract=contract):
                self.assertIn(contract, initializer)
        self.assertNotIn('f"{os.environ[\'GITHUB_RUN_ID\']}.1"', initializer)

        for name in (
            "build_gcc_amd64",
            "build_gcc_arm64",
            "build_p2996_amd64",
            "build_p2996_arm64",
            "build_p2996_macos_arm64",
            "build_p2996_macos_x86_64",
        ):
            with self.subTest(job=name):
                block = self.job(name)
                self.assertIn("needs: initialize_candidate_batch", block)
                self.assertIn('batch_workflow_run = os.environ["BATCH_WORKFLOW_RUN"]', block)

        for name in ("index_gcc", "index_p2996", "publish_macos_release", "seal_candidate_lock"):
            with self.subTest(consumer=name):
                block = self.job(name)
                self.assertNotIn("GITHUB_RUN_ID']}.1", block)
                self.assertIn("workflow_run", block)

        completed, record, outputs, _, _ = self.run_candidate_batch_manifest("1")
        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertEqual(record["workflow_run"], "42.1")
        self.assertEqual(outputs["workflow_run"], "42.1")

        completed, record, outputs, before, after = self.run_candidate_batch_manifest(
            "2",
            existing_origin="42.1",
        )
        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertEqual(record["workflow_run"], "42.1")
        self.assertEqual(outputs["workflow_run"], "42.1")
        self.assertEqual(after, before)

        completed, record, outputs, _, _ = self.run_candidate_batch_manifest("2")
        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertEqual(record["workflow_run"], "42.2")
        self.assertEqual(outputs["workflow_run"], "42.2")

        completed, _, _, _, _ = self.run_candidate_batch_manifest(
            "1",
            existing_origin="42.2",
        )
        self.assertNotEqual(completed.returncode, 0)

        completed, _, _, _, _ = self.run_candidate_batch_manifest(
            "2",
            existing_origin="42.1",
            noncanonical_existing=True,
        )
        self.assertNotEqual(completed.returncode, 0)

    def test_default_branch_promotion_consumes_only_committed_digests(self):
        block = self.job("promote_legacy_aliases")
        self.assertIn("github.event.repository.default_branch", block)
        self.assertIn(".github/docker/toolchains.lock", block)
        self.assertIn("validate-toolchain-locks.py", block)
        self.assertIn("verify_remote_index", block)
        self.assertIn("gcc-descriptors.json", block)
        self.assertIn("p2996-descriptors.json", block)
        self.assertIn('set(raw) != {"schemaVersion", "mediaType", "manifests"}', block)
        self.assertIn('hashlib.sha256(raw_bytes).hexdigest()', block)
        self.assertIn("build-p2996-macos.sh", self.workflow)
        self.assertNotIn("build-p2996-macos.sh", block)
        self.assertNotIn("softprops/action-gh-release", block)
        self.assertNotIn("docker buildx bake", block)
        for contract in (
            "Debug",
            "Release",
            "compat_ci_export",
            "compat_ci_check_linux",
            "compat_check_demo_negative",
            "Layout mismatch",
            "not byte-copy safe",
            "/opt/p2996-toolchain/lib/x86_64-unknown-linux-gnu",
        ):
            with self.subTest(contract=contract):
                self.assertIn(contract, block)
        self.assertLess(block.index("compat_check_demo_negative"), block.index(":latest"))

    def test_alias_promotion_rolls_back_both_repositories_on_partial_failure(self):
        completed = self.run_alias_promotion_transaction()
        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertIn("write:gcc", completed.stdout)
        self.assertIn("write:p2996", completed.stdout)
        self.assertIn("verify:new", completed.stdout)
        self.assertNotIn("rollback:", completed.stdout)

        completed = self.run_alias_promotion_transaction(fail_write="p2996")
        self.assertEqual(completed.returncode, 23, completed.stderr)
        self.assertIn("rollback:gcc", completed.stdout)
        self.assertIn("rollback:p2996", completed.stdout)

        completed = self.run_alias_promotion_transaction(
            fail_write="p2996", fail_rollback="gcc"
        )
        self.assertEqual(completed.returncode, 70, completed.stderr)
        self.assertIn("rollback:gcc", completed.stdout)
        self.assertIn("rollback:p2996", completed.stdout)
        self.assertIn("ROLLBACK FAILED", completed.stderr)

        block = self.job("promote_legacy_aliases")
        self.assertIn("gcc_old_digest", block)
        self.assertIn("p2996_old_digest", block)
        self.assertIn("verify_alias_digest", block)

    def test_actions_are_immutable_and_no_extra_build_action_is_introduced(self):
        pins = (
            "actions/checkout@11d5960a326750d5838078e36cf38b85af677262",
            "actions/upload-artifact@ea165f8d65b6e75b540449e92b4886f43607fa02",
            "actions/download-artifact@d3f86a106a0bac45b974a628896c90dbdf5c8093",
            "docker/login-action@c94ce9fb468520275223c153574b00df6fe4bcc9",
            "docker/setup-buildx-action@8d2750c68a42422c14e847fe6c8ac0403b4cbd6f",
            "softprops/action-gh-release@3bb12739c298aeb8a4eeaf626c5b8d85266b0e65",
        )
        for pin in pins:
            with self.subTest(pin=pin):
                self.assertIn(pin, self.workflow)
        for workflow_name, workflow in (
            ("toolchain", self.workflow),
            ("ci", self.ci_workflow),
            ("compat", self.compat_workflow),
        ):
            for action_ref in re.findall(r"(?m)^\s*- uses:\s*([^\s]+)", workflow):
                with self.subTest(workflow=workflow_name, action=action_ref):
                    self.assertRegex(action_ref, r"@[0-9a-f]{40}$")
        self.assertNotIn("docker/metadata-action", self.workflow)
        self.assertNotIn("docker/bake-action", self.workflow)

    def test_private_ghcr_jobs_are_unreachable_from_fork_pull_requests(self):
        same_repository_only = (
            "github.event_name == 'push' || github.event_name == 'workflow_dispatch' || "
            "(github.event_name == 'pull_request' && "
            "github.event.pull_request.head.repo.full_name == github.repository)"
        )
        for name, workflow in (
            ("ci", self.ci_workflow),
            ("compat", self.compat_workflow),
        ):
            with self.subTest(workflow=name):
                self.assertRegex(
                    workflow,
                    r"(?ms)^permissions:\n  contents: read$",
                )
                jobs = re.findall(
                    r"(?ms)^  ([a-zA-Z0-9_]+):\n(.*?)(?=^  [a-zA-Z0-9_]+:\n|\Z)",
                    workflow,
                )
                private_jobs = [
                    (job, block)
                    for job, block in jobs
                    if "image: ghcr.io/ximicpp/typelayout-" in block
                ]
                self.assertGreater(len(private_jobs), 0)
                for job, block in private_jobs:
                    with self.subTest(workflow=name, job=job):
                        self.assertRegex(
                            block,
                            r"(?ms)^    permissions:\n"
                            r"      contents: read\n"
                            r"      packages: read$",
                        )
                        self.assertIn("username: ${{ github.actor }}", block)
                        self.assertIn(
                            "password: ${{ secrets.GITHUB_TOKEN }}", block
                        )
                        if job == "compat_cross_target":
                            self.assertIn("github.event_name == 'push'", block)
                            self.assertIn(
                                "github.event_name == 'workflow_dispatch'", block
                            )
                            self.assertNotIn(
                                "github.event_name == 'pull_request'", block
                            )
                        else:
                            self.assertIn(same_repository_only, block)

    def test_self_hosted_jobs_are_never_reachable_from_pull_requests(self):
        jobs = re.findall(
            r"(?ms)^  ([a-zA-Z0-9_]+):\n(.*?)(?=^  [a-zA-Z0-9_]+:\n|\Z)",
            self.compat_workflow,
        )
        self_hosted = [
            (job, block) for job, block in jobs if "self-hosted" in block
        ]
        self.assertGreater(len(self_hosted), 0)
        for job, block in self_hosted:
            with self.subTest(job=job):
                self.assertIn("github.event_name == 'push'", block)
                self.assertIn("github.event_name == 'workflow_dispatch'", block)
                self.assertNotIn("github.event_name == 'pull_request'", block)
                self.assertNotIn("packages: read", block)

    def test_every_ctest_command_fails_when_no_tests_are_registered(self):
        for workflow_name, workflow in (
            ("toolchain", self.workflow),
            ("ci", self.ci_workflow),
            ("compat", self.compat_workflow),
        ):
            commands = re.findall(r"(?m)^.*\bctest\b.*$", workflow)
            self.assertGreater(len(commands), 0)
            for command in commands:
                with self.subTest(workflow=workflow_name, command=command):
                    self.assertIn("--no-tests=error", command)

        with tempfile.TemporaryDirectory() as directory:
            permissive = subprocess.run(
                ["ctest", "--test-dir", directory], capture_output=True, text=True
            )
            strict = subprocess.run(
                ["ctest", "--test-dir", directory, "--no-tests=error"],
                capture_output=True,
                text=True,
            )
        self.assertEqual(permissive.returncode, 0)
        self.assertNotEqual(strict.returncode, 0)

    def test_workflow_yaml_bash_and_embedded_python_are_syntactically_valid(self):
        for path in (TOOLCHAIN_WORKFLOW, CI_WORKFLOW, COMPAT_WORKFLOW):
            workflow = yaml.safe_load(path.read_text(encoding="utf-8"))
            self.assertIsInstance(workflow, dict)
            jobs = workflow.get("jobs")
            self.assertIsInstance(jobs, dict)
            for job_name, job in jobs.items():
                for index, step in enumerate(job.get("steps", [])):
                    script = step.get("run")
                    if not isinstance(script, str):
                        continue
                    bash_script = re.sub(r"\$\{\{.*?\}\}", "github_expression", script)
                    completed = subprocess.run(
                        ["bash", "-n"],
                        input=bash_script.encode(),
                        capture_output=True,
                    )
                    with self.subTest(path=path.name, job=job_name, step=index):
                        self.assertEqual(
                            completed.returncode,
                            0,
                            completed.stderr.decode(errors="replace"),
                        )
                    for python_index, match in enumerate(
                        re.finditer(r"(?ms)<<'PY'\n(.*?)^PY$", script)
                    ):
                        with self.subTest(
                            path=path.name,
                            job=job_name,
                            step=index,
                            python=python_index,
                        ):
                            compile(match.group(1), f"{path}:{job_name}:{index}", "exec")


if __name__ == "__main__":
    unittest.main()
