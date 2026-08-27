#!/usr/bin/env python3
"""Strict evidence boundary for the relocatable-world native matrix."""

import argparse
import hashlib
import json
import subprocess
from pathlib import Path


NODES = (
    "x86_64_linux_gcc",
    "x86_64_linux_clang",
    "arm64_linux_gcc",
    "arm64_linux_clang",
    "arm64_macos_clang",
    "x86_64_macos_clang",
)

LOCAL_NODES = (
    "x86_64_linux_gcc",
    "x86_64_linux_clang",
    "arm64_linux_gcc",
    "arm64_linux_clang",
    "arm64_macos_clang",
)

KEYS = (
    "WorldSnapshot",
    "Entity",
    "EntityRelativePtr",
    "EntityIndexEntry",
)

TRANSFER_STATUSES = (
    "PASS",
    "SKIPPED_TYPELAYOUT_REJECT",
    "REJECT_ENVELOPE",
    "REJECT_REGION",
    "REJECT_GRAPH",
    "INCOMPLETE",
)

PROFILES = ("authoritative", "local-arm64-macos")
PROBE_KEYS = (
    "char_bit",
    "pointer_bits",
    "endian",
    "reflection",
    "memcpy_object_lifetime",
    "memcpy_array_lifetime",
)
COMPILER_KEYS = (
    "family",
    "revision",
    "version",
    "target",
    "stdlib",
    "xcode_version",
    "xcode_build",
    "sdk_version",
    "sdk_build",
    "deployment_target",
    "sdk_locked",
)
BUILD_KEYS = (
    "profile",
    "execution",
    "runner",
    "runner_image",
    "source_sha",
    "flags",
    "workflow_run",
    "toolchain_artifact_sha256",
)

APPLE_IDENTITY_KEYS = (
    "xcode_version",
    "xcode_build",
    "sdk_version",
    "sdk_build",
    "deployment_target",
)

NODE_POLICY_KEYS = (
    "node",
    "compiler_family",
    "compiler_revision",
    "compiler_version",
    "target",
    "stdlib",
    "flags",
    "toolchain_artifact_sha256",
) + APPLE_IDENTITY_KEYS

_AUTHORITATIVE_RUNNERS = {
    "x86_64_linux_gcc": "ubuntu-24.04",
    "x86_64_linux_clang": "ubuntu-24.04",
    "arm64_linux_gcc": "ubuntu-24.04-arm",
    "arm64_linux_clang": "ubuntu-24.04-arm",
    "arm64_macos_clang": "macos-15",
    "x86_64_macos_clang": "macos-15-intel",
}

_LOCAL_EXECUTION = {
    "x86_64_linux_gcc": "emulated",
    "x86_64_linux_clang": "emulated",
    "arm64_linux_gcc": "native",
    "arm64_linux_clang": "native",
    "arm64_macos_clang": "native",
}


class EvidenceError(ValueError):
    """Raised when evidence does not satisfy the fixed matrix contract."""


def _duplicate_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise EvidenceError(f"duplicate JSON key {key!r}")
        result[key] = value
    return result


def _reject_json_constant(value):
    raise EvidenceError(f"non-finite JSON constant {value!r} is not permitted")


def load_json(path):
    path = Path(path)
    try:
        value = json.loads(
            path.read_text(encoding="utf-8"),
            object_pairs_hook=_duplicate_object,
            parse_constant=_reject_json_constant,
        )
    except EvidenceError:
        raise
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise EvidenceError(f"cannot read strict JSON {path}: {error}") from error
    if not isinstance(value, dict):
        raise EvidenceError(f"{path}: top-level JSON value must be an object")
    return value


def _write_json(path, value):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + ".tmp")
    with temporary.open("w", encoding="utf-8", newline="\n") as stream:
        stream.write(json.dumps(value, indent=2, sort_keys=False) + "\n")
    temporary.replace(path)


def _sha256(path):
    try:
        return hashlib.sha256(Path(path).read_bytes()).hexdigest()
    except OSError as error:
        raise EvidenceError(f"cannot hash {path}: {error}") from error


def _expect_object(value, where):
    if not isinstance(value, dict):
        raise EvidenceError(f"{where} must be an object")
    return value


def _expect_exact_keys(value, expected, where):
    value = _expect_object(value, where)
    if set(value) != set(expected) or len(value) != len(expected):
        raise EvidenceError(
            f"{where} keys must be exactly {', '.join(expected)}"
        )
    return value


def _expect_nonempty_string(value, where):
    if not isinstance(value, str) or not value:
        raise EvidenceError(f"{where} must be a non-empty string")
    return value


def _expect_boolean(value, where):
    if type(value) is not bool:
        raise EvidenceError(f"{where} must be a JSON boolean")
    return value


def _expect_integer(value, where):
    if type(value) is not int:
        raise EvidenceError(f"{where} must be a JSON integer")
    return value


def _is_lower_hex(value, length):
    return (
        isinstance(value, str)
        and len(value) == length
        and all(character in "0123456789abcdef" for character in value)
    )


def _expect_sha256(value, where):
    if not _is_lower_hex(value, 64):
        raise EvidenceError(f"{where} must be a 64-character lowercase SHA256")
    return value


def _expect_source_sha(value, where):
    if not _is_lower_hex(value, 40):
        raise EvidenceError(f"{where} must be a 40-character lowercase source SHA")
    return value


def _is_canonical_positive_decimal(value):
    return (
        isinstance(value, str)
        and value
        and value[0] != "0"
        and all(character in "0123456789" for character in value)
    )


