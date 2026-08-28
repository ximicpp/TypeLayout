#!/usr/bin/env python3
"""Strict evidence boundary for the relocatable-world native matrix."""

import argparse
import hashlib
import json
import re
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

LINUX_PLATFORMS = ("linux/amd64", "linux/arm64")
MACOS_NODES = ("arm64_macos_clang", "x86_64_macos_clang")
P2996_CMAKE_FLAGS = (
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
)
P2996_PLATFORM_CMAKE_FLAGS = {
    "linux/amd64": (
        "-DCMAKE_INSTALL_PREFIX=/opt/p2996-toolchain",
        "-DLLVM_TARGETS_TO_BUILD=X86",
        "-DLLVM_ENABLE_PER_TARGET_RUNTIME_DIR=ON",
    ),
    "linux/arm64": (
        "-DCMAKE_INSTALL_PREFIX=/opt/p2996-toolchain",
        "-DLLVM_TARGETS_TO_BUILD=AArch64",
        "-DLLVM_ENABLE_PER_TARGET_RUNTIME_DIR=ON",
    ),
    "macos/arm64": (
        "-DCMAKE_INSTALL_PREFIX=${TOOLCHAIN_ROOT}",
        "-DCMAKE_OSX_ARCHITECTURES=arm64",
        "-DCMAKE_OSX_SYSROOT=${SDKROOT}",
        "-DCMAKE_OSX_DEPLOYMENT_TARGET=15.0",
        "-DLLVM_TARGETS_TO_BUILD=AArch64",
        "-DLLVM_ENABLE_PER_TARGET_RUNTIME_DIR=OFF",
    ),
    "macos/x86_64": (
        "-DCMAKE_INSTALL_PREFIX=${TOOLCHAIN_ROOT}",
        "-DCMAKE_OSX_ARCHITECTURES=x86_64",
        "-DCMAKE_OSX_SYSROOT=${SDKROOT}",
        "-DCMAKE_OSX_DEPLOYMENT_TARGET=15.0",
        "-DLLVM_TARGETS_TO_BUILD=X86",
        "-DLLVM_ENABLE_PER_TARGET_RUNTIME_DIR=OFF",
    ),
}
LINUX_POLICY_NODES = {
    ("gcc", "linux/amd64"): "x86_64_linux_gcc",
    ("p2996", "linux/amd64"): "x86_64_linux_clang",
    ("gcc", "linux/arm64"): "arm64_linux_gcc",
    ("p2996", "linux/arm64"): "arm64_linux_clang",
}

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

AGREEMENT_STATUSES = ("PERMIT", "REJECT", "INCOMPLETE")
RUN_IDENTITY_KEYS = (
    "source_sha",
    "workflow_run",
    "sources_sha256",
    "outputs_sha256",
)
RESULT_BUILD_KEYS = RUN_IDENTITY_KEYS + (
    "execution",
    "runner",
    "runner_image",
    "toolchain_artifact_sha256",
    "compiler_family",
    "compiler_revision",
    "compiler_version",
    "target",
    "stdlib",
    "flags",
    "xcode_version",
    "xcode_build",
    "sdk_version",
    "sdk_build",
    "deployment_target",
    "sdk_locked",
)
CLOSURE_IDENTITY_KEYS = (
    "nodes",
    "pairs",
    "named_decisions",
    "consumers",
    "transfers",
)
CLOSURE_COUNT_KEYS = (
    "nodes",
    "pairs",
    "named_decisions",
    "named_permits",
    "consumers",
    "transfers",
    "passes",
)

PROFILES = ("authoritative", "local-arm64-macos")
LOCAL_WORKFLOW_RUN_MAX_LENGTH = 128
LOCAL_WORKFLOW_RUN_ALPHANUMERIC = (
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"
)
LOCAL_WORKFLOW_RUN_CHARACTERS = (
    LOCAL_WORKFLOW_RUN_ALPHANUMERIC + "_.-"
)
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


def _expect_sha512(value, where):
    if not _is_lower_hex(value, 128):
        raise EvidenceError(f"{where} must be a 128-character lowercase SHA512")
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


def _is_authoritative_workflow_run(value):
    if not isinstance(value, str):
        return False
    parts = value.split(".")
    return len(parts) == 2 and all(
        _is_canonical_positive_decimal(part) for part in parts
    )


def _is_numeric_workflow_run_pair(value):
    if not isinstance(value, str):
        return False
    parts = value.split(".")
    return len(parts) == 2 and all(
        part and all(character in "0123456789" for character in part)
        for part in parts
    )


