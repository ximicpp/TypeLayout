#!/usr/bin/env python3
"""Strict evidence boundary for the relocatable-world native matrix."""

import argparse
import hashlib
import json
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
    "xcode",
    "sdk",
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
)

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


def load_json(path):
    path = Path(path)
    try:
        value = json.loads(
            path.read_text(encoding="utf-8"),
            object_pairs_hook=_duplicate_object,
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
    temporary.write_text(
        json.dumps(value, indent=2, sort_keys=False) + "\n",
        encoding="utf-8",
        newline="\n",
    )
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
        for key in ("xcode", "sdk", "deployment_target"):
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
    for key in ("runner", "runner_image", "flags", "workflow_run"):
        _expect_nonempty_string(value[key], f"build.{key}")
    _expect_source_sha(value["source_sha"], "build.source_sha")
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


def _required_mapping(record, key, where):
    mapping = _expect_object(record.get(key), f"{where}.{key}")
    return mapping


def _load_lock_policy(sources_lock, outputs_lock, node):
    sources_lock = Path(sources_lock)
    outputs_lock = Path(outputs_lock)
    sources = load_json(sources_lock)
    outputs = load_json(outputs_lock)
    if sources.get("schema") != 1 or type(sources.get("schema")) is not int:
        raise EvidenceError("source lock schema must be integer 1")
    if outputs.get("schema") != 1 or type(outputs.get("schema")) is not int:
        raise EvidenceError("output lock schema must be integer 1")
    sources_digest = _sha256(sources_lock)
    if outputs.get("sources_sha256") != sources_digest:
        raise EvidenceError("output lock sources_sha256 does not bind the source lock")
    source_nodes = _required_mapping(sources, "nodes", "source lock")
    output_nodes = _required_mapping(outputs, "nodes", "output lock")
    if node not in source_nodes or node not in output_nodes:
        raise EvidenceError(f"toolchain locks do not define node {node}")
    source_policy = _expect_object(source_nodes[node], f"source lock node {node}")
    output_policy = _expect_object(output_nodes[node], f"output lock node {node}")
    for key in ("compiler_family", "compiler_revision", "flags"):
        _expect_nonempty_string(
            source_policy.get(key), f"source lock node {node}.{key}"
        )
    for key in (
        "compiler_version",
        "target",
        "stdlib",
        "runner_image",
        "xcode",
        "sdk",
        "deployment_target",
    ):
        _expect_nonempty_string(
            output_policy.get(key), f"output lock node {node}.{key}"
        )
    return source_policy, output_policy, sources_digest, _sha256(outputs_lock)


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
    _expect_source_sha(source_sha, "source_sha")
    _expect_nonempty_string(workflow_run, "workflow_run")
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

    source_policy, output_policy, sources_digest, outputs_digest = (
        _load_lock_policy(sources_lock, outputs_lock, node)
    )
    compiler = probe_record["compiler"]
    comparisons = (
        ("family", source_policy["compiler_family"]),
        ("revision", source_policy["compiler_revision"]),
        ("version", output_policy["compiler_version"]),
        ("target", output_policy["target"]),
        ("stdlib", output_policy["stdlib"]),
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
        if environment["runner_image"] != output_policy["runner_image"]:
            raise EvidenceError("authoritative runner_image does not match output lock")
    else:
        if execution != _LOCAL_EXECUTION[node]:
            raise EvidenceError("local execution mode does not match its node")
        if _is_macos(node) and environment["runner_image"] != "personal-macos":
            raise EvidenceError("local macOS runner_image must be personal-macos")
        if not _is_macos(node) and environment["runner_image"] != output_policy["runner_image"]:
            raise EvidenceError("local Linux runner_image does not match output lock")

    apple_keys = ("xcode", "sdk", "deployment_target")
    if not _is_macos(node):
        for key in apple_keys:
            if compiler[key] != "none":
                raise EvidenceError(f"Linux compiler {key} must be literal 'none'")
        if not compiler["sdk_locked"]:
            raise EvidenceError("Linux compiler sdk_locked must be true")
    elif profile == "authoritative":
        for key in apple_keys:
            if compiler[key] != output_policy[key]:
                raise EvidenceError(f"authoritative macOS {key} does not match output lock")
        if not compiler["sdk_locked"]:
            raise EvidenceError("authoritative macOS sdk_locked must be true")
    else:
        actual_match = all(compiler[key] == output_policy[key] for key in apple_keys)
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
            "flags": source_policy["flags"],
            "workflow_run": workflow_run,
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
                output=args.output,
            )
    except EvidenceError as error:
        parser.exit(2, f"evidence error: {error}\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