def _expect_workflow_run(value, profile, source_sha, where):
    _expect_nonempty_string(value, where)
    validate_profile(profile)
    _expect_source_sha(source_sha, f"{where} source SHA")
    if profile == "authoritative":
        parts = value.split(".")
        if len(parts) != 2 or not all(
            _is_canonical_positive_decimal(part) for part in parts
        ):
            raise EvidenceError(
                f"{where} must be canonical positive run_id.run_attempt"
            )
        return value

    parts = value.split("-")
    if len(parts) != 4:
        raise EvidenceError(
            f"{where} must be local-<head>-<UTC timestamp>-<pid>"
        )
    prefix, head, timestamp, process_id = parts
    timestamp_digits = timestamp[:8] + timestamp[9:15]
    if (
        prefix != "local"
        or head != source_sha[:12]
        or len(timestamp) != 16
        or timestamp[8:9] != "T"
        or timestamp[15:16] != "Z"
        or len(timestamp_digits) != 14
        or not all(
            character in "0123456789" for character in timestamp_digits
        )
        or not _is_canonical_positive_decimal(process_id)
    ):
        raise EvidenceError(
            f"{where} must be local-<12-hex-head>-<UTC timestamp>-<pid>"
        )
    return value


def validate_node(node):
    if node not in NODES:
        raise EvidenceError(f"unknown matrix node {node!r}")
    return node


def validate_profile(profile):
    if profile not in PROFILES:
        raise EvidenceError(f"unknown evidence profile {profile!r}")
    return profile


def profile_nodes(profile):
    validate_profile(profile)
    return NODES if profile == "authoritative" else LOCAL_NODES


def _validate_keyed_booleans(value, where):
    value = _expect_exact_keys(value, KEYS, where)
    for key in KEYS:
        _expect_boolean(value[key], f"{where}.{key}")
    return value


def _validate_signatures(value, where):
    value = _expect_exact_keys(value, KEYS, where)
    for key in KEYS:
        _expect_nonempty_string(value[key], f"{where}.{key}")
    return value


def _expected_family(node):
    return "gcc" if node.endswith("_gcc") else "clang"


def _is_macos(node):
    return "_macos_" in node


def _validate_probe_values(value, where="probe"):
    value = _expect_exact_keys(value, PROBE_KEYS, where)
    if _expect_integer(value["char_bit"], f"{where}.char_bit") <= 0:
        raise EvidenceError(f"{where}.char_bit must be positive")
    if _expect_integer(value["pointer_bits"], f"{where}.pointer_bits") <= 0:
        raise EvidenceError(f"{where}.pointer_bits must be positive")
    if value["endian"] not in ("little", "big"):
        raise EvidenceError(f"{where}.endian must be little or big")
    for key in PROBE_KEYS[3:]:
        _expect_boolean(value[key], f"{where}.{key}")
    return value


def _validate_compiler(value, node, where="compiler"):
    value = _expect_exact_keys(value, COMPILER_KEYS, where)
    for key in COMPILER_KEYS[:-1]:
        _expect_nonempty_string(value[key], f"{where}.{key}")
    _expect_boolean(value["sdk_locked"], f"{where}.sdk_locked")
    if value["family"] != _expected_family(node):
        raise EvidenceError(
            f"{where}.family does not match node {node}: {value['family']!r}"
        )
    if not _is_macos(node):
        for key in APPLE_IDENTITY_KEYS:
            if value[key] != "none":
                raise EvidenceError(f"Linux {where}.{key} must be literal 'none'")
        if not value["sdk_locked"]:
            raise EvidenceError(f"Linux {where}.sdk_locked must be true")
    return value


def validate_probe(path):
    record = load_json(path)
    _expect_exact_keys(
        record,
        ("schema", "node", "probe", "admission", "compiler", "environment"),
        "probe top-level",
    )
    if type(record["schema"]) is not int or record["schema"] != 1:
        raise EvidenceError("probe schema must be integer 1")
    node = validate_node(record["node"])
    _validate_probe_values(record["probe"])
    _validate_keyed_booleans(record["admission"], "admission")
    _validate_compiler(record["compiler"], node)
    environment = _expect_exact_keys(
        record["environment"], ("runner", "runner_image"), "environment"
    )
    _expect_nonempty_string(environment["runner"], "environment.runner")
    _expect_nonempty_string(
        environment["runner_image"], "environment.runner_image"
    )
    return record


def _validate_facts(path):
    path = Path(path)
    record = load_json(path)
    _expect_exact_keys(
        record,
        ("schema", "node", "admission", "signatures"),
        "producer facts top-level",
    )
    if type(record["schema"]) is not int or record["schema"] != 1:
        raise EvidenceError("producer facts schema must be integer 1")
    node = validate_node(record["node"])
    expected_filename = f"{node}.producer-facts.json"
    if path.name != expected_filename:
        raise EvidenceError(
            f"producer facts filename must be exactly {expected_filename!r}"
        )
    _validate_keyed_booleans(record["admission"], "producer facts admission")
    _validate_signatures(record["signatures"], "producer facts signatures")
    return record


def _validate_filename(value, expected, where):
    _expect_nonempty_string(value, where)
    path = Path(value)
    if (
        path.is_absolute()
        or value in (".", "..")
        or "/" in value
        or "\\" in value
        or path.name != value
        or value != expected
    ):
        raise EvidenceError(f"{where} filename must be exactly {expected!r}")
    return value