def _expect_workflow_run(value, profile, source_sha, where):
    _expect_nonempty_string(value, where)
    validate_profile(profile)
    _expect_source_sha(source_sha, f"{where} source SHA")
    if profile == "authoritative":
        if not _is_authoritative_workflow_run(value):
            raise EvidenceError(
                f"{where} must be canonical positive run_id.run_attempt"
            )
        return value

    if _is_numeric_workflow_run_pair(value):
        raise EvidenceError(
            f"{where} must not use authoritative run_id.run_attempt form"
        )
    if (
        len(value) > LOCAL_WORKFLOW_RUN_MAX_LENGTH
        or value[0] not in LOCAL_WORKFLOW_RUN_ALPHANUMERIC
        or value[-1] not in LOCAL_WORKFLOW_RUN_ALPHANUMERIC
        or ".." in value
        or not all(
            character in LOCAL_WORKFLOW_RUN_CHARACTERS for character in value
        )
    ):
        raise EvidenceError(
            f"{where} must be 1-{LOCAL_WORKFLOW_RUN_MAX_LENGTH} safe ASCII "
            "filename characters, beginning and ending with an alphanumeric"
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


def _expect_digest_qualified_image(value, where):
    value = _expect_nonempty_string(value, where)
    repository, separator, digest = value.rpartition("@")
    if not repository or separator != "@":
        raise EvidenceError(f"{where} must be an image pinned by digest")
    _expect_digest_reference(digest, where)
    return value


def _expect_string_array(value, where, *, nonempty=True):
    if not isinstance(value, list):
        raise EvidenceError(f"{where} must be a JSON array")
    if nonempty and not value:
        raise EvidenceError(f"{where} must not be empty")
    for index, entry in enumerate(value):
        _expect_nonempty_string(entry, f"{where}[{index}]")
    if len(set(value)) != len(value):
        raise EvidenceError(f"{where} entries must be unique")
    return value


def _expect_https_url(value, where):
    value = _expect_nonempty_string(value, where)
    if not value.startswith("https://") or any(
        ord(character) < 33 or ord(character) > 126 for character in value
    ):
        raise EvidenceError(f"{where} must be a printable HTTPS URL")
    return value


def _expect_plain_filename(value, where):
    value = _expect_nonempty_string(value, where)
    path = Path(value)
    if (
        path.is_absolute()
        or value in (".", "..")
        or "/" in value
        or "\\" in value
        or path.name != value
    ):
        raise EvidenceError(f"{where} must be a plain filename")
    return value


def _expect_locked_packages(value, where):
    packages = _expect_string_array(value, where)
    for index, package in enumerate(packages):
        name, separator, version = package.partition("=")
        if not name or separator != "=" or not version:
            raise EvidenceError(
                f"{where}[{index}] must pin one package as name=version"
            )
    return packages


def _expect_immutable_archive_url(value, sources_digest, revision, where):
    value = _expect_https_url(value, where)
    source_tag = f"/typelayout-toolchains-{sources_digest}/"
    if source_tag not in value or revision not in value:
        raise EvidenceError(
            f"{where} must bind the source digest and compiler revision"
        )
    return value


def load_node_toolchain_policy(sources_lock, outputs_lock, node):
    """Validate the complete lock pair and return one normalized node policy."""
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
    locked_sources_digest = _expect_sha256(
        outputs["sources_sha256"], "output lock.sources_sha256"
    )
    if locked_sources_digest != sources_digest:
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

    gcc_source = _expect_exact_keys(
        sources["gcc"],
        (
            "version",
            "compiler_family",
            "compiler_revision",
            "flags",
            "source",
            "prerequisites",
            "configure_flags",
        ),
        "source lock.gcc",
    )
    clang_source = _expect_exact_keys(
        sources["p2996"],
        (
            "repository",
            "commit",
            "compiler_family",
            "compiler_revision",
            "flags",
            "projects",
            "runtimes",
            "llvm_targets",
            "cmake_flags",
            "platform_cmake_flags",
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
    _expect_https_url(
        clang_source["repository"], "source lock.p2996.repository"
    )
    if gcc_source["compiler_family"] != "gcc":
        raise EvidenceError("source lock.gcc compiler_family must be gcc")
    if clang_source["compiler_family"] != "clang":
        raise EvidenceError("source lock.p2996 compiler_family must be clang")
    _expect_source_sha(clang_source["commit"], "source lock.p2996.commit")
    _expect_source_sha(
        clang_source["compiler_revision"],
        "source lock.p2996.compiler_revision",
    )
    if clang_source["commit"] != clang_source["compiler_revision"]:
        raise EvidenceError("source lock.p2996 commit and compiler_revision differ")

    gcc_download = _expect_exact_keys(
        gcc_source["source"],
        ("url", "filename", "sha512"),
        "source lock.gcc.source",
    )
    _expect_https_url(gcc_download["url"], "source lock.gcc.source.url")
    _expect_plain_filename(
        gcc_download["filename"], "source lock.gcc.source.filename"
    )
    _expect_sha512(gcc_download["sha512"], "source lock.gcc.source.sha512")
    prerequisites = _expect_exact_keys(
        gcc_source["prerequisites"],
        ("gmp", "mpfr", "mpc", "isl"),
        "source lock.gcc.prerequisites",
    )
    for prerequisite in ("gmp", "mpfr", "mpc", "isl"):
        where = f"source lock.gcc.prerequisites.{prerequisite}"
        record = _expect_exact_keys(
            prerequisites[prerequisite],
            ("version", "url", "filename", "sha512"),
            where,
        )
        _expect_nonempty_string(record["version"], f"{where}.version")
        _expect_https_url(record["url"], f"{where}.url")
        _expect_plain_filename(record["filename"], f"{where}.filename")
        _expect_sha512(record["sha512"], f"{where}.sha512")
    configure_flags = _expect_string_array(
        gcc_source["configure_flags"], "source lock.gcc.configure_flags"
    )
    if "--disable-nls" not in configure_flags:
        raise EvidenceError("source lock.gcc.configure_flags must disable NLS")
    if any("download_prerequisites" in flag for flag in configure_flags):
        raise EvidenceError(
            "source lock.gcc.configure_flags must not download prerequisites"
        )

    projects = _expect_string_array(
        clang_source["projects"], "source lock.p2996.projects"
    )
    if projects != ["clang"]:
        raise EvidenceError("source lock.p2996.projects must be exactly clang")
    runtimes = _expect_string_array(
        clang_source["runtimes"], "source lock.p2996.runtimes"
    )
    if runtimes != ["libcxx", "libcxxabi", "libunwind"]:
        raise EvidenceError(
            "source lock.p2996.runtimes must be libcxx, libcxxabi, libunwind"
        )
    llvm_targets = _expect_string_array(
        clang_source["llvm_targets"], "source lock.p2996.llvm_targets"
    )
    if llvm_targets != ["X86", "AArch64"]:
        raise EvidenceError(
            "source lock.p2996.llvm_targets must be X86 and AArch64"
        )
    cmake_flags = _expect_string_array(
        clang_source["cmake_flags"], "source lock.p2996.cmake_flags"
    )
    if cmake_flags != list(P2996_CMAKE_FLAGS):
        raise EvidenceError("source lock.p2996.cmake_flags are not canonical")
    platform_cmake_flags = _expect_exact_keys(
        clang_source["platform_cmake_flags"],
        P2996_PLATFORM_CMAKE_FLAGS,
        "source lock.p2996.platform_cmake_flags",
    )
    for platform, expected_flags in P2996_PLATFORM_CMAKE_FLAGS.items():
        actual_flags = _expect_string_array(
            platform_cmake_flags[platform],
            f"source lock.p2996.platform_cmake_flags.{platform}",
        )
        if actual_flags != list(expected_flags):
            raise EvidenceError(
                "source lock.p2996.platform_cmake_flags."
                f"{platform} are not canonical"
            )

    linux_source = _expect_exact_keys(
        sources["linux"],
        ("platforms", "base_images", "apt", "packages", "docker"),
        "source lock.linux",
    )
    source_platforms = _expect_exact_keys(
        linux_source["platforms"],
        LINUX_PLATFORMS,
        "source lock.linux.platforms",
    )
    expected_linux_platforms = {
        "linux/amd64": ("x86_64", "ubuntu-24.04"),
        "linux/arm64": ("arm64", "ubuntu-24.04-arm"),
    }
    for platform, (architecture, runner) in expected_linux_platforms.items():
        platform_source = _expect_exact_keys(
            source_platforms[platform],
            ("architecture", "runner"),
            f"source lock.linux.platforms.{platform}",
        )
        _expect_nonempty_string(
            platform_source["architecture"],
            f"source lock.linux.platforms.{platform}.architecture",
        )
        _expect_nonempty_string(
            platform_source["runner"],
            f"source lock.linux.platforms.{platform}.runner",
        )
        if platform_source["architecture"] != architecture:
            raise EvidenceError(
                f"source lock platform {platform} architecture must be {architecture}"
            )
        if platform_source["runner"] != runner:
            raise EvidenceError(
                f"source lock platform {platform} runner must be {runner}"
            )

    base_images = _expect_exact_keys(
        linux_source["base_images"],
        ("gcc_builder", "gcc_runtime", "p2996_builder", "p2996_runtime"),
        "source lock.linux.base_images",
    )
    for image in base_images:
        _expect_digest_qualified_image(
            base_images[image], f"source lock.linux.base_images.{image}"
        )

    apt_source = _expect_exact_keys(
        linux_source["apt"],
        ("snapshot", "suites", "components"),
        "source lock.linux.apt",
    )
    _expect_nonempty_string(
        apt_source["snapshot"], "source lock.linux.apt.snapshot"
    )
    _expect_string_array(apt_source["suites"], "source lock.linux.apt.suites")
    _expect_string_array(
        apt_source["components"], "source lock.linux.apt.components"
    )

    packages = _expect_exact_keys(
        linux_source["packages"],
        ("gcc_builder", "gcc_runtime", "p2996_builder", "p2996_runtime"),
        "source lock.linux.packages",
    )
    for package_set in packages:
        _expect_locked_packages(
            packages[package_set],
            f"source lock.linux.packages.{package_set}",
        )

    docker_source = _expect_exact_keys(
        linux_source["docker"],
        (
            "runner_images_commit",
            "runners",
            "buildx_version",
            "buildkit_image",
            "dockerfile_frontend",
        ),
        "source lock.linux.docker",
    )
    _expect_source_sha(
        docker_source["runner_images_commit"],
        "source lock.linux.docker.runner_images_commit",
    )
    _expect_nonempty_string(
        docker_source["buildx_version"],
        "source lock.linux.docker.buildx_version",
    )
    _expect_digest_qualified_image(
        docker_source["buildkit_image"],
        "source lock.linux.docker.buildkit_image",
    )
    _expect_digest_qualified_image(
        docker_source["dockerfile_frontend"],
        "source lock.linux.docker.dockerfile_frontend",
    )
    docker_runners = _expect_exact_keys(
        docker_source["runners"],
        ("ubuntu-24.04", "ubuntu-24.04-arm"),
        "source lock.linux.docker.runners",
    )
    for runner in docker_runners:
        runner_record = _expect_exact_keys(
            docker_runners[runner],
            ("client_version", "server_version"),
            f"source lock.linux.docker.runners.{runner}",
        )
        for key in ("client_version", "server_version"):
            _expect_nonempty_string(
                runner_record[key],
                f"source lock.linux.docker.runners.{runner}.{key}",
            )

    source_macos = _expect_exact_keys(
        sources["macos"],
        ("runner_images_repository", "runner_images_commit", "nodes"),
        "source lock.macos",
    )
    _expect_https_url(
        source_macos["runner_images_repository"],
        "source lock.macos.runner_images_repository",
    )
    _expect_source_sha(
        source_macos["runner_images_commit"],
        "source lock.macos.runner_images_commit",
    )
    if (
        source_macos["runner_images_commit"]
        != docker_source["runner_images_commit"]
    ):
        raise EvidenceError("source lock runner-images commits must match")
    source_macos_nodes = _expect_exact_keys(
        source_macos["nodes"],
        MACOS_NODES,
        "source lock.macos.nodes",
    )
    expected_macos_nodes = {
        "arm64_macos_clang": ("macos-15", "arm64", "AArch64"),
        "x86_64_macos_clang": ("macos-15-intel", "x86_64", "X86"),
    }
    for macos_node, expected_values in expected_macos_nodes.items():
        source_node = _expect_exact_keys(
            source_macos_nodes[macos_node],
            ("runner", "architecture", "llvm_target", "flags")
            + APPLE_IDENTITY_KEYS,
            f"source lock.macos.nodes.{macos_node}",
        )
        for key in (
            "runner",
            "architecture",
            "llvm_target",
            "flags",
        ) + APPLE_IDENTITY_KEYS:
            _expect_nonempty_string(
                source_node[key],
                f"source lock.macos.nodes.{macos_node}.{key}",
            )
        actual_values = tuple(
            source_node[key] for key in ("runner", "architecture", "llvm_target")
        )
        if actual_values != expected_values:
            raise EvidenceError(
                f"source lock macOS identity differs for {macos_node}"
            )

    actions = _expect_exact_keys(
        sources["actions"],
        (
            "checkout",
            "upload_artifact",
            "download_artifact",
            "docker_login",
            "setup_buildx",
            "build_push",
            "github_release",
        ),
        "source lock.actions",
    )
    for action in actions:
        _expect_source_sha(actions[action], f"source lock.actions.{action}")

    recipes = _expect_exact_keys(
        sources["recipes"],
        (
            ".gitattributes",
            ".github/docker/Dockerfile.gcc16",
            ".github/docker/Dockerfile.p2996",
            ".github/docker/docker-bake.hcl",
            ".github/scripts/build-p2996-macos.sh",
            ".github/scripts/verify-p2996-toolchain.sh",
            ".github/workflows/toolchain-images.yml",
        ),
        "source lock.recipes",
    )
    for recipe in recipes:
        _expect_sha256(recipes[recipe], f"source lock.recipes.{recipe}")
    output_linux = _expect_exact_keys(
        outputs["linux"], ("gcc", "p2996"), "output lock.linux"
    )
    output_macos = _expect_exact_keys(
        outputs["macos"],
        MACOS_NODES,
        "output lock.macos",
    )

    artifact_digests = []
    source_compilers = {"gcc": gcc_source, "p2996": clang_source}
    for toolchain in ("gcc", "p2996"):
        output_toolchain = _expect_exact_keys(
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
        artifact_digests.append(
            _expect_digest_reference(
                output_toolchain["index_digest"],
                f"output lock.linux.{toolchain}.index_digest",
            )
        )
        if (
            output_toolchain["compiler_revision"]
            != source_compilers[toolchain]["compiler_revision"]
        ):
            raise EvidenceError(
                f"output lock Linux compiler revision differs for {toolchain}"
            )
        platform_outputs = _expect_exact_keys(
            output_toolchain["platforms"],
            LINUX_PLATFORMS,
            f"output lock.linux.{toolchain}.platforms",
        )
        for platform in LINUX_PLATFORMS:
            platform_output = _expect_exact_keys(
                platform_outputs[platform],
                ("manifest_digest", "target"),
                f"output lock.linux.{toolchain}.platforms.{platform}",
            )
            target = _expect_nonempty_string(
                platform_output["target"],
                f"output lock.linux.{toolchain}.platforms.{platform}.target",
            )
            target_prefix = (
                "x86_64-" if platform == "linux/amd64" else "aarch64-"
            )
            if toolchain == "gcc":
                if not target.startswith(target_prefix):
                    raise EvidenceError(
                        "output lock Linux GCC target does not match "
                        f"{platform}: {target!r}"
                    )
            else:
                expected_target = target_prefix + "unknown-linux-gnu"
                if target != expected_target:
                    raise EvidenceError(
                        "output lock Linux P2996 target does not match "
                        f"{platform}: {target!r}"
                    )
            artifact_digests.append(
                _expect_digest_reference(
                    platform_output["manifest_digest"],
                    f"output lock.linux.{toolchain}.platforms."
                    f"{platform}.manifest_digest",
                )
            )

        if toolchain == "gcc":
            if output_toolchain["compiler_version"] != "16.2.0":
                raise EvidenceError(
                    "output lock Linux GCC compiler_version must be exactly 16.2.0"
                )
            if re.fullmatch(r"libstdc\+\+-[0-9]+", output_toolchain["stdlib"]) is None:
                raise EvidenceError(
                    "output lock Linux GCC stdlib must match libstdc++-digits"
                )
        elif re.fullmatch(r"libc\+\+-[0-9]+", output_toolchain["stdlib"]) is None:
            raise EvidenceError(
                "output lock Linux P2996 stdlib must match libc++-digits"
            )

    for macos_node in MACOS_NODES:
        output_node = _expect_exact_keys(
            output_macos[macos_node],
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
            f"output lock.macos.{macos_node}",
        )
        for key in (
            "compiler_revision",
            "compiler_version",
            "target",
            "stdlib",
        ) + APPLE_IDENTITY_KEYS:
            _expect_nonempty_string(
                output_node[key], f"output lock.macos.{macos_node}.{key}"
            )
        source_node = source_macos_nodes[macos_node]
        expected_target = (
            f"{source_node['architecture']}-apple-macosx"
            f"{source_node['deployment_target']}.0"
        )
        if output_node["target"] != expected_target:
            raise EvidenceError(
                f"output lock macOS target differs for {macos_node}: "
                f"{output_node['target']!r}"
            )
        _expect_immutable_archive_url(
            output_node["url"],
            sources_digest,
            clang_source["compiler_revision"],
            f"output lock.macos.{macos_node}.url",
        )
        artifact_digests.append(
            _expect_sha256(
                output_node["archive_sha256"],
                f"output lock.macos.{macos_node}.archive_sha256",
            )
        )
        observed = _expect_exact_keys(
            output_node["observed_runner"],
            ("image_os", "image_version"),
            f"output lock.macos.{macos_node}.observed_runner",
        )
        for key in ("image_os", "image_version"):
            _expect_nonempty_string(
                observed[key],
                f"output lock.macos.{macos_node}.observed_runner.{key}",
            )
        if output_node["compiler_revision"] != clang_source["compiler_revision"]:
            raise EvidenceError(
                f"output lock macOS compiler revision differs for {macos_node}"
            )
        for key in APPLE_IDENTITY_KEYS:
            if output_node[key] != source_macos_nodes[macos_node][key]:
                raise EvidenceError(
                    f"output lock macOS {key} differs from source hard lock "
                    f"for {macos_node}"
                )

        if re.fullmatch(r"libc\+\+-[0-9]+", output_node["stdlib"]) is None:
            raise EvidenceError(
                f"output lock macOS P2996 stdlib must match libc++-digits for {macos_node}"
            )

    p2996_identities = {
        (
            output_linux["p2996"]["compiler_version"],
            output_linux["p2996"]["stdlib"],
        )
    }
    p2996_identities.update(
        (record["compiler_version"], record["stdlib"])
        for record in output_macos.values()
    )
    if len(p2996_identities) != 1:
        raise EvidenceError(
            "all Linux and macOS P2996 outputs must have identical "
            "compiler_version and stdlib identities"
        )

    if len(artifact_digests) != 8 or len(set(artifact_digests)) != 8:
        raise EvidenceError("all eight output lock artifact digests must be unique")

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
        assignment_lines = []
        terminated = False
        for line in tail.splitlines():
            literal = line.strip()
            if not literal:
                continue
            if literal.endswith(";"):
                assignment_lines.append(literal[:-1].rstrip())
                terminated = True
                break
            assignment_lines.append(literal)
        if not terminated:
            raise EvidenceError(f"signature header {key} signature lacks terminator")
        fragments = []
        for literal in assignment_lines:
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
        load_node_toolchain_policy(sources_lock, outputs_lock, node)
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
        "named_decisions": [
            {"left": left, "right": right, "key": key}
            for left, right in _profile_pairs(profile)
            for key in KEYS
        ],
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
            "named_decisions": [],
            "consumers": [],
            "transfers": [],
        },
        "status": "INCOMPLETE",
        "error": reason,
    }
    _write_json(output, record)
    return record


def _expect_array(value, where):
    if not isinstance(value, list):
        raise EvidenceError(f"{where} must be a JSON array")
    return value


def _expect_nullable_sha256(value, where):
    if value is None:
        return None
    return _expect_sha256(value, where)


def _validate_run_identity(value, profile, where="run"):
    value = _expect_exact_keys(value, RUN_IDENTITY_KEYS, where)
    source_sha = _expect_source_sha(value["source_sha"], f"{where}.source_sha")
    _expect_workflow_run(
        value["workflow_run"], profile, source_sha, f"{where}.workflow_run"
    )
    _expect_sha256(value["sources_sha256"], f"{where}.sources_sha256")
    _expect_sha256(value["outputs_sha256"], f"{where}.outputs_sha256")
    return value


def _validate_consumer_build(value, node, profile, where="build"):
    value = _expect_exact_keys(value, RESULT_BUILD_KEYS, where)
    _validate_run_identity(
        {key: value[key] for key in RUN_IDENTITY_KEYS}, profile, where
    )
    if value["execution"] not in ("native", "emulated"):
        raise EvidenceError(f"{where}.execution must be native or emulated")
    for key in RESULT_BUILD_KEYS[5:-1]:
        _expect_nonempty_string(value[key], f"{where}.{key}")
    _expect_sha256(
        value["toolchain_artifact_sha256"],
        f"{where}.toolchain_artifact_sha256",
    )
    _expect_boolean(value["sdk_locked"], f"{where}.sdk_locked")
    if value["compiler_family"] != _expected_family(node):
        raise EvidenceError(f"{where}.compiler_family does not match {node}")
    if profile == "authoritative":
        if value["execution"] != "native":
            raise EvidenceError(f"{where}.execution must be native")
        if value["runner"] != _AUTHORITATIVE_RUNNERS[node]:
            raise EvidenceError(f"{where}.runner does not match {node}")
        if not value["sdk_locked"]:
            raise EvidenceError(f"{where}.sdk_locked must be true")
    elif value["execution"] != _LOCAL_EXECUTION[node]:
        raise EvidenceError(f"{where}.execution does not match local {node}")
    if not _is_macos(node):
        for key in APPLE_IDENTITY_KEYS:
            if value[key] != "none":
                raise EvidenceError(f"Linux {where}.{key} must be literal 'none'")
        if not value["sdk_locked"]:
            raise EvidenceError(f"Linux {where}.sdk_locked must be true")
    return value


def validate_results(path):
    path = Path(path)
    record = load_json(path)
    _expect_exact_keys(
        record,
        (
            "schema",
            "profile",
            "consumer",
            "consumer_provenance_sha256",
            "build",
            "transfers",
        ),
        "results top-level",
    )
    if type(record["schema"]) is not int or record["schema"] != 1:
        raise EvidenceError("results schema must be integer 1")
    profile = validate_profile(record["profile"])
    consumer = validate_node(record["consumer"])
    nodes = profile_nodes(profile)
    if consumer not in nodes:
        raise EvidenceError(f"consumer {consumer} is not part of profile {profile}")
    if path.name != f"{consumer}.results.json":
        raise EvidenceError(
            f"results filename must be exactly {consumer}.results.json"
        )
    consumer_digest = _expect_nullable_sha256(
        record["consumer_provenance_sha256"],
        "consumer_provenance_sha256",
    )
    build = record["build"]
    if build is None:
        if consumer_digest is not None:
            raise EvidenceError(
                "fallback results cannot invent consumer provenance"
            )
    else:
        if consumer_digest is None:
            raise EvidenceError(
                "evaluated results require consumer provenance"
            )
        _validate_consumer_build(build, consumer, profile)

    transfers = _expect_array(record["transfers"], "transfers")
    expected_producers = [node for node in nodes if node != consumer]
    if len(transfers) != len(expected_producers):
        raise EvidenceError("results must preserve every non-self producer slot")
    for index, (transfer, expected_producer) in enumerate(
        zip(transfers, expected_producers)
    ):
        where = f"transfers[{index}]"
        transfer = _expect_exact_keys(
            transfer,
            (
                "producer",
                "status",
                "reason",
                "producer_provenance_sha256",
                "region_sha256",
            ),
            where,
        )
        if transfer["producer"] != expected_producer:
            raise EvidenceError(
                f"{where}.producer must be fixed profile-order {expected_producer}"
            )
        status = transfer["status"]
        if status not in TRANSFER_STATUSES:
            raise EvidenceError(f"{where}.status is unknown")
        _expect_nonempty_string(transfer["reason"], f"{where}.reason")
        provenance_digest = _expect_nullable_sha256(
            transfer["producer_provenance_sha256"],
            f"{where}.producer_provenance_sha256",
        )
        region_digest = _expect_nullable_sha256(
            transfer["region_sha256"], f"{where}.region_sha256"
        )
        if status in (
            "PASS",
            "REJECT_ENVELOPE",
            "REJECT_REGION",
            "REJECT_GRAPH",
        ) and (provenance_digest is None or region_digest is None):
            raise EvidenceError(f"{where}.{status} requires both digests")
        if status == "SKIPPED_TYPELAYOUT_REJECT" and provenance_digest is None:
            raise EvidenceError(
                f"{where}.SKIPPED_TYPELAYOUT_REJECT requires provenance"
            )
        if build is None and status != "INCOMPLETE":
            raise EvidenceError("fallback results must contain only INCOMPLETE")
    return record


def _expected_agreement(profile, producers):
    nodes = profile_nodes(profile)
    by_node = {slot["node"]: slot for slot in producers}
    pairs = []
    for left, right in _profile_pairs(profile):
        decisions = []
        for key_index, key in enumerate(KEYS):
            left_record = by_node[left]
            right_record = by_node[right]
            if (
                not left_record["present"]
                or not right_record["present"]
                or not left_record["signatures"][key_index]
                or not right_record["signatures"][key_index]
            ):
                status = "INCOMPLETE"
                reason = "producer evidence incomplete"
            elif (
                not left_record["admission"][key_index]
                or not right_record["admission"][key_index]
            ):
                status = "REJECT"
                reason = "Admission rejected"
            elif (
                left_record["signatures"][key_index]
                != right_record["signatures"][key_index]
            ):
                status = "REJECT"
                reason = "layout signature differs"
            else:
                status = "PERMIT"
                reason = "Admission and signature agree"
            decisions.append({"key": key, "status": status, "reason": reason})
        pairs.append({"left": left, "right": right, "decisions": decisions})
    return {
        "schema": 1,
        "profile": profile,
        "producer_provenance_sha256": {
            node: (
                by_node[node]["provenance_sha256"]
                if by_node[node]["present"]
                else None
            )
            for node in nodes
        },
        "pairs": pairs,
    }


def validate_agreements(path):
    record = load_json(path)
    _expect_exact_keys(
        record,
        ("schema", "profile", "producer_provenance_sha256", "pairs"),
        "agreements top-level",
    )
    if type(record["schema"]) is not int or record["schema"] != 1:
        raise EvidenceError("agreements schema must be integer 1")
    profile = validate_profile(record["profile"])
    nodes = profile_nodes(profile)
    bindings = _expect_exact_keys(
        record["producer_provenance_sha256"],
        nodes,
        "producer_provenance_sha256",
    )
    for node in nodes:
        _expect_nullable_sha256(
            bindings[node], f"producer_provenance_sha256.{node}"
        )

    pairs = _expect_array(record["pairs"], "pairs")
    expected_pairs = _profile_pairs(profile)
    if len(pairs) != len(expected_pairs):
        raise EvidenceError("agreements must preserve every fixed pair")
    for pair_index, (pair, expected_pair) in enumerate(zip(pairs, expected_pairs)):
        where = f"pairs[{pair_index}]"
        pair = _expect_exact_keys(pair, ("left", "right", "decisions"), where)
        if (pair["left"], pair["right"]) != expected_pair:
            raise EvidenceError(f"{where} must use fixed profile pair order")
        decisions = _expect_array(pair["decisions"], f"{where}.decisions")
        if len(decisions) != len(KEYS):
            raise EvidenceError(f"{where} must contain four named decisions")
        missing_producer = (
            bindings[pair["left"]] is None or bindings[pair["right"]] is None
        )
        for key_index, (decision, key) in enumerate(zip(decisions, KEYS)):
            decision_where = f"{where}.decisions[{key_index}]"
            decision = _expect_exact_keys(
                decision, ("key", "status", "reason"), decision_where
            )
            if decision["key"] != key:
                raise EvidenceError(
                    f"{decision_where}.key must use fixed key order"
                )
            if decision["status"] not in AGREEMENT_STATUSES:
                raise EvidenceError(f"{decision_where}.status is unknown")
            _expect_nonempty_string(
                decision["reason"], f"{decision_where}.reason"
            )
            if missing_producer != (decision["status"] == "INCOMPLETE"):
                raise EvidenceError(
                    f"{decision_where}.status disagrees with provenance presence"
                )
    return record


def _identity_contract(profile):
    nodes = profile_nodes(profile)
    return {
        "nodes": list(nodes),
        "pairs": [
            {"left": left, "right": right}
            for left, right in _profile_pairs(profile)
        ],
        "named_decisions": [
            {"left": left, "right": right, "key": key}
            for left, right in _profile_pairs(profile)
            for key in KEYS
        ],
        "consumers": list(nodes),
        "transfers": [
            {"consumer": consumer, "producer": producer}
            for consumer, producer in _profile_transfers(profile)
        ],
    }


def _identity_token(value):
    if isinstance(value, str):
        return (value,)
    if "key" in value:
        return (value["left"], value["right"], value["key"])
    if "consumer" in value:
        return (value["consumer"], value["producer"])
    return (value["left"], value["right"])


def _validate_identity_map(value, profile, where, *, complete):
    value = _expect_exact_keys(value, CLOSURE_IDENTITY_KEYS, where)
    expected = _identity_contract(profile)
    for kind in CLOSURE_IDENTITY_KEYS:
        entries = _expect_array(value[kind], f"{where}.{kind}")
        canonical = expected[kind]
        if complete:
            if entries != canonical:
                raise EvidenceError(f"{where}.{kind} differs from fixed profile")
            continue
        canonical_tokens = [_identity_token(entry) for entry in canonical]
        actual_tokens = []
        for index, entry in enumerate(entries):
            if kind in ("nodes", "consumers"):
                validate_node(entry)
                token = (entry,)
            else:
                keys = {
                    "pairs": ("left", "right"),
                    "named_decisions": ("left", "right", "key"),
                    "transfers": ("consumer", "producer"),
                }[kind]
                entry = _expect_exact_keys(
                    entry, keys, f"{where}.{kind}[{index}]"
                )
                token = tuple(entry[key] for key in keys)
            if token not in canonical_tokens:
                raise EvidenceError(f"{where}.{kind}[{index}] is not expected")
            actual_tokens.append(token)
        if len(set(actual_tokens)) != len(actual_tokens):
            raise EvidenceError(f"{where}.{kind} contains duplicate diagnostics")
        order = [canonical_tokens.index(token) for token in actual_tokens]
        if order != sorted(order):
            raise EvidenceError(f"{where}.{kind} is not in fixed profile order")
    return value


def validate_closure(path):
    record = load_json(path)
    _expect_exact_keys(
        record,
        (
            "schema",
            "profile",
            "authoritative",
            "run",
            "agreements_sha256",
            "expected",
            "counts",
            "missing",
            "duplicates",
            "status",
            "error",
        ),
        "closure top-level",
    )
    if type(record["schema"]) is not int or record["schema"] != 1:
        raise EvidenceError("closure schema must be integer 1")
    profile = validate_profile(record["profile"])
    _expect_boolean(record["authoritative"], "closure.authoritative")
    _validate_identity_map(record["expected"], profile, "expected", complete=True)
    missing = _validate_identity_map(
        record["missing"], profile, "missing", complete=False
    )
    duplicates = _validate_identity_map(
        record["duplicates"], profile, "duplicates", complete=False
    )
    for kind in CLOSURE_IDENTITY_KEYS:
        overlap = {
            _identity_token(value) for value in missing[kind]
        } & {_identity_token(value) for value in duplicates[kind]}
        if overlap:
            raise EvidenceError(f"missing and duplicates overlap for {kind}")

    counts = _expect_exact_keys(
        record["counts"], CLOSURE_COUNT_KEYS, "counts"
    )
    for key in CLOSURE_COUNT_KEYS:
        if _expect_integer(counts[key], f"counts.{key}") < 0:
            raise EvidenceError(f"counts.{key} must be non-negative")
    if counts["named_permits"] > counts["named_decisions"]:
        raise EvidenceError("counts.named_permits exceeds named_decisions")
    if counts["passes"] > counts["transfers"]:
        raise EvidenceError("counts.passes exceeds transfers")
    status = record["status"]
    if status not in ("PASS", "REJECT", "INCOMPLETE"):
        raise EvidenceError("closure.status is unknown")

    if record["run"] is None:
        if status != "INCOMPLETE" or record["agreements_sha256"] is not None:
            raise EvidenceError("fallback closure must be INCOMPLETE without Agreement")
        _expect_nonempty_string(record["error"], "closure.error")
        if record["authoritative"]:
            raise EvidenceError("fallback closure cannot be authoritative")
        if any(counts[key] != 0 for key in CLOSURE_COUNT_KEYS):
            raise EvidenceError("fallback closure counts must all be zero")
        if record["missing"] != record["expected"] or any(
            record["duplicates"][key] for key in CLOSURE_IDENTITY_KEYS
        ):
            raise EvidenceError(
                "fallback closure must preserve every missing identity"
            )
    else:
        _validate_run_identity(record["run"], profile)
        _expect_sha256(record["agreements_sha256"], "agreements_sha256")
        if record["error"] is not None:
            raise EvidenceError("evaluated closure.error must be null")

    expected_counts = {
        "nodes": len(profile_nodes(profile)),
        "pairs": len(_profile_pairs(profile)),
        "named_decisions": len(_profile_pairs(profile)) * len(KEYS),
        "consumers": len(profile_nodes(profile)),
        "transfers": len(_profile_transfers(profile)),
    }
    diagnostics_empty = all(
        not missing[key] and not duplicates[key] for key in CLOSURE_IDENTITY_KEYS
    )
    complete_counts = all(counts[key] == value for key, value in expected_counts.items())
    if status in ("PASS", "REJECT") and not (
        diagnostics_empty and complete_counts
    ):
        raise EvidenceError(f"{status} closure must contain every fixed identity")
    if status == "PASS":
        if (
            counts["named_permits"] != expected_counts["named_decisions"]
            or counts["passes"] != expected_counts["transfers"]
        ):
            raise EvidenceError("PASS closure requires every decision and transfer")
    elif status == "REJECT" and (
        counts["named_permits"] == expected_counts["named_decisions"]
        and counts["passes"] == expected_counts["transfers"]
    ):
        raise EvidenceError("REJECT closure requires a rejected decision or transfer")
    if profile == "local-arm64-macos" and record["authoritative"]:
        raise EvidenceError("local closure cannot be authoritative")
    if status == "INCOMPLETE" and record["authoritative"]:
        raise EvidenceError("INCOMPLETE closure cannot be authoritative")
    return record


def _canonical_directory(path, where, *, empty=False):
    path = Path(path)
    if path.is_symlink():
        raise EvidenceError(f"{where} must not be a symlink")
    try:
        resolved = path.resolve(strict=True)
    except OSError as error:
        raise EvidenceError(f"{where} directory is unavailable: {error}") from error
    if not resolved.is_dir():
        raise EvidenceError(f"{where} must be a real directory")
    if empty:
        try:
            entries = list(resolved.iterdir())
        except OSError as error:
            raise EvidenceError(f"cannot inspect {where}: {error}") from error
        if entries:
            raise EvidenceError(f"{where} must be empty in fixture context")
    return resolved


def _is_within(path, directory):
    try:
        Path(path).relative_to(directory)
        return True
    except ValueError:
        return False


def _validate_generated_output(output, expected_name, input_directories=(), input_files=()):
    output = Path(output)
    if output.name != expected_name:
        raise EvidenceError(f"generated header must be named {expected_name}")
    try:
        resolved_output = output.resolve(strict=False)
    except OSError as error:
        raise EvidenceError(f"cannot resolve generated header: {error}") from error
    for directory in input_directories:
        if _is_within(resolved_output, Path(directory)):
            raise EvidenceError("generated output must be outside every input directory")
    for input_file in input_files:
        try:
            resolved_input = Path(input_file).resolve(strict=True)
        except OSError as error:
            raise EvidenceError(f"input file is unavailable: {error}") from error
        if resolved_output == resolved_input:
            raise EvidenceError("generated output must not overwrite an input file")
    return output


def _write_text_atomic(path, text):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + ".tmp")
    try:
        with temporary.open("w", encoding="utf-8", newline="\n") as stream:
            stream.write(text)
        temporary.replace(path)
    except OSError as error:
        try:
            temporary.unlink(missing_ok=True)
        except OSError:
            pass
        raise EvidenceError(f"cannot atomically write {path}: {error}") from error


def _load_context(
    *,
    profile,
    fixture_context,
    expect_source_sha,
    expect_workflow_run,
    sources_lock,
    outputs_lock,
    extra_production_values=(),
):
    profile = validate_profile(profile)
    context_values = (
        expect_source_sha,
        expect_workflow_run,
        sources_lock,
        outputs_lock,
        *extra_production_values,
    )
    if fixture_context:
        if any(value is not None for value in context_values):
            raise EvidenceError(
                "fixture context and production context are mutually exclusive"
            )
        return None
    if any(value is None for value in context_values):
        raise EvidenceError("production context requires every locked input")
    expect_source_sha = _expect_source_sha(
        expect_source_sha, "expect_source_sha"
    )
    _expect_workflow_run(
        expect_workflow_run,
        profile,
        expect_source_sha,
        "expect_workflow_run",
    )
    policies = {}
    sources_digest = None
    outputs_digest = None
    for node in profile_nodes(profile):
        policy, node_sources_digest, node_outputs_digest = (
            load_node_toolchain_policy(sources_lock, outputs_lock, node)
        )
        if sources_digest is None:
            sources_digest = node_sources_digest
            outputs_digest = node_outputs_digest
        elif (
            sources_digest != node_sources_digest
            or outputs_digest != node_outputs_digest
        ):
            raise EvidenceError("normalized node policies disagree on lock identity")
        policies[node] = policy
    return {
        "profile": profile,
        "source_sha": expect_source_sha,
        "workflow_run": expect_workflow_run,
        "sources_sha256": sources_digest,
        "outputs_sha256": outputs_digest,
        "policies": policies,
    }


def _policy_matches_compiler(compiler, policy, profile, node, where):
    comparisons = {
        "family": "compiler_family",
        "revision": "compiler_revision",
        "version": "compiler_version",
        "target": "target",
        "stdlib": "stdlib",
    }
    for actual_key, policy_key in comparisons.items():
        if compiler[actual_key] != policy[policy_key]:
            raise EvidenceError(
                f"{where}.{actual_key} does not match normalized policy for {node}"
            )
    if not _is_macos(node):
        for key in APPLE_IDENTITY_KEYS:
            if compiler[key] != "none":
                raise EvidenceError(f"Linux {where}.{key} must be literal 'none'")
        if not compiler["sdk_locked"]:
            raise EvidenceError(f"Linux {where}.sdk_locked must be true")
    elif profile == "authoritative":
        for key in APPLE_IDENTITY_KEYS:
            if compiler[key] != policy[key]:
                raise EvidenceError(
                    f"{where}.{key} does not match normalized policy for {node}"
                )
        if not compiler["sdk_locked"]:
            raise EvidenceError(f"{where}.sdk_locked must be true")
    else:
        actual_match = all(compiler[key] == policy[key] for key in APPLE_IDENTITY_KEYS)
        if compiler["sdk_locked"] != actual_match:
            raise EvidenceError(f"{where}.sdk_locked is not truthful")


def _producer_slot(node, evidence_root, context):
    empty = {
        "node": node,
        "present": False,
        "error": "producer provenance unavailable",
        "provenance_sha256": "",
        "run": {key: "" for key in RUN_IDENTITY_KEYS},
        "authoritative_eligible": False,
        "admission": [False] * len(KEYS),
        "signatures": [""] * len(KEYS),
        "region_present": False,
        "region_filename": "",
        "region_sha256": "",
    }
    path = evidence_root / f"{node}.provenance.json"
    if not path.exists():
        return empty
    try:
        record = validate_provenance(path)
        if record["status"] == "INCOMPLETE":
            empty["error"] = record["error"]
            return empty
        build = record["build"]
        compiler = record["compiler"]
        locks = record["locks"]
        policy = context["policies"][node]
        if build["profile"] != context["profile"]:
            raise EvidenceError("producer profile differs from selected profile")
        if build["source_sha"] != context["source_sha"]:
            raise EvidenceError("producer source SHA differs from matrix run")
        if build["workflow_run"] != context["workflow_run"]:
            raise EvidenceError("producer workflow run differs from matrix run")
        if locks["sources_sha256"] != context["sources_sha256"]:
            raise EvidenceError("producer source-lock digest differs")
        if locks["outputs_sha256"] != context["outputs_sha256"]:
            raise EvidenceError("producer output-lock digest differs")
        if build["flags"] != policy["flags"]:
            raise EvidenceError("producer flags differ from normalized policy")
        if (
            build["toolchain_artifact_sha256"]
            != policy["toolchain_artifact_sha256"]
        ):
            raise EvidenceError("producer toolchain artifact differs from policy")
        _policy_matches_compiler(
            compiler, policy, context["profile"], node, "producer compiler"
        )
        region = record["artifacts"].get("region")
        authoritative_eligible = (
            context["profile"] == "authoritative"
            and build["execution"] == "native"
            and compiler["sdk_locked"]
        )
        return {
            "node": node,
            "present": True,
            "error": "",
            "provenance_sha256": _sha256(path),
            "run": {
                "source_sha": build["source_sha"],
                "workflow_run": build["workflow_run"],
                "sources_sha256": locks["sources_sha256"],
                "outputs_sha256": locks["outputs_sha256"],
            },
            "authoritative_eligible": authoritative_eligible,
            "admission": [record["admission"][key] for key in KEYS],
            "signatures": [record["signatures"][key] for key in KEYS],
            "region_present": region is not None,
            "region_filename": region["filename"] if region else "",
            "region_sha256": region["sha256"] if region else "",
        }
    except EvidenceError as error:
        empty["error"] = str(error)
        return empty


def _producer_slots(profile, evidence_root, context):
    if context is None:
        return [
            {
                "node": node,
                "present": False,
                "error": "fixture producer evidence unavailable",
                "provenance_sha256": "",
                "run": {key: "" for key in RUN_IDENTITY_KEYS},
                "authoritative_eligible": False,
                "admission": [False] * len(KEYS),
                "signatures": [""] * len(KEYS),
                "region_present": False,
                "region_filename": "",
                "region_sha256": "",
            }
            for node in profile_nodes(profile)
        ]
    return [
        _producer_slot(node, evidence_root, context)
        for node in profile_nodes(profile)
    ]


def _validate_consumer_probe(
    probe_path, node, context, toolchain_artifact_sha256
):
    probe = validate_probe(probe_path)
    if probe["node"] != node:
        raise EvidenceError("consumer probe node differs from --consumer")
    _require_platform_probe_pass(probe)
    artifact = _expect_sha256(
        toolchain_artifact_sha256, "toolchain_artifact_sha256"
    )
    policy = context["policies"][node]
    if artifact != policy["toolchain_artifact_sha256"]:
        raise EvidenceError("consumer toolchain artifact differs from output lock")
    _policy_matches_compiler(
        probe["compiler"], policy, context["profile"], node, "consumer compiler"
    )
    runner = probe["environment"]["runner"]
    execution = (
        "native"
        if context["profile"] == "authoritative"
        else _LOCAL_EXECUTION[node]
    )
    if context["profile"] == "authoritative" and runner != _AUTHORITATIVE_RUNNERS[node]:
        raise EvidenceError("consumer runner differs from authoritative node")
    return {
        **{key: context[key] for key in RUN_IDENTITY_KEYS},
        "execution": execution,
        "runner": runner,
        "runner_image": probe["environment"]["runner_image"],
        "toolchain_artifact_sha256": artifact,
        "compiler_family": probe["compiler"]["family"],
        "compiler_revision": probe["compiler"]["revision"],
        "compiler_version": probe["compiler"]["version"],
        "target": probe["compiler"]["target"],
        "stdlib": probe["compiler"]["stdlib"],
        "flags": policy["flags"],
        "xcode_version": probe["compiler"]["xcode_version"],
        "xcode_build": probe["compiler"]["xcode_build"],
        "sdk_version": probe["compiler"]["sdk_version"],
        "sdk_build": probe["compiler"]["sdk_build"],
        "deployment_target": probe["compiler"]["deployment_target"],
        "sdk_locked": probe["compiler"]["sdk_locked"],
        "authoritative_eligible": (
            context["profile"] == "authoritative"
            and execution == "native"
            and probe["compiler"]["sdk_locked"]
        ),
    }


_CPP_NODE_NAMES = {node: node for node in NODES}
_CPP_KEY_NAMES = {
    "WorldSnapshot": "world_snapshot",
    "Entity": "entity",
    "EntityRelativePtr": "entity_relative_ptr",
    "EntityIndexEntry": "entity_index_entry",
}
_CPP_AGREEMENT_STATUS = {
    "PERMIT": "permit",
    "REJECT": "reject",
    "INCOMPLETE": "incomplete",
}
_CPP_TRANSFER_STATUS = {
    "PASS": "pass",
    "SKIPPED_TYPELAYOUT_REJECT": "skipped_typelayout_reject",
    "REJECT_ENVELOPE": "reject_envelope",
    "REJECT_REGION": "reject_region",
    "REJECT_GRAPH": "reject_graph",
    "INCOMPLETE": "incomplete",
}


class _CppHeader:
    def __init__(self, guard, namespace):
        self.guard = guard
        self.namespace = namespace
        self.definitions = []
        self.counter = 0

    def string(self, value, label="text"):
        if not isinstance(value, str):
            raise EvidenceError(f"generated {label} must be a string")
        encoded = value.encode("utf-8")
        name = f"generated_text_{self.counter}"
        self.counter += 1
        if encoded:
            initializers = ", ".join(
                f"static_cast<char>(0x{byte:02x})" for byte in encoded
            )
            self.definitions.append(
                f"inline constexpr std::array<char, {len(encoded)}> "
                f"{name}_storage{{{{{initializers}}}}};"
            )
            self.definitions.append(
                f"inline constexpr std::string_view {name}{{"
                f"{name}_storage.data(), {len(encoded)}}};"
            )
        else:
            self.definitions.append(
                f"inline constexpr std::array<char, 0> {name}_storage{{}};"
            )
            self.definitions.append(
                f"inline constexpr std::string_view {name}{{}};"
            )
        return name

    def render(self, body):
        definitions = "\n".join(self.definitions)
        return (
            "// Generated by relocatable_world_evidence.py; do not edit.\n"
            f"#ifndef {self.guard}\n#define {self.guard}\n\n"
            '#include "matrix_model.hpp"\n\n'
            "#include <array>\n#include <string_view>\n\n"
            f"namespace {self.namespace} {{\n\n"
            "namespace matrix = relocatable_world_demo::matrix;\n\n"
            f"{definitions}\n\n{body}\n\n"
            f"}} // namespace {self.namespace}\n\n"
            f"#endif // {self.guard}\n"
        )


def _cpp_bool(value):
    return "true" if value else "false"


def _cpp_profile(profile):
    return (
        "matrix::profile_id::authoritative"
        if profile == "authoritative"
        else "matrix::profile_id::local_arm64_macos"
    )


def _cpp_node(node):
    return f"matrix::node_id::{_CPP_NODE_NAMES[node]}"


def _cpp_run(builder, run, label):
    values = [builder.string(run[key], f"{label}.{key}") for key in RUN_IDENTITY_KEYS]
    return "matrix::run_identity{" + ", ".join(values) + "}"


def _cpp_producer(builder, slot):
    error = builder.string(slot["error"], f"{slot['node']} error")
    provenance = builder.string(
        slot["provenance_sha256"], f"{slot['node']} provenance"
    )
    run = _cpp_run(builder, slot["run"], f"{slot['node']} run")
    signatures = [
        builder.string(value, f"{slot['node']} signature")
        for value in slot["signatures"]
    ]
    region_filename = builder.string(
        slot["region_filename"], f"{slot['node']} region filename"
    )
    region_digest = builder.string(
        slot["region_sha256"], f"{slot['node']} region digest"
    )
    admission = ", ".join(_cpp_bool(value) for value in slot["admission"])
    return (
        "matrix::producer_record{"
        f"{_cpp_node(slot['node'])}, {_cpp_bool(slot['present'])}, {error}, "
        f"{provenance}, {run}, {_cpp_bool(slot['authoritative_eligible'])}, "
        f"std::array<bool, matrix::key_count>{{{admission}}}, "
        "std::array<std::string_view, matrix::key_count>{"
        + ", ".join(signatures)
        + "}, "
        f"{_cpp_bool(slot['region_present'])}, {region_filename}, {region_digest}"
        "}"
    )


def _cpp_producer_array(builder, producers):
    values = ",\n    ".join(_cpp_producer(builder, slot) for slot in producers)
    return (
        f"inline constexpr std::array<matrix::producer_record, {len(producers)}> "
        f"producers{{{{\n    {values}\n}}}};"
    )


def _fixture_run_identity(profile):
    return {
        "source_sha": "0" * 40,
        "workflow_run": "1.1" if profile == "authoritative" else "fixture-local",
        "sources_sha256": "0" * 64,
        "outputs_sha256": "0" * 64,
    }


def prepare_consumer(
    *,
    profile,
    consumer,
    evidence,
    output_header,
    fixture_context=False,
    consumer_probe=None,
    toolchain_artifact_sha256=None,
    expect_source_sha=None,
    expect_workflow_run=None,
    sources_lock=None,
    outputs_lock=None,
):
    profile = validate_profile(profile)
    consumer = validate_node(consumer)
    if consumer not in profile_nodes(profile):
        raise EvidenceError(f"consumer {consumer} is not part of profile {profile}")
    context = _load_context(
        profile=profile,
        fixture_context=fixture_context,
        expect_source_sha=expect_source_sha,
        expect_workflow_run=expect_workflow_run,
        sources_lock=sources_lock,
        outputs_lock=outputs_lock,
        extra_production_values=(consumer_probe, toolchain_artifact_sha256),
    )
    evidence_root = _canonical_directory(
        evidence, "producer evidence", empty=fixture_context
    )
    output_header = _validate_generated_output(
        output_header,
        "relocatable_world_consumer_input.hpp",
        (evidence_root,),
        (() if consumer_probe is None else (consumer_probe,)),
    )
    producers = _producer_slots(profile, evidence_root, context)
    if fixture_context:
        run = _fixture_run_identity(profile)
        build = {
            **run,
            "execution": "native",
            "runner": "fixture",
            "runner_image": "fixture",
            "toolchain_artifact_sha256": "0" * 64,
            "compiler_family": _expected_family(consumer),
            "compiler_revision": "fixture",
            "compiler_version": "fixture",
            "target": "fixture",
            "stdlib": "fixture",
            "flags": "fixture",
            **{
                key: ("fixture" if _is_macos(consumer) else "none")
                for key in APPLE_IDENTITY_KEYS
            },
            "sdk_locked": not _is_macos(consumer),
            "authoritative_eligible": False,
        }
    else:
        build = _validate_consumer_probe(
            consumer_probe, consumer, context, toolchain_artifact_sha256
        )
    own = next(slot for slot in producers if slot["node"] == consumer)

    builder = _CppHeader(
        "BOOST_TYPELAYOUT_RELOCATABLE_WORLD_CONSUMER_INPUT_HPP",
        "relocatable_world_demo::generated::consumer_input",
    )
    evidence_root_view = builder.string(str(evidence_root), "evidence root")
    provenance_view = builder.string(
        own["provenance_sha256"] if own["present"] else "",
        "consumer provenance",
    )
    build_run = _cpp_run(builder, build, "consumer build run")
    build_strings = {
        key: builder.string(build[key], f"consumer build {key}")
        for key in RESULT_BUILD_KEYS[4:-1]
    }
    producer_declaration = _cpp_producer_array(builder, producers)
    body = "\n".join(
        (
            f"inline constexpr bool fixture_context = {_cpp_bool(fixture_context)};",
            f"inline constexpr auto profile = {_cpp_profile(profile)};",
            f"inline constexpr auto consumer = {_cpp_node(consumer)};",
            f"inline constexpr std::string_view evidence_root = {evidence_root_view};",
            (
                "inline constexpr bool consumer_provenance_present = "
                f"{_cpp_bool(own['present'])};"
            ),
            (
                "inline constexpr std::string_view consumer_provenance_sha256 = "
                f"{provenance_view};"
            ),
            (
                "inline constexpr matrix::consumer_build_record build{"
                f"{build_run}, {build_strings['execution']}, "
                f"{build_strings['runner']}, {build_strings['runner_image']}, "
                f"{build_strings['toolchain_artifact_sha256']}, "
                f"{build_strings['compiler_family']}, "
                f"{build_strings['compiler_revision']}, "
                f"{build_strings['compiler_version']}, {build_strings['target']}, "
                f"{build_strings['stdlib']}, {build_strings['flags']}, "
                f"{build_strings['xcode_version']}, {build_strings['xcode_build']}, "
                f"{build_strings['sdk_version']}, {build_strings['sdk_build']}, "
                f"{build_strings['deployment_target']}, "
                f"{_cpp_bool(build['sdk_locked'])}, "
                f"{_cpp_bool(build['authoritative_eligible'])}}};"
            ),
            producer_declaration,
        )
    )
    _write_text_atomic(output_header, builder.render(body))
    return output_header


def prepare_agreements(
    *,
    profile,
    evidence,
    output_header,
    fixture_context=False,
    expect_source_sha=None,
    expect_workflow_run=None,
    sources_lock=None,
    outputs_lock=None,
):
    profile = validate_profile(profile)
    context = _load_context(
        profile=profile,
        fixture_context=fixture_context,
        expect_source_sha=expect_source_sha,
        expect_workflow_run=expect_workflow_run,
        sources_lock=sources_lock,
        outputs_lock=outputs_lock,
    )
    evidence_root = _canonical_directory(
        evidence, "producer evidence", empty=fixture_context
    )
    output_header = _validate_generated_output(
        output_header,
        "relocatable_world_agreement_input.hpp",
        (evidence_root,),
    )
    producers = _producer_slots(profile, evidence_root, context)
    builder = _CppHeader(
        "BOOST_TYPELAYOUT_RELOCATABLE_WORLD_AGREEMENT_INPUT_HPP",
        "relocatable_world_demo::generated::agreement_input",
    )
    producer_declaration = _cpp_producer_array(builder, producers)
    body = "\n".join(
        (
            f"inline constexpr bool fixture_context = {_cpp_bool(fixture_context)};",
            f"inline constexpr auto profile = {_cpp_profile(profile)};",
            producer_declaration,
        )
    )
    _write_text_atomic(output_header, builder.render(body))
    return output_header


def _consumer_build_matches_policy(build, node, profile, context):
    policy = context["policies"][node]
    for key in RUN_IDENTITY_KEYS:
        if build[key] != context[key]:
            raise EvidenceError(f"consumer {node} {key} differs from matrix run")
    comparisons = {
        "toolchain_artifact_sha256": "toolchain_artifact_sha256",
        "compiler_family": "compiler_family",
        "compiler_revision": "compiler_revision",
        "compiler_version": "compiler_version",
        "target": "target",
        "stdlib": "stdlib",
        "flags": "flags",
    }
    for build_key, policy_key in comparisons.items():
        if build[build_key] != policy[policy_key]:
            raise EvidenceError(
                f"consumer {node} {build_key} differs from normalized policy"
            )
    if not _is_macos(node):
        for key in APPLE_IDENTITY_KEYS:
            if build[key] != "none":
                raise EvidenceError(f"Linux consumer {node} {key} must be none")
        if not build["sdk_locked"]:
            raise EvidenceError(f"Linux consumer {node} SDK must be locked")
    elif profile == "authoritative":
        for key in APPLE_IDENTITY_KEYS:
            if build[key] != policy[key]:
                raise EvidenceError(
                    f"consumer {node} {key} differs from normalized policy"
                )
        if not build["sdk_locked"]:
            raise EvidenceError(f"authoritative consumer {node} SDK is unlocked")
    else:
        actual_match = all(build[key] == policy[key] for key in APPLE_IDENTITY_KEYS)
        if build["sdk_locked"] != actual_match:
            raise EvidenceError(f"local consumer {node} sdk_locked is not truthful")


def _consumer_slot(node, results_root, producers, context):
    expected_producers = [
        producer for producer in profile_nodes(context["profile"]) if producer != node
    ]
    empty = {
        "consumer": node,
        "present": False,
        "error": "consumer result unavailable",
        "run": {key: "" for key in RUN_IDENTITY_KEYS},
        "authoritative_eligible": False,
        "transfers": [
            {"consumer": node, "producer": producer, "status": "INCOMPLETE"}
            for producer in expected_producers
        ],
    }
    path = results_root / f"{node}.results.json"
    if not path.exists():
        return empty
    producer_by_node = {record["node"]: record for record in producers}
    try:
        record = validate_results(path)
        if record["profile"] != context["profile"] or record["consumer"] != node:
            raise EvidenceError("consumer result identity differs from selected slot")
        if record["build"] is None:
            empty["error"] = "fallback consumer result"
            return empty
        build = record["build"]
        _consumer_build_matches_policy(
            build, node, context["profile"], context
        )
        own_producer = producer_by_node[node]
        if (
            not own_producer["present"]
            or record["consumer_provenance_sha256"]
            != own_producer["provenance_sha256"]
        ):
            raise EvidenceError("consumer result is not bound to own provenance")

        transfers = []
        for transfer in record["transfers"]:
            producer = producer_by_node[transfer["producer"]]
            status = transfer["status"]
            if not producer["present"]:
                if status != "INCOMPLETE":
                    raise EvidenceError(
                        "missing producer requires INCOMPLETE transfer"
                    )
            else:
                supplied_provenance = transfer["producer_provenance_sha256"]
                if (
                    supplied_provenance is not None
                    and supplied_provenance != producer["provenance_sha256"]
                ):
                    raise EvidenceError("transfer producer provenance digest differs")
                supplied_region = transfer["region_sha256"]
                if (
                    supplied_region is not None
                    and supplied_region != producer["region_sha256"]
                ):
                    raise EvidenceError("transfer region digest differs")
                gate_permits = all(
                    own_producer["admission"][key_index]
                    and producer["admission"][key_index]
                    and own_producer["signatures"][key_index]
                    == producer["signatures"][key_index]
                    for key_index in range(len(KEYS))
                )
                if gate_permits and status == "SKIPPED_TYPELAYOUT_REJECT":
                    raise EvidenceError("consumer skipped after a permitting TypeLayout gate")
                if not gate_permits and status != "SKIPPED_TYPELAYOUT_REJECT":
                    raise EvidenceError("consumer loaded after a rejecting TypeLayout gate")
                if status in (
                    "PASS",
                    "REJECT_ENVELOPE",
                    "REJECT_REGION",
                    "REJECT_GRAPH",
                ) and not producer["region_present"]:
                    raise EvidenceError("loader result requires a verified region")
            transfers.append(
                {
                    "consumer": node,
                    "producer": transfer["producer"],
                    "status": status,
                }
            )
        return {
            "consumer": node,
            "present": True,
            "error": "",
            "run": {key: build[key] for key in RUN_IDENTITY_KEYS},
            "authoritative_eligible": (
                context["profile"] == "authoritative"
                and build["execution"] == "native"
                and build["sdk_locked"]
            ),
            "transfers": transfers,
        }
    except EvidenceError as error:
        empty["error"] = str(error)
        return empty


def _fixture_consumer_slots(profile):
    return [
        {
            "consumer": consumer,
            "present": False,
            "error": "fixture consumer result unavailable",
            "run": {key: "" for key in RUN_IDENTITY_KEYS},
            "authoritative_eligible": False,
            "transfers": [
                {
                    "consumer": consumer,
                    "producer": producer,
                    "status": "INCOMPLETE",
                }
                for producer in profile_nodes(profile)
                if producer != consumer
            ],
        }
        for consumer in profile_nodes(profile)
    ]


def _cpp_agreement_pairs(agreement):
    records = []
    for pair in agreement["pairs"]:
        decisions = ", ".join(
            "matrix::named_decision{"
            f"matrix::key_id::{_CPP_KEY_NAMES[decision['key']]}, "
            "matrix::agreement_status::"
            f"{_CPP_AGREEMENT_STATUS[decision['status']]}}}"
            for decision in pair["decisions"]
        )
        records.append(
            "matrix::pair_record{"
            f"{_cpp_node(pair['left'])}, {_cpp_node(pair['right'])}, "
            f"std::array<matrix::named_decision, matrix::key_count>{{{decisions}}}"
            "}"
        )
    return (
        f"inline constexpr std::array<matrix::pair_record, {len(records)}> "
        "agreements{{\n    " + ",\n    ".join(records) + "\n}};"
    )


def prepare_matrix(
    *,
    profile,
    evidence,
    results,
    agreements,
    output_header,
    fixture_context=False,
    expect_source_sha=None,
    expect_workflow_run=None,
    sources_lock=None,
    outputs_lock=None,
):
    profile = validate_profile(profile)
    context = _load_context(
        profile=profile,
        fixture_context=fixture_context,
        expect_source_sha=expect_source_sha,
        expect_workflow_run=expect_workflow_run,
        sources_lock=sources_lock,
        outputs_lock=outputs_lock,
    )
    evidence_root = _canonical_directory(
        evidence, "producer evidence", empty=fixture_context
    )
    results_root = _canonical_directory(
        results, "consumer results", empty=fixture_context
    )
    agreement_input = Path(agreements)
    if agreement_input.is_symlink():
        raise EvidenceError("Agreement input must not be a symlink")
    try:
        agreements_path = agreement_input.resolve(strict=True)
    except OSError as error:
        raise EvidenceError(f"Agreement input is unavailable: {error}") from error
    if not agreements_path.is_file():
        raise EvidenceError("Agreement input must be a regular file")
    if _is_within(agreements_path, evidence_root) or _is_within(
        agreements_path, results_root
    ):
        raise EvidenceError(
            "Agreement input must be outside producer and result directories"
        )
    output_header = _validate_generated_output(
        output_header,
        "relocatable_world_matrix_input.hpp",
        (evidence_root, results_root),
        (agreements_path,),
    )
    agreement = validate_agreements(agreements_path)
    if agreement["profile"] != profile:
        raise EvidenceError("Agreement profile differs from selected profile")
    producers = _producer_slots(profile, evidence_root, context)
    recomputed = _expected_agreement(profile, producers)
    if fixture_context:
        if any(
            digest is not None
            for digest in agreement["producer_provenance_sha256"].values()
        ) or any(
            decision["status"] != "INCOMPLETE"
            for pair in agreement["pairs"]
            for decision in pair["decisions"]
        ):
            raise EvidenceError("fixture Agreement must contain only missing slots")
        consumers = _fixture_consumer_slots(profile)
        expected_run = _fixture_run_identity(profile)
    else:
        if agreement != recomputed:
            raise EvidenceError(
                "Agreement artifact is stale or differs from producer evidence"
            )
        consumers = [
            _consumer_slot(node, results_root, producers, context)
            for node in profile_nodes(profile)
        ]
        expected_run = {key: context[key] for key in RUN_IDENTITY_KEYS}

    builder = _CppHeader(
        "BOOST_TYPELAYOUT_RELOCATABLE_WORLD_MATRIX_INPUT_HPP",
        "relocatable_world_demo::generated::matrix_input",
    )
    agreement_digest = builder.string(
        _sha256(agreements_path), "Agreement digest"
    )
    run_initializer = _cpp_run(builder, expected_run, "expected run")
    producer_declaration = _cpp_producer_array(builder, producers)

    bindings = []
    for node in profile_nodes(profile):
        digest = agreement["producer_provenance_sha256"][node]
        digest_view = builder.string(digest or "", f"{node} Agreement binding")
        bindings.append(
            "matrix::provenance_binding{"
            f"{_cpp_node(node)}, {_cpp_bool(digest is not None)}, {digest_view}}}"
        )
    binding_declaration = (
        f"inline constexpr std::array<matrix::provenance_binding, {len(bindings)}> "
        "agreement_provenance{{\n    "
        + ",\n    ".join(bindings)
        + "\n}};"
    )
    agreement_declaration = _cpp_agreement_pairs(agreement)
    consumer_records = []
    for slot in consumers:
        consumer_run = _cpp_run(
            builder, slot["run"], f"{slot['consumer']} consumer run"
        )
        consumer_records.append(
            "matrix::consumer_record{"
            f"{_cpp_node(slot['consumer'])}, {_cpp_bool(slot['present'])}, "
            f"{consumer_run}, {_cpp_bool(slot['authoritative_eligible'])}}}"
        )
    consumer_declaration = (
        f"inline constexpr std::array<matrix::consumer_record, {len(consumers)}> "
        "consumers{{\n    " + ",\n    ".join(consumer_records) + "\n}};"
    )
    transfer_records = [
        "matrix::transfer_record{"
        f"{_cpp_node(transfer['consumer'])}, {_cpp_node(transfer['producer'])}, "
        f"matrix::transfer_status::{_CPP_TRANSFER_STATUS[transfer['status']]}}}"
        for consumer in consumers
        for transfer in consumer["transfers"]
    ]
    transfer_declaration = (
        f"inline constexpr std::array<matrix::transfer_record, {len(transfer_records)}> "
        "transfers{{\n    " + ",\n    ".join(transfer_records) + "\n}};"
    )
    body = "\n".join(
        (
            f"inline constexpr bool fixture_context = {_cpp_bool(fixture_context)};",
            f"inline constexpr auto profile = {_cpp_profile(profile)};",
            f"inline constexpr matrix::run_identity expected_run = {run_initializer};",
            f"inline constexpr std::string_view agreements_sha256 = {agreement_digest};",
            producer_declaration,
            binding_declaration,
            agreement_declaration,
            consumer_declaration,
            transfer_declaration,
        )
    )
    _write_text_atomic(output_header, builder.render(body))
    return output_header


def _require_flat_run_directory(directory, profile):
    root = _canonical_directory(directory, "audit run")
    nodes = profile_nodes(profile)
    expected_names = {
        "agreements.json",
        "closure.json",
        "source-sha.txt",
        "workflow-run.txt" if profile == "authoritative" else "run-id.txt",
    }
    for node in nodes:
        expected_names.update(
            {
                f"{node}.provenance.json",
                f"{node}.sig.hpp",
                f"{node}.region",
                f"{node}.results.json",
            }
        )
    observed = set()
    for entry in root.iterdir():
        if entry.is_symlink() or not entry.is_file():
            raise EvidenceError("audit run must be one fixed flat directory")
        if entry.name in observed:
            raise EvidenceError(f"audit run contains duplicate filename {entry.name}")
        observed.add(entry.name)
    missing = expected_names - observed
    unexpected = observed - expected_names
    if missing:
        raise EvidenceError(
            "audit run is missing fixed files: " + ", ".join(sorted(missing))
        )
    if unexpected:
        raise EvidenceError(
            "audit run contains unexpected flat files: "
            + ", ".join(sorted(unexpected))
        )
    return root


def _validate_required_run_metadata(
    root, profile, expect_source_sha, expect_workflow_run
):
    names = (
        "source-sha.txt",
        "workflow-run.txt" if profile == "authoritative" else "run-id.txt",
    )
    expected = (expect_source_sha, expect_workflow_run)
    for name, expected_value in zip(names, expected):
        path = root / name
        try:
            text = path.read_text(encoding="utf-8")
        except (OSError, UnicodeError) as error:
            raise EvidenceError(f"cannot read run metadata {path.name}: {error}") from error
        if text not in (expected_value, expected_value + "\n"):
            raise EvidenceError(
                f"run metadata {path.name} must be one exact matching line"
            )


def audit_run(
    *,
    directory,
    expect_source_sha,
    sources_lock,
    outputs_lock,
    expect_nodes,
    expect_pairs,
    expect_named_permits,
    expect_transfers,
    expect_workflow_run=None,
):
    if expect_nodes == len(NODES):
        profile = "authoritative"
    elif expect_nodes == len(LOCAL_NODES):
        profile = "local-arm64-macos"
    else:
        raise EvidenceError("expect_nodes must select the fixed 6-node or 5-node profile")
    fixed_counts = {
        "nodes": len(profile_nodes(profile)),
        "pairs": len(_profile_pairs(profile)),
        "named_permits": len(_profile_pairs(profile)) * len(KEYS),
        "transfers": len(_profile_transfers(profile)),
    }
    supplied_counts = {
        "nodes": expect_nodes,
        "pairs": expect_pairs,
        "named_permits": expect_named_permits,
        "transfers": expect_transfers,
    }
    if supplied_counts != fixed_counts:
        raise EvidenceError("caller-supplied audit counts differ from fixed profile")

    root = _require_flat_run_directory(directory, profile)
    if expect_workflow_run is None:
        first_path = root / f"{profile_nodes(profile)[0]}.provenance.json"
        first = validate_provenance(first_path)
        if first["status"] == "INCOMPLETE":
            raise EvidenceError("cannot infer workflow run from incomplete provenance")
        expect_workflow_run = first["build"]["workflow_run"]
    _validate_required_run_metadata(
        root, profile, expect_source_sha, expect_workflow_run
    )
    context = _load_context(
        profile=profile,
        fixture_context=False,
        expect_source_sha=expect_source_sha,
        expect_workflow_run=expect_workflow_run,
        sources_lock=sources_lock,
        outputs_lock=outputs_lock,
    )
    producers = _producer_slots(profile, root, context)
    for producer in producers:
        if not producer["present"]:
            raise EvidenceError(
                f"producer {producer['node']} is incomplete: {producer['error']}"
            )
        if not producer["region_present"]:
            raise EvidenceError(f"producer {producer['node']} is not READY")
        if profile == "authoritative" and not producer["authoritative_eligible"]:
            raise EvidenceError(
                f"producer {producer['node']} is not authoritative evidence"
            )

    agreement_path = root / "agreements.json"
    agreement = validate_agreements(agreement_path)
    if agreement != _expected_agreement(profile, producers):
        raise EvidenceError("Agreement differs from recomputed producer decisions")
    if any(
        decision["status"] != "PERMIT"
        for pair in agreement["pairs"]
        for decision in pair["decisions"]
    ):
        raise EvidenceError("audited Agreement contains a non-PERMIT decision")

    consumers = [
        _consumer_slot(node, root, producers, context)
        for node in profile_nodes(profile)
    ]
    for consumer in consumers:
        if not consumer["present"]:
            raise EvidenceError(
                f"consumer {consumer['consumer']} is incomplete: {consumer['error']}"
            )
        if profile == "authoritative" and not consumer["authoritative_eligible"]:
            raise EvidenceError(
                f"consumer {consumer['consumer']} is not authoritative evidence"
            )
        if any(
            transfer["status"] != "PASS"
            for transfer in consumer["transfers"]
        ):
            raise EvidenceError(
                f"consumer {consumer['consumer']} contains a non-PASS transfer"
            )

    closure = validate_closure(root / "closure.json")
    if closure["profile"] != profile:
        raise EvidenceError("closure profile differs from audit profile")
    expected_run = {key: context[key] for key in RUN_IDENTITY_KEYS}
    if closure["run"] != expected_run:
        raise EvidenceError("closure run identity differs from audited context")
    if closure["agreements_sha256"] != _sha256(agreement_path):
        raise EvidenceError("closure Agreement digest differs from agreements.json")
    if closure["expected"] != _identity_contract(profile):
        raise EvidenceError("closure expected identities differ from fixed profile")
    expected_closure_counts = {
        "nodes": fixed_counts["nodes"],
        "pairs": fixed_counts["pairs"],
        "named_decisions": fixed_counts["named_permits"],
        "named_permits": fixed_counts["named_permits"],
        "consumers": fixed_counts["nodes"],
        "transfers": fixed_counts["transfers"],
        "passes": fixed_counts["transfers"],
    }
    if closure["counts"] != expected_closure_counts:
        raise EvidenceError("closure counts differ from audited fixed graph")
    if any(closure[where][key] for where in ("missing", "duplicates") for key in CLOSURE_IDENTITY_KEYS):
        raise EvidenceError("closure contains missing or duplicate identities")
    if closure["status"] != "PASS" or closure["error"] is not None:
        raise EvidenceError("audited closure is not PASS")
    expected_authoritative = profile == "authoritative"
    if closure["authoritative"] != expected_authoritative:
        raise EvidenceError("closure authoritative flag differs from evidence profile")
    return closure


def _add_profile_argument(parser):
    parser.add_argument("--profile", required=True, choices=PROFILES)


def _build_parser():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)

    validate = commands.add_parser("validate-provenance")
    validate.add_argument("file", type=Path)

    validate_results_parser = commands.add_parser("validate-results")
    validate_results_parser.add_argument("file", type=Path)

    validate_agreements_parser = commands.add_parser("validate-agreements")
    validate_agreements_parser.add_argument("file", type=Path)

    validate_closure_parser = commands.add_parser("validate-closure")
    validate_closure_parser.add_argument("file", type=Path)

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

    prepare_consumer_parser = commands.add_parser("prepare-consumer")
    _add_profile_argument(prepare_consumer_parser)
    prepare_consumer_parser.add_argument("--consumer", required=True)
    prepare_consumer_parser.add_argument("--evidence", required=True, type=Path)
    prepare_consumer_parser.add_argument("--consumer-probe", type=Path)
    prepare_consumer_parser.add_argument("--toolchain-artifact-sha256")
    prepare_consumer_parser.add_argument("--expect-source-sha")
    prepare_consumer_parser.add_argument("--expect-workflow-run")
    prepare_consumer_parser.add_argument("--sources-lock", type=Path)
    prepare_consumer_parser.add_argument("--outputs-lock", type=Path)
    prepare_consumer_parser.add_argument("--fixture-context", action="store_true")
    prepare_consumer_parser.add_argument(
        "--output-header", required=True, type=Path
    )

    prepare_agreements_parser = commands.add_parser("prepare-agreements")
    _add_profile_argument(prepare_agreements_parser)
    prepare_agreements_parser.add_argument(
        "--evidence", required=True, type=Path
    )
    prepare_agreements_parser.add_argument("--expect-source-sha")
    prepare_agreements_parser.add_argument("--expect-workflow-run")
    prepare_agreements_parser.add_argument("--sources-lock", type=Path)
    prepare_agreements_parser.add_argument("--outputs-lock", type=Path)
    prepare_agreements_parser.add_argument("--fixture-context", action="store_true")
    prepare_agreements_parser.add_argument(
        "--output-header", required=True, type=Path
    )

    prepare_matrix_parser = commands.add_parser("prepare-matrix")
    _add_profile_argument(prepare_matrix_parser)
    prepare_matrix_parser.add_argument("--evidence", required=True, type=Path)
    prepare_matrix_parser.add_argument("--results", required=True, type=Path)
    prepare_matrix_parser.add_argument("--agreements", required=True, type=Path)
    prepare_matrix_parser.add_argument("--expect-source-sha")
    prepare_matrix_parser.add_argument("--expect-workflow-run")
    prepare_matrix_parser.add_argument("--sources-lock", type=Path)
    prepare_matrix_parser.add_argument("--outputs-lock", type=Path)
    prepare_matrix_parser.add_argument("--fixture-context", action="store_true")
    prepare_matrix_parser.add_argument("--output-header", required=True, type=Path)

    audit = commands.add_parser("audit-run")
    audit.add_argument("--directory", required=True, type=Path)
    audit.add_argument("--expect-source-sha", required=True)
    audit.add_argument("--expect-workflow-run")
    audit.add_argument("--sources-lock", required=True, type=Path)
    audit.add_argument("--outputs-lock", required=True, type=Path)
    audit.add_argument("--expect-nodes", required=True, type=int)
    audit.add_argument("--expect-pairs", required=True, type=int)
    audit.add_argument("--expect-named-permits", required=True, type=int)
    audit.add_argument("--expect-transfers", required=True, type=int)
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
        elif args.command == "validate-results":
            record = validate_results(args.file)
            print(
                f"RESULTS PASS consumer={record['consumer']} "
                f"transfers={len(record['transfers'])}"
            )
        elif args.command == "validate-agreements":
            record = validate_agreements(args.file)
            print(
                f"AGREEMENTS PASS profile={record['profile']} "
                f"pairs={len(record['pairs'])}"
            )
        elif args.command == "validate-closure":
            record = validate_closure(args.file)
            print(
                f"CLOSURE PASS profile={record['profile']} status={record['status']}"
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
        elif args.command == "prepare-consumer":
            prepare_consumer(
                profile=args.profile,
                consumer=args.consumer,
                evidence=args.evidence,
                consumer_probe=args.consumer_probe,
                toolchain_artifact_sha256=args.toolchain_artifact_sha256,
                expect_source_sha=args.expect_source_sha,
                expect_workflow_run=args.expect_workflow_run,
                sources_lock=args.sources_lock,
                outputs_lock=args.outputs_lock,
                fixture_context=args.fixture_context,
                output_header=args.output_header,
            )
        elif args.command == "prepare-agreements":
            prepare_agreements(
                profile=args.profile,
                evidence=args.evidence,
                expect_source_sha=args.expect_source_sha,
                expect_workflow_run=args.expect_workflow_run,
                sources_lock=args.sources_lock,
                outputs_lock=args.outputs_lock,
                fixture_context=args.fixture_context,
                output_header=args.output_header,
            )
        elif args.command == "prepare-matrix":
            prepare_matrix(
                profile=args.profile,
                evidence=args.evidence,
                results=args.results,
                agreements=args.agreements,
                expect_source_sha=args.expect_source_sha,
                expect_workflow_run=args.expect_workflow_run,
                sources_lock=args.sources_lock,
                outputs_lock=args.outputs_lock,
                fixture_context=args.fixture_context,
                output_header=args.output_header,
            )
        elif args.command == "audit-run":
            record = audit_run(
                directory=args.directory,
                expect_source_sha=args.expect_source_sha,
                expect_workflow_run=args.expect_workflow_run,
                sources_lock=args.sources_lock,
                outputs_lock=args.outputs_lock,
                expect_nodes=args.expect_nodes,
                expect_pairs=args.expect_pairs,
                expect_named_permits=args.expect_named_permits,
                expect_transfers=args.expect_transfers,
            )
            print(
                f"AUDIT PASS profile={record['profile']} "
                f"nodes={record['counts']['nodes']} "
                f"pairs={record['counts']['pairs']} "
                f"named_permits={record['counts']['named_permits']} "
                f"transfers={record['counts']['transfers']}"
            )
    except EvidenceError as error:
        parser.exit(2, f"evidence error: {error}\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