def _validate_artifact(entry, kind, node, provenance_path):
    entry = _expect_exact_keys(entry, ("filename", "sha256"), f"artifacts.{kind}")
    suffix = ".sig.hpp" if kind == "signature" else ".region"
    filename = _validate_filename(
        entry["filename"], f"{node}{suffix}", f"artifacts.{kind}"
    )
    expected_digest = _expect_sha256(
        entry["sha256"], f"artifacts.{kind}.SHA256"
    )
    provenance_path = Path(provenance_path)
    artifact_path = provenance_path.parent / filename
    try:
        resolved_parent = provenance_path.parent.resolve(strict=True)
        resolved_artifact = artifact_path.resolve(strict=True)
    except OSError as error:
        raise EvidenceError(f"artifacts.{kind} file is unavailable: {error}") from error
    if resolved_artifact.parent != resolved_parent or not resolved_artifact.is_file():
        raise EvidenceError(f"artifacts.{kind} filename escapes the evidence bundle")
    actual_digest = _sha256(resolved_artifact)
    if actual_digest != expected_digest:
        raise EvidenceError(
            f"artifacts.{kind} SHA256 mismatch: expected {expected_digest}, "
            f"got {actual_digest}"
        )


def _validate_build(value, node):
    value = _expect_exact_keys(value, BUILD_KEYS, "build")
    profile = validate_profile(value["profile"])
    if node not in profile_nodes(profile):
        raise EvidenceError(f"node {node} is not part of profile {profile}")
    if value["execution"] not in ("native", "emulated"):
        raise EvidenceError("build.execution must be native or emulated")
    for key in ("runner", "runner_image", "flags"):
        _expect_nonempty_string(value[key], f"build.{key}")
    source_sha = _expect_source_sha(value["source_sha"], "build.source_sha")
    _expect_workflow_run(
        value["workflow_run"], profile, source_sha, "build.workflow_run"
    )
    _expect_sha256(
        value["toolchain_artifact_sha256"],
        "build.toolchain_artifact_sha256",
    )
    if profile == "authoritative":
        if value["execution"] != "native":
            raise EvidenceError("authoritative build.execution must be native")
        if value["runner"] != _AUTHORITATIVE_RUNNERS[node]:
            raise EvidenceError("authoritative build.runner does not match its node")
    elif value["execution"] != _LOCAL_EXECUTION[node]:
        raise EvidenceError("local build.execution does not match its node")
    return value


def _validate_provenance_filename(path, node):
    expected = f"{node}.provenance.json"
    if Path(path).name != expected:
        raise EvidenceError(f"provenance filename must be exactly {expected!r}")


def validate_provenance(path):
    path = Path(path)
    record = load_json(path)
    status = record.get("status")
    if status == "INCOMPLETE":
        _expect_exact_keys(
            record,
            ("schema", "node", "status", "error"),
            "provenance top-level",
        )
        if type(record["schema"]) is not int or record["schema"] != 1:
            raise EvidenceError("provenance schema must be integer 1")
        node = validate_node(record["node"])
        _validate_provenance_filename(path, node)
        _expect_nonempty_string(record["error"], "provenance.error")
        return record

    _expect_exact_keys(
        record,
        (
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
        ),
        "provenance top-level",
    )
    if type(record["schema"]) is not int or record["schema"] != 1:
        raise EvidenceError("provenance schema must be integer 1")
    node = validate_node(record["node"])
    _validate_provenance_filename(path, node)
    if status not in ("READY", "REJECT"):
        raise EvidenceError("provenance status must be READY, REJECT, or INCOMPLETE")

    probe = _validate_probe_values(record["probe"])
    if (
        probe["char_bit"] != 8
        or probe["pointer_bits"] != 64
        or probe["endian"] != "little"
        or not probe["reflection"]
        or not probe["memcpy_object_lifetime"]
        or not probe["memcpy_array_lifetime"]
    ):
        raise EvidenceError("evaluated provenance requires all platform probe gates")
    admission = _validate_keyed_booleans(record["admission"], "admission")
    _validate_signatures(record["signatures"], "signatures")
    _validate_compiler(record["compiler"], node)
    build = _validate_build(record["build"], node)
    locks = _expect_exact_keys(
        record["locks"], ("sources_sha256", "outputs_sha256"), "locks"
    )
    _expect_sha256(locks["sources_sha256"], "locks.sources_sha256")
    _expect_sha256(locks["outputs_sha256"], "locks.outputs_sha256")

    all_admitted = all(admission[key] for key in KEYS)
    artifacts = _expect_object(record["artifacts"], "artifacts")
    if status == "READY":
        if not all_admitted:
            raise EvidenceError("READY provenance requires four admitted keys")
        _expect_exact_keys(artifacts, ("signature", "region"), "artifacts")
        _validate_artifact(artifacts["signature"], "signature", node, path)
        _validate_artifact(artifacts["region"], "region", node, path)
    else:
        if all_admitted:
            raise EvidenceError("REJECT provenance requires at least one Admission false")
        if artifacts:
            raise EvidenceError("REJECT provenance artifacts must be empty")

    if build["profile"] == "authoritative" and not record["compiler"]["sdk_locked"]:
        raise EvidenceError("authoritative compiler.sdk_locked must be true")
    return record


def _require_fields(record, keys, where):
    record = _expect_object(record, where)
    for key in keys:
        if key not in record:
            raise EvidenceError(f"{where} is missing required field {key}")
    return record


def _expect_digest_reference(value, where):
    _expect_nonempty_string(value, where)
    prefix = "sha256:"
    if not value.startswith(prefix):
        raise EvidenceError(f"{where} must be a digest-qualified sha256 reference")
    return _expect_sha256(value[len(prefix):], where)


def _load_lock_policy(sources_lock, outputs_lock, node):
    node = validate_node(node)
    sources_lock = Path(sources_lock)
    outputs_lock = Path(outputs_lock)
    sources = load_json(sources_lock)
    outputs = load_json(outputs_lock)
    _expect_exact_keys(
        sources,
        ("schema", "gcc", "p2996", "linux", "macos", "actions", "recipes"),
        "source lock top-level",
    )
    _expect_exact_keys(
        outputs,
        (
            "schema",
            "sources_sha256",
            "source_sha",
            "workflow_run",
            "linux",
            "macos",
        ),
        "output lock top-level",
    )
    if sources["schema"] != 1 or type(sources["schema"]) is not int:
        raise EvidenceError("source lock schema must be integer 1")
    if outputs["schema"] != 1 or type(outputs["schema"]) is not int:
        raise EvidenceError("output lock schema must be integer 1")
    sources_digest = _sha256(sources_lock)
    if outputs["sources_sha256"] != sources_digest:
        raise EvidenceError("output lock sources_sha256 does not bind the source lock")
    output_source_sha = _expect_source_sha(
        outputs["source_sha"], "output lock.source_sha"
    )
    _expect_workflow_run(
        outputs["workflow_run"],
        "authoritative",
        output_source_sha,
        "output lock.workflow_run",
    )

    gcc_source = _require_fields(
        sources["gcc"],
        ("version", "compiler_family", "compiler_revision", "flags"),
        "source lock.gcc",
    )
    clang_source = _require_fields(
        sources["p2996"],
        (
            "repository",
            "commit",
            "compiler_family",
            "compiler_revision",
            "flags",
        ),
        "source lock.p2996",
    )
    for key in ("version", "compiler_family", "compiler_revision", "flags"):
        _expect_nonempty_string(gcc_source[key], f"source lock.gcc.{key}")
    for key in (
        "repository",
        "commit",
        "compiler_family",
        "compiler_revision",
        "flags",
    ):
        _expect_nonempty_string(clang_source[key], f"source lock.p2996.{key}")
    if gcc_source["compiler_family"] != "gcc":
        raise EvidenceError("source lock.gcc compiler_family must be gcc")
    if clang_source["compiler_family"] != "clang":
        raise EvidenceError("source lock.p2996 compiler_family must be clang")
    if clang_source["commit"] != clang_source["compiler_revision"]:
        raise EvidenceError("source lock.p2996 commit and compiler_revision differ")

    linux_source = _require_fields(
        sources["linux"], ("platforms", "docker"), "source lock.linux"
    )
    source_platforms = _expect_exact_keys(
        linux_source["platforms"],
        ("linux/amd64", "linux/arm64"),
        "source lock.linux.platforms",
    )
    expected_architectures = {
        "linux/amd64": "x86_64",
        "linux/arm64": "arm64",
    }
    for platform, architecture in expected_architectures.items():
        platform_source = _require_fields(
            source_platforms[platform],
            ("architecture",),
            f"source lock.linux.platforms.{platform}",
        )
        if platform_source["architecture"] != architecture:
            raise EvidenceError(
                f"source lock platform {platform} architecture must be {architecture}"
            )

    source_macos = _require_fields(
        sources["macos"], ("nodes",), "source lock.macos"
    )
    source_macos_nodes = _expect_exact_keys(
        source_macos["nodes"],
        ("arm64_macos_clang", "x86_64_macos_clang"),
        "source lock.macos.nodes",
    )
    output_linux = _expect_exact_keys(
        outputs["linux"], ("gcc", "p2996"), "output lock.linux"
    )
    output_macos = _expect_exact_keys(
        outputs["macos"],
        ("arm64_macos_clang", "x86_64_macos_clang"),
        "output lock.macos",
    )

    if _is_macos(node):
        source_node = _require_fields(
            source_macos_nodes[node],
            ("flags",) + APPLE_IDENTITY_KEYS,
            f"source lock.macos.nodes.{node}",
        )
        output_node = _require_fields(
            output_macos[node],
            (
                "url",
                "archive_sha256",
                "compiler_revision",
                "compiler_version",
                "target",
                "stdlib",
            )
            + APPLE_IDENTITY_KEYS
            + ("observed_runner",),
            f"output lock.macos.{node}",
        )
        for key in ("flags",) + APPLE_IDENTITY_KEYS:
            _expect_nonempty_string(
                source_node[key], f"source lock.macos.nodes.{node}.{key}"
            )
        for key in (
            "url",
            "compiler_revision",
            "compiler_version",
            "target",
            "stdlib",
        ) + APPLE_IDENTITY_KEYS:
            _expect_nonempty_string(
                output_node[key], f"output lock.macos.{node}.{key}"
            )
        artifact_digest = _expect_sha256(
            output_node["archive_sha256"],
            f"output lock.macos.{node}.archive_sha256",
        )
        observed = _expect_exact_keys(
            output_node["observed_runner"],
            ("image_os", "image_version"),
            f"output lock.macos.{node}.observed_runner",
        )
        for key in ("image_os", "image_version"):
            _expect_nonempty_string(
                observed[key], f"output lock.macos.{node}.observed_runner.{key}"
            )
        if output_node["compiler_revision"] != clang_source["compiler_revision"]:
            raise EvidenceError(
                f"output lock macOS compiler revision differs for {node}"
            )
        for key in APPLE_IDENTITY_KEYS:
            if output_node[key] != source_node[key]:
                raise EvidenceError(
                    f"output lock macOS {key} differs from source hard lock for {node}"
                )
        policy = {
            "node": node,
            "compiler_family": clang_source["compiler_family"],
            "compiler_revision": clang_source["compiler_revision"],
            "compiler_version": output_node["compiler_version"],
            "target": output_node["target"],
            "stdlib": output_node["stdlib"],
            "flags": source_node["flags"],
            "toolchain_artifact_sha256": artifact_digest,
            **{key: output_node[key] for key in APPLE_IDENTITY_KEYS},
        }
    else:
        toolchain = "gcc" if node.endswith("_gcc") else "p2996"
        platform = "linux/arm64" if node.startswith("arm64_") else "linux/amd64"
        source_compiler = gcc_source if toolchain == "gcc" else clang_source
        output_toolchain = _require_fields(
            output_linux[toolchain],
            (
                "repository",
                "index_digest",
                "compiler_revision",
                "compiler_version",
                "stdlib",
                "platforms",
            ),
            f"output lock.linux.{toolchain}",
        )
        for key in (
            "repository",
            "compiler_revision",
            "compiler_version",
            "stdlib",
        ):
            _expect_nonempty_string(
                output_toolchain[key], f"output lock.linux.{toolchain}.{key}"
            )
        _expect_digest_reference(
            output_toolchain["index_digest"],
            f"output lock.linux.{toolchain}.index_digest",
        )
        platform_outputs = _expect_exact_keys(
            output_toolchain["platforms"],
            ("linux/amd64", "linux/arm64"),
            f"output lock.linux.{toolchain}.platforms",
        )
        platform_output = _require_fields(
            platform_outputs[platform],
            ("manifest_digest", "target"),
            f"output lock.linux.{toolchain}.platforms.{platform}",
        )
        _expect_nonempty_string(
            platform_output["target"],
            f"output lock.linux.{toolchain}.platforms.{platform}.target",
        )
        artifact_digest = _expect_digest_reference(
            platform_output["manifest_digest"],
            f"output lock.linux.{toolchain}.platforms.{platform}.manifest_digest",
        )
        if output_toolchain["compiler_revision"] != source_compiler["compiler_revision"]:
            raise EvidenceError(
                f"output lock Linux compiler revision differs for {node}"
            )
        policy = {
            "node": node,
            "compiler_family": source_compiler["compiler_family"],
            "compiler_revision": source_compiler["compiler_revision"],
            "compiler_version": output_toolchain["compiler_version"],
            "target": platform_output["target"],
            "stdlib": output_toolchain["stdlib"],
            "flags": source_compiler["flags"],
            "toolchain_artifact_sha256": artifact_digest,
            **{key: "none" for key in APPLE_IDENTITY_KEYS},
        }

    _expect_exact_keys(policy, NODE_POLICY_KEYS, f"normalized policy for {node}")
    return policy, sources_digest, _sha256(outputs_lock)


def _require_platform_probe_pass(probe):
    expected = {
        "char_bit": 8,
        "pointer_bits": 64,
        "endian": "little",
        "reflection": True,
        "memcpy_object_lifetime": True,
        "memcpy_array_lifetime": True,
    }
    for key in PROBE_KEYS:
        if probe["probe"][key] != expected[key]:
            raise EvidenceError(
                f"platform probe gate {key} failed: {probe['probe'][key]!r}"
            )


def _read_generated_signature_header(path, node):
    try:
        text = Path(path).read_text(encoding="utf-8")
    except (OSError, UnicodeError) as error:
        raise EvidenceError(f"cannot read signature header {path}: {error}") from error
    namespace_marker = f"namespace {node} {{"
    if text.count(namespace_marker) != 1:
        raise EvidenceError(
            f"signature header must declare exactly one namespace {node}"
        )
    if (
        text.count("_layout[] =") != len(KEYS)
        or text.count("_byte_copy_safe =") != len(KEYS)
    ):
        raise EvidenceError(
            "signature header must declare exactly four contract types"
        )

    signatures = {}
    byte_copy_safe = {}
    for key in KEYS:
        declaration = f"inline constexpr const char {key}_layout[] ="
        if text.count(declaration) != 1:
            raise EvidenceError(
                f"signature header must declare {key} signature exactly once"
            )
        tail = text.split(declaration, 1)[1]
        assignment, separator, _ = tail.partition(";")
        if not separator:
            raise EvidenceError(f"signature header {key} signature lacks terminator")
        fragments = []
        for line in assignment.splitlines():
            literal = line.strip()
            if not literal:
                continue
            try:
                fragment = json.loads(literal)
            except json.JSONDecodeError as error:
                raise EvidenceError(
                    f"signature header {key} contains an invalid string literal"
                ) from error
            if not isinstance(fragment, str):
                raise EvidenceError(f"signature header {key} literal is not a string")
            fragments.append(fragment)
        signature = "".join(fragments)
        if not signature:
            raise EvidenceError(f"signature header {key} signature is empty")
        signatures[key] = signature

        true_line = f"inline constexpr bool {key}_byte_copy_safe = true;"
        false_line = f"inline constexpr bool {key}_byte_copy_safe = false;"
        true_count = text.count(true_line)
        false_count = text.count(false_line)
        if true_count + false_count != 1:
            raise EvidenceError(
                f"signature header must declare {key} byte-copy flag exactly once"
            )
        byte_copy_safe[key] = true_count == 1
    return signatures, byte_copy_safe


def validate_producer_artifacts(node, directory):
    node = validate_node(node)
    directory = Path(directory)
    if not directory.is_dir():
        raise EvidenceError("producer artifact directory is unavailable")

    anchor = directory / f"{node}.provenance.json"
    facts_path = _require_bundle_artifact(
        directory / f"{node}.producer-facts.json",
        anchor,
        f"{node}.producer-facts.json",
        "producer facts",
    )
    signature_path = _require_bundle_artifact(
        directory / f"{node}.sig.hpp",
        anchor,
        f"{node}.sig.hpp",
        "producer signature",
    )
    region_path = _require_bundle_artifact(
        directory / f"{node}.region",
        anchor,
        f"{node}.region",
        "producer region",
    )
    facts = _validate_facts(facts_path)
    if facts["node"] != node:
        raise EvidenceError("producer facts node does not match requested node")
    if not all(facts["admission"][key] for key in KEYS):
        raise EvidenceError("producer integration bundle is not READY")

    if region_path.stat().st_size == 0:
        raise EvidenceError("producer region artifact is empty")

    signatures, byte_copy_safe = _read_generated_signature_header(
        signature_path, node
    )
    for key in KEYS:
        if signatures[key] != facts["signatures"][key]:
            raise EvidenceError(f"producer {key} signature disagrees with facts")
        if byte_copy_safe[key] != facts["admission"][key]:
            raise EvidenceError(f"producer {key} Admission disagrees with signature")
    return facts


def _run_evidence_program(program, arguments, description):
    program = Path(program)
    try:
        resolved_program = program.resolve(strict=True)
    except OSError as error:
        raise EvidenceError(
            f"{description} executable is unavailable: {error}"
        ) from error
    if not resolved_program.is_file():
        raise EvidenceError(f"{description} executable is not a regular file")
    try:
        return subprocess.run(
            [str(resolved_program), *(str(argument) for argument in arguments)],
            capture_output=True,
            text=True,
            check=False,
        )
    except OSError as error:
        raise EvidenceError(f"cannot run {description}: {error}") from error


def verify_producer_bundle(node, directory, producer, exporter):
    node = validate_node(node)
    directory = Path(directory)
    directory.mkdir(parents=True, exist_ok=True)
    for suffix in (".producer-facts.json", ".region", ".sig.hpp"):
        artifact = directory / f"{node}{suffix}"
        if artifact.exists():
            if not artifact.is_file():
                raise EvidenceError(
                    f"producer output is not a regular file: {artifact}"
                )
            artifact.unlink()

    produced = _run_evidence_program(
        producer, (node, directory), "relocatable-world producer"
    )
    expected_stdout = (
        f"PRODUCER READY node={node} admission=4/4 region={node}.region"
    )
    if produced.returncode != 0 or produced.stdout.strip() != expected_stdout:
        raise EvidenceError(
            "producer did not emit its READY contract: "
            f"exit={produced.returncode}, stdout={produced.stdout!r}, "
            f"stderr={produced.stderr!r}"
        )

    exported = _run_evidence_program(
        exporter, (directory, node), "relocatable-world exporter"
    )
    if exported.returncode != 0:
        raise EvidenceError(
            "exporter failed: "
            f"exit={exported.returncode}, stdout={exported.stdout!r}, "
            f"stderr={exported.stderr!r}"
        )
    facts = validate_producer_artifacts(node, directory)

    invalid_node = "invalid_matrix_node"
    rejected_producer = _run_evidence_program(
        producer, (invalid_node, directory), "relocatable-world producer"
    )
    rejected_exporter = _run_evidence_program(
        exporter, (directory, invalid_node), "relocatable-world exporter"
    )
    if rejected_producer.returncode == 0 or rejected_exporter.returncode == 0:
        raise EvidenceError("producer and exporter must reject an invalid node")
    return facts


def _require_bundle_artifact(path, output, expected_name, where):
    path = Path(path)
    output = Path(output)
    if path.name != expected_name:
        raise EvidenceError(f"{where} filename must be exactly {expected_name!r}")
    try:
        if path.resolve(strict=True).parent != output.parent.resolve(strict=True):
            raise EvidenceError(f"{where} must be in the provenance bundle directory")
    except OSError as error:
        raise EvidenceError(f"{where} is unavailable: {error}") from error
    if not path.is_file():
        raise EvidenceError(f"{where} is not a regular file")
    return path


def seal_producer(
    *,
    node,
    profile,
    execution,
    probe,
    facts,
    sources_lock,
    outputs_lock,
    runner,
    source_sha,
    workflow_run,
    toolchain_artifact_sha256,
    output,
    signature=None,
    region=None,
):
    node = validate_node(node)
    profile = validate_profile(profile)
    if node not in profile_nodes(profile):
        raise EvidenceError(f"node {node} is not part of profile {profile}")
    if execution not in ("native", "emulated"):
        raise EvidenceError("execution must be native or emulated")
    _expect_nonempty_string(runner, "runner")
    source_sha = _expect_source_sha(source_sha, "source_sha")
    _expect_workflow_run(workflow_run, profile, source_sha, "workflow_run")
    toolchain_artifact_sha256 = _expect_sha256(
        toolchain_artifact_sha256, "toolchain_artifact_sha256"
    )
    output = Path(output)
    if output.name != f"{node}.provenance.json":
        raise EvidenceError(
            f"provenance filename must be exactly {node}.provenance.json"
        )

    probe_record = validate_probe(probe)
    facts_record = _validate_facts(facts)
    if probe_record["node"] != node or facts_record["node"] != node:
        raise EvidenceError("probe and producer facts node must match --node")
    if probe_record["admission"] != facts_record["admission"]:
        raise EvidenceError("probe and producer facts Admission decisions differ")
    _require_platform_probe_pass(probe_record)
    if probe_record["environment"]["runner"] != runner:
        raise EvidenceError("probe runner does not match --runner")

    policy, sources_digest, outputs_digest = (
        _load_lock_policy(sources_lock, outputs_lock, node)
    )
    if (
        toolchain_artifact_sha256
        != policy["toolchain_artifact_sha256"]
    ):
        raise EvidenceError(
            "toolchain artifact SHA256 does not match output lock"
        )
    compiler = probe_record["compiler"]
    comparisons = (
        ("family", policy["compiler_family"]),
        ("revision", policy["compiler_revision"]),
        ("version", policy["compiler_version"]),
        ("target", policy["target"]),
        ("stdlib", policy["stdlib"]),
    )
    for key, expected in comparisons:
        if compiler[key] != expected:
            raise EvidenceError(
                f"compiler {key} does not match toolchain locks: "
                f"expected {expected!r}, got {compiler[key]!r}"
            )

    environment = probe_record["environment"]
    if profile == "authoritative":
        if execution != "native":
            raise EvidenceError("authoritative producers must execute natively")
        if runner != _AUTHORITATIVE_RUNNERS[node]:
            raise EvidenceError("authoritative runner does not match its node")
    else:
        if execution != _LOCAL_EXECUTION[node]:
            raise EvidenceError("local execution mode does not match its node")

    if not _is_macos(node):
        for key in APPLE_IDENTITY_KEYS:
            if compiler[key] != "none":
                raise EvidenceError(f"Linux compiler {key} must be literal 'none'")
        if not compiler["sdk_locked"]:
            raise EvidenceError("Linux compiler sdk_locked must be true")
    elif profile == "authoritative":
        for key in APPLE_IDENTITY_KEYS:
            if compiler[key] != policy[key]:
                raise EvidenceError(f"authoritative macOS {key} does not match output lock")
        if not compiler["sdk_locked"]:
            raise EvidenceError("authoritative macOS sdk_locked must be true")
    else:
        actual_match = all(
            compiler[key] == policy[key]
            for key in APPLE_IDENTITY_KEYS
        )
        if compiler["sdk_locked"] != actual_match:
            raise EvidenceError("local macOS sdk_locked is not truthful")

    all_admitted = all(facts_record["admission"][key] for key in KEYS)
    artifacts = {}
    if all_admitted:
        if signature is None or region is None:
            raise EvidenceError("READY producer requires signature and region artifacts")
        signature_path = _require_bundle_artifact(
            signature, output, f"{node}.sig.hpp", "signature artifact"
        )
        region_path = _require_bundle_artifact(
            region, output, f"{node}.region", "region artifact"
        )
        header_signatures, header_admission = _read_generated_signature_header(
            signature_path, node
        )
        for key in KEYS:
            if header_signatures[key] != facts_record["signatures"][key]:
                raise EvidenceError(f"{key} signature disagrees with generated header")
            if header_admission[key] != facts_record["admission"][key]:
                raise EvidenceError(f"{key} Admission disagrees with generated header")
        artifacts = {
            "signature": {
                "filename": signature_path.name,
                "sha256": _sha256(signature_path),
            },
            "region": {
                "filename": region_path.name,
                "sha256": _sha256(region_path),
            },
        }
        status = "READY"
    else:
        if signature is not None or region is not None:
            raise EvidenceError("REJECT producer must not supply payload artifacts")
        status = "REJECT"

    provenance = {
        "schema": 1,
        "node": node,
        "status": status,
        "probe": {key: probe_record["probe"][key] for key in PROBE_KEYS},
        "admission": {key: facts_record["admission"][key] for key in KEYS},
        "signatures": {key: facts_record["signatures"][key] for key in KEYS},
        "compiler": {key: compiler[key] for key in COMPILER_KEYS},
        "build": {
            "profile": profile,
            "execution": execution,
            "runner": runner,
            "runner_image": environment["runner_image"],
            "source_sha": source_sha,
            "flags": policy["flags"],
            "workflow_run": workflow_run,
            "toolchain_artifact_sha256": toolchain_artifact_sha256,
        },
        "locks": {
            "sources_sha256": sources_digest,
            "outputs_sha256": outputs_digest,
        },
        "artifacts": artifacts,
    }
    _write_json(output, provenance)
    return validate_provenance(output)


def write_fallback_provenance(node, reason, output):
    validate_node(node)
    _expect_nonempty_string(reason, "reason")
    _validate_provenance_filename(output, node)
    record = {
        "schema": 1,
        "node": node,
        "status": "INCOMPLETE",
        "error": reason,
    }
    _write_json(output, record)
    return record


def write_fallback_results(profile, consumer, reason, output):
    nodes = profile_nodes(profile)
    if consumer not in nodes:
        raise EvidenceError(f"consumer {consumer!r} is not in profile {profile}")
    _expect_nonempty_string(reason, "reason")
    transfers = []
    for producer in nodes:
        if producer == consumer:
            continue
        transfers.append(
            {
                "producer": producer,
                "status": "INCOMPLETE",
                "reason": reason,
                "producer_provenance_sha256": None,
                "region_sha256": None,
            }
        )
    record = {
        "schema": 1,
        "profile": profile,
        "consumer": consumer,
        "consumer_provenance_sha256": None,
        "build": None,
        "transfers": transfers,
    }
    _write_json(output, record)
    return record


def _profile_pairs(profile):
    nodes = profile_nodes(profile)
    pairs = []
    for left_index in range(len(nodes)):
        for right_index in range(left_index + 1, len(nodes)):
            pairs.append((nodes[left_index], nodes[right_index]))
    return pairs


def _profile_transfers(profile):
    nodes = profile_nodes(profile)
    transfers = []
    for consumer in nodes:
        for producer in nodes:
            if consumer != producer:
                transfers.append((consumer, producer))
    return transfers


def write_fallback_agreements(profile, reason, output):
    nodes = profile_nodes(profile)
    _expect_nonempty_string(reason, "reason")
    pairs = []
    for left, right in _profile_pairs(profile):
        decisions = []
        for key in KEYS:
            decisions.append(
                {"key": key, "status": "INCOMPLETE", "reason": reason}
            )
        pairs.append({"left": left, "right": right, "decisions": decisions})
    record = {
        "schema": 1,
        "profile": profile,
        "producer_provenance_sha256": {node: None for node in nodes},
        "pairs": pairs,
    }
    _write_json(output, record)
    return record


def write_fallback_closure(profile, reason, output):
    nodes = profile_nodes(profile)
    _expect_nonempty_string(reason, "reason")
    pair_identities = [
        {"left": left, "right": right} for left, right in _profile_pairs(profile)
    ]
    transfer_identities = [
        {"consumer": consumer, "producer": producer}
        for consumer, producer in _profile_transfers(profile)
    ]
    expected = {
        "nodes": list(nodes),
        "pairs": pair_identities,
        "keys": list(KEYS),
        "consumers": list(nodes),
        "transfers": transfer_identities,
    }
    record = {
        "schema": 1,
        "profile": profile,
        "authoritative": False,
        "run": None,
        "agreements_sha256": None,
        "expected": expected,
        "counts": {
            "nodes": 0,
            "pairs": 0,
            "named_decisions": 0,
            "named_permits": 0,
            "consumers": 0,
            "transfers": 0,
            "passes": 0,
        },
        "missing": expected,
        "duplicates": {
            "nodes": [],
            "pairs": [],
            "keys": [],
            "consumers": [],
            "transfers": [],
        },
        "status": "INCOMPLETE",
        "error": reason,
    }
    _write_json(output, record)
    return record


def _add_profile_argument(parser):
    parser.add_argument("--profile", required=True, choices=PROFILES)


def _build_parser():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)

    validate = commands.add_parser("validate-provenance")
    validate.add_argument("file", type=Path)

    verify_producer = commands.add_parser("verify-producer-bundle")
    verify_producer.add_argument("--node", required=True)
    verify_producer.add_argument("--directory", required=True, type=Path)
    verify_producer.add_argument("--producer", required=True, type=Path)
    verify_producer.add_argument("--exporter", required=True, type=Path)

    fallback_provenance = commands.add_parser("fallback-provenance")
    fallback_provenance.add_argument("--node", required=True)
    fallback_provenance.add_argument("--reason", required=True)
    fallback_provenance.add_argument("--output", required=True, type=Path)

    fallback_results = commands.add_parser("fallback-results")
    _add_profile_argument(fallback_results)
    fallback_results.add_argument("--consumer", required=True)
    fallback_results.add_argument("--reason", required=True)
    fallback_results.add_argument("--output", required=True, type=Path)

    fallback_agreements = commands.add_parser("fallback-agreements")
    _add_profile_argument(fallback_agreements)
    fallback_agreements.add_argument("--reason", required=True)
    fallback_agreements.add_argument("--output", required=True, type=Path)

    fallback_closure = commands.add_parser("fallback-closure")
    _add_profile_argument(fallback_closure)
    fallback_closure.add_argument("--reason", required=True)
    fallback_closure.add_argument("--output", required=True, type=Path)

    seal = commands.add_parser("seal-producer")
    seal.add_argument("--node", required=True)
    _add_profile_argument(seal)
    seal.add_argument("--execution", required=True, choices=("native", "emulated"))
    seal.add_argument("--probe", required=True, type=Path)
    seal.add_argument("--facts", required=True, type=Path)
    seal.add_argument("--signature", type=Path)
    seal.add_argument("--region", type=Path)
    seal.add_argument("--sources-lock", required=True, type=Path)
    seal.add_argument("--outputs-lock", required=True, type=Path)
    seal.add_argument("--runner", required=True)
    seal.add_argument("--source-sha", required=True)
    seal.add_argument("--workflow-run", required=True)
    seal.add_argument("--toolchain-artifact-sha256", required=True)
    seal.add_argument("--output", required=True, type=Path)
    return parser


def main(arguments=None):
    parser = _build_parser()
    args = parser.parse_args(arguments)
    try:
        if args.command == "validate-provenance":
            record = validate_provenance(args.file)
            print(
                f"PROVENANCE PASS node={record['node']} status={record['status']}"
            )
        elif args.command == "verify-producer-bundle":
            record = verify_producer_bundle(
                args.node,
                args.directory,
                args.producer,
                args.exporter,
            )
            print(f"PRODUCER BUNDLE PASS node={record['node']}")
        elif args.command == "fallback-provenance":
            write_fallback_provenance(args.node, args.reason, args.output)
        elif args.command == "fallback-results":
            write_fallback_results(
                args.profile, args.consumer, args.reason, args.output
            )
        elif args.command == "fallback-agreements":
            write_fallback_agreements(args.profile, args.reason, args.output)
        elif args.command == "fallback-closure":
            write_fallback_closure(args.profile, args.reason, args.output)
        elif args.command == "seal-producer":
            seal_producer(
                node=args.node,
                profile=args.profile,
                execution=args.execution,
                probe=args.probe,
                facts=args.facts,
                signature=args.signature,
                region=args.region,
                sources_lock=args.sources_lock,
                outputs_lock=args.outputs_lock,
                runner=args.runner,
                source_sha=args.source_sha,
                workflow_run=args.workflow_run,
                toolchain_artifact_sha256=args.toolchain_artifact_sha256,
                output=args.output,
            )
    except EvidenceError as error:
        parser.exit(2, f"evidence error: {error}\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
