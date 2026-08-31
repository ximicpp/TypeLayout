#!/usr/bin/env bash
set -euo pipefail

# A registry token must never appear in an inherited xtrace stream.
case "$-" in
    *x*) set +x ;;
esac

# Credential copies must remain shell-local even if the caller enabled allexport.
set +a
unset explicit_user explicit_token ghcr_user ghcr_token
explicit_user=${TYPELAYOUT_GHCR_USER-}
explicit_token=${TYPELAYOUT_GHCR_TOKEN-}
unset TYPELAYOUT_GHCR_USER TYPELAYOUT_GHCR_TOKEN

readonly PROFILE=local-arm64-macos
readonly OUTPUT_RELATIVE=build/relocatable-world-local
readonly SOURCES_LOCK=.github/docker/toolchain-sources.lock
readonly OUTPUTS_LOCK=.github/docker/toolchains.lock
readonly LOCK_VALIDATOR=.github/scripts/validate-toolchain-locks.py
readonly MACOS_VERIFIER=.github/scripts/verify-p2996-toolchain.sh
readonly EVIDENCE_TOOL=tools/relocatable_world_evidence.py
readonly LOCAL_RUNNER=local-arm64-macos
readonly FINAL_LINE="LOCAL COVERAGE 5/6: 3 native-architecture + 2 Docker-emulated; WORLD Agreement 40/40 transfers 20/20; UNIT Agreement 40/40 handoffs 20/20; authoritative closure unavailable"

readonly -a LOCAL_NODES=(
    "x86_64_linux_gcc"
    "x86_64_linux_clang"
    "arm64_linux_gcc"
    "arm64_linux_clang"
    "arm64_macos_clang"
)

readonly -a LINUX_NODE_SPECS=(
    "x86_64_linux_gcc|linux/amd64|gcc|emulated"
    "x86_64_linux_clang|linux/amd64|p2996|emulated"
    "arm64_linux_gcc|linux/arm64|gcc|native"
    "arm64_linux_clang|linux/arm64|p2996|native"
)

readonly -a IMPLEMENTATION_PATHS=(
    ".gitattributes"
    "CMakeLists.txt"
    "cmake"
    "include/boost/typelayout"
    "include/boost/typelayout.hpp"
    "example/relocatable_world_demo"
    "tools/relocatable_world_evidence.py"
    "tools/run-relocatable-world.sh"
    ".github/docker/toolchain-sources.lock"
    ".github/docker/toolchains.lock"
    ".github/scripts/validate-toolchain-locks.py"
    ".github/scripts/macos-runtime-origin-probe.cpp"
    ".github/scripts/verify-p2996-toolchain.sh"
)

usage() {
    cat <<'EOF'
Usage: tools/run-relocatable-world.sh [--source-sha SHA] [--run-id ID]

Run the non-authoritative five-node relocatable-world closure on an ARM64 Mac.
An explicit source SHA must equal the exact current HEAD. The output directory is
build/relocatable-world-local and must not already exist.
EOF
}

die() {
    printf 'relocatable-world launcher: %s\n' "$*" >&2
    exit 1
}

require_command() {
    command -v "$1" >/dev/null 2>&1 \
        || die "required command is missing: $1"
}

verify_implementation_state() {
    local implementation_status
    local implementation_path
    local tracked_count
    local candidate

    git diff --quiet HEAD -- "${IMPLEMENTATION_PATHS[@]}" \
        || die "executable source differs from HEAD"
    implementation_status="$(
        git status --porcelain=v1 --untracked-files=all -- \
            "${IMPLEMENTATION_PATHS[@]}"
    )"
    [[ -z "${implementation_status}" ]] \
        || die "executable source contains uncommitted or untracked files"

    for implementation_path in "${IMPLEMENTATION_PATHS[@]}"; do
        [[ -e "${implementation_path}" ]] \
            || die "required implementation path is missing: ${implementation_path}"
        if [[ -d "${implementation_path}" ]]; then
            tracked_count="$(
                git ls-files -- "${implementation_path}" \
                    | wc -l | tr -d '[:space:]'
            )"
            [[ "${tracked_count}" != 0 ]] \
                || die "implementation directory has no tracked files: ${implementation_path}"
            while IFS= read -r -d '' candidate; do
                candidate=${candidate#./}
                git ls-files --error-unmatch -- "${candidate}" >/dev/null 2>&1 \
                    || die "implementation file is not tracked: ${candidate}"
            done < <(find "${implementation_path}" \
                \( -type f -o -type l \) -print0)
        else
            git ls-files --error-unmatch -- "${implementation_path}" \
                    >/dev/null 2>&1 \
                || die "implementation file is not tracked: ${implementation_path}"
        fi
    done
}

source_sha_argument=
run_id_argument=
source_sha_supplied=false
run_id_supplied=false
while (($#)); do
    case "$1" in
        --source-sha)
            (($# >= 2)) || die "--source-sha requires a value"
            [[ "${source_sha_supplied}" == false ]] \
                || die "--source-sha may be specified only once"
            source_sha_argument=$2
            source_sha_supplied=true
            shift 2
            ;;
        --run-id)
            (($# >= 2)) || die "--run-id requires a value"
            [[ "${run_id_supplied}" == false ]] \
                || die "--run-id may be specified only once"
            run_id_argument=$2
            run_id_supplied=true
            shift 2
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        *)
            die "unknown option: $1"
            ;;
    esac
done

require_command git
script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
repository_root="$(cd -- "${script_dir}/.." && pwd -P)"
cd "${repository_root}"

head_sha="$(git rev-parse --verify 'HEAD^{commit}')" \
    || die "cannot resolve the current HEAD commit"
[[ "${head_sha}" =~ ^[0-9a-f]{40}$ ]] \
    || die "current HEAD is not a lowercase 40-hex commit"

if [[ "${source_sha_supplied}" == true ]]; then
    [[ "${source_sha_argument}" =~ ^[0-9a-f]{40}$ ]] \
        || die "--source-sha must be a lowercase 40-hex commit"
    [[ "${source_sha_argument}" == "${head_sha}" ]] \
        || die "--source-sha does not equal current HEAD ${head_sha}"
    source_sha=${source_sha_argument}
else
    source_sha=${head_sha}
fi

if [[ "${run_id_supplied}" == true ]]; then
    run_id=${run_id_argument}
else
    run_id="local-${head_sha:0:12}-$(date -u +%Y%m%dT%H%M%SZ)-$$"
fi
if ((${#run_id} == 0 || ${#run_id} > 128)) \
    || [[ ! "${run_id}" =~ ^[A-Za-z0-9]([A-Za-z0-9_.-]*[A-Za-z0-9])?$ ]] \
    || [[ "${run_id}" == *..* ]]; then
    die "--run-id must use 1-128 safe filename characters, begin and end with an alphanumeric, and not contain '..'"
fi
if [[ "${run_id}" =~ ^[0-9]+\.[0-9]+$ ]]; then
    die "--run-id must not use the authoritative run_id.run_attempt form"
fi

printf 'SOURCE SHA: %s\n' "${source_sha}"
printf 'RUN ID: %s\n' "${run_id}"

verify_implementation_state

output_directory="${repository_root}/${OUTPUT_RELATIVE}"
[[ ! -e "${output_directory}" ]] \
    || die "evidence output already exists: ${output_directory}"

[[ "$(uname -s)" == Darwin ]] \
    || die "this launcher requires macOS"
[[ "$(uname -m)" == arm64 ]] \
    || die "this launcher requires a native ARM64 Mac"

for command_name in \
        bash python3 cmake ninja c++ docker curl git find grep awk sed tr \
        mktemp date uname xcode-select xcodebuild xcrun sw_vers tar zstd \
        shasum otool stat; do
    require_command "${command_name}"
done

for required_file in \
        "${SOURCES_LOCK}" "${OUTPUTS_LOCK}" "${LOCK_VALIDATOR}" \
        "${MACOS_VERIFIER}" "${EVIDENCE_TOOL}" \
        "tools/run-relocatable-world.sh"; do
    [[ -f "${required_file}" && ! -L "${required_file}" ]] \
        || die "required preflight file is missing or is a symbolic link: ${required_file}"
done

for lf_script in \
        "tools/run-relocatable-world.sh" "${EVIDENCE_TOOL}" \
        "${LOCK_VALIDATOR}" "${MACOS_VERIFIER}"; do
    eol_record="$(git ls-files --eol -- "${lf_script}")"
    [[ "${eol_record}" == *"i/lf"* \
        && "${eol_record}" == *"w/lf"* \
        && "${eol_record}" == *"attr/text eol=lf"* ]] \
        || die "script must be committed and checked out with LF endings: ${lf_script}"
    python3 - "${lf_script}" <<'PY'
from pathlib import Path
import sys

data = Path(sys.argv[1]).read_bytes()
if not data or b"\r" in data or not data.endswith(b"\n"):
    raise SystemExit(f"script is not normalized LF text: {sys.argv[1]}")
PY
done

for executable_script in \
        "tools/run-relocatable-world.sh" "${EVIDENCE_TOOL}" "${MACOS_VERIFIER}"; do
    [[ -x "${executable_script}" ]] \
        || die "script is not executable in the worktree: ${executable_script}"
    index_mode="$(git ls-files -s -- "${executable_script}" | awk '{print $1}')"
    [[ "${index_mode}" == 100755 ]] \
        || die "script is not committed executable: ${executable_script}"
done

bash -n tools/run-relocatable-world.sh
bash -n "${MACOS_VERIFIER}"
python3 - "${EVIDENCE_TOOL}" "${LOCK_VALIDATOR}" <<'PY'
from pathlib import Path
import sys

for filename in sys.argv[1:]:
    compile(Path(filename).read_text(encoding="utf-8"), filename, "exec")
PY

docker version >/dev/null
docker_architecture="$(docker info --format '{{.Architecture}}')" \
    || die "Docker Desktop is not available"
case "${docker_architecture}" in
    arm64|aarch64) ;;
    *) die "Docker Desktop is not running an ARM64 Linux VM: ${docker_architecture}" ;;
esac
docker buildx version >/dev/null \
    || die "Docker Buildx is unavailable"

developer_directory="$(xcode-select -p)" \
    || die "Xcode command-line tools are unavailable"
[[ -n "${developer_directory}" ]] \
    || die "xcode-select returned an empty developer directory"
xcodebuild -version >/dev/null
xcrun --sdk macosx --show-sdk-version >/dev/null
xcrun --sdk macosx --show-sdk-build-version >/dev/null
xcrun --sdk macosx --show-sdk-path >/dev/null
python3 --version >/dev/null
cmake --version >/dev/null
ninja --version >/dev/null

python3 "${LOCK_VALIDATOR}" \
    --sources "${SOURCES_LOCK}" --outputs "${OUTPUTS_LOCK}" \
    --recipe-root . >/dev/null

preflight_root="$(mktemp -d "${TMPDIR:-/tmp}/typelayout-local-preflight.XXXXXX")"
cleanup_preflight() {
    if [[ -n "${preflight_root-}" && -d "${preflight_root}" ]]; then
        rm -rf -- "${preflight_root}"
    fi
}
trap cleanup_preflight EXIT

policy_json="${preflight_root}/policies.json"
python3 - "${SOURCES_LOCK}" "${OUTPUTS_LOCK}" "${policy_json}" <<'PY'
import importlib.util
import json
from pathlib import Path
import sys

spec = importlib.util.spec_from_file_location(
    "relocatable_world_evidence", "tools/relocatable_world_evidence.py"
)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
sources, outputs, destination = sys.argv[1:]
output_lock = module.load_json(outputs)
specs = (
    ("x86_64_linux_gcc", "linux/amd64", "gcc", "emulated"),
    ("x86_64_linux_clang", "linux/amd64", "p2996", "emulated"),
    ("arm64_linux_gcc", "linux/arm64", "gcc", "native"),
    ("arm64_linux_clang", "linux/arm64", "p2996", "native"),
)
records = {}
for node, platform, toolchain, execution in specs:
    policy, sources_digest, outputs_digest = module.load_node_toolchain_policy(
        sources, outputs, node
    )
    image = output_lock["linux"][toolchain]
    records[node] = {
        **policy,
        "platform": platform,
        "toolchain": toolchain,
        "execution": execution,
        "repository": image["repository"],
        "index_digest": image["index_digest"],
        "manifest_digest": image["platforms"][platform]["manifest_digest"],
        "sources_sha256": sources_digest,
        "outputs_sha256": outputs_digest,
    }
policy, sources_digest, outputs_digest = module.load_node_toolchain_policy(
    sources, outputs, "arm64_macos_clang"
)
records["arm64_macos_clang"] = {
    **policy,
    "platform": "macos/arm64",
    "toolchain": "p2996",
    "execution": "native",
    "sources_sha256": sources_digest,
    "outputs_sha256": outputs_digest,
}
if tuple(records) != (
    "x86_64_linux_gcc",
    "x86_64_linux_clang",
    "arm64_linux_gcc",
    "arm64_linux_clang",
    "arm64_macos_clang",
):
    raise SystemExit("local launcher policy does not contain exactly five nodes")
with open(destination, "w", encoding="utf-8", newline="\n") as stream:
    json.dump(records, stream, indent=2, sort_keys=False)
    stream.write("\n")
PY

policy_field() {
    python3 - "${policy_json}" "$1" "$2" <<'PY'
import json
import sys

with open(sys.argv[1], encoding="utf-8") as stream:
    value = json.load(stream)[sys.argv[2]][sys.argv[3]]
if not isinstance(value, str) or not value or any(c in value for c in "\t\r\n"):
    raise SystemExit(f"unsafe policy field {sys.argv[2]}.{sys.argv[3]}")
print(value)
PY
}

resolve_linux_flags() {
    python3 - "${policy_json}" "$1" <<'PY'
import json
import sys

with open(sys.argv[1], encoding="utf-8") as stream:
    policy = json.load(stream)[sys.argv[2]]
flags = policy["flags"].replace("${TOOLCHAIN_ROOT}", "/opt/p2996-toolchain")
flags = flags.replace("${TARGET_TRIPLE}", policy["target"])
if "${" in flags or any(c in flags for c in "\t\r\n"):
    raise SystemExit("unexpanded or unsafe Linux compiler flags")
print(flags)
PY
}

oauth_response_user() {
    python3 - "$1" "$2" "${3-}" <<'PY'
import json
from pathlib import Path
import sys

headers = Path(sys.argv[1]).read_text(encoding="utf-8", errors="strict")
scopes = []
for line in headers.splitlines():
    name, separator, value = line.partition(":")
    if separator and name.strip().casefold() == "x-oauth-scopes":
        scopes.extend(scope.strip() for scope in value.split(","))
if "read:packages" not in scopes:
    raise SystemExit("GitHub token does not report read:packages scope")
body = json.loads(Path(sys.argv[2]).read_text(encoding="utf-8"))
login = body.get("login")
if not isinstance(login, str) or not login:
    raise SystemExit("GitHub user response has no login")
if sys.argv[3] and login.casefold() != sys.argv[3].casefold():
    raise SystemExit("TYPELAYOUT_GHCR_USER does not match token owner")
print(login)
PY
}

ghcr_user=
ghcr_token=
auth_headers="${preflight_root}/github-headers.txt"
auth_body="${preflight_root}/github-user.json"
if [[ -n "${explicit_user}" || -n "${explicit_token}" ]]; then
    [[ -n "${explicit_user}" && -n "${explicit_token}" ]] \
        || die "private GHCR credentials require both TYPELAYOUT_GHCR_USER and TYPELAYOUT_GHCR_TOKEN"
    [[ "${explicit_user}" =~ ^[A-Za-z0-9]([A-Za-z0-9-]{0,37}[A-Za-z0-9])?$ \
        && "${explicit_token}" =~ ^[A-Za-z0-9_]+$ ]] \
        || die "private GHCR credential value is unsafe"
    ghcr_user=${explicit_user}
    ghcr_token=${explicit_token}
    {
        printf 'silent\nshow-error\nfail\n'
        printf 'url = "https://api.github.com/user"\n'
        printf 'header = "Accept: application/vnd.github+json"\n'
        printf 'header = "Authorization: Bearer %s"\n' "${ghcr_token}"
    } | curl --config - --dump-header "${auth_headers}" --output "${auth_body}"
    oauth_response_user "${auth_headers}" "${auth_body}" "${ghcr_user}" >/dev/null
elif command -v gh >/dev/null 2>&1 \
        && gh auth status --hostname github.com >/dev/null 2>&1; then
    gh api -i user >"${auth_headers}"
    gh api user >"${auth_body}"
    ghcr_user="$(gh api user --jq .login)"
    oauth_response_user "${auth_headers}" "${auth_body}" "${ghcr_user}" >/dev/null
    ghcr_token="$(gh auth token --hostname github.com)"
    [[ -n "${ghcr_token}" ]] \
        || die "authenticated gh session returned an empty token"
else
    die "private GHCR credentials are unavailable; authenticate gh with read:packages or set TYPELAYOUT_GHCR_USER and TYPELAYOUT_GHCR_TOKEN"
fi
unset explicit_user explicit_token
printf '%s' "${ghcr_token}" \
    | docker login ghcr.io --username "${ghcr_user}" --password-stdin >/dev/null
unset ghcr_token

verify_remote_index() {
    node=$1
    repository="$(policy_field "${node}" repository)"
    index_digest="$(policy_field "${node}" index_digest)"
    toolchain="$(policy_field "${node}" toolchain)"
    raw_index="${preflight_root}/${toolchain}-index.json"
    if [[ ! -f "${raw_index}" ]]; then
        docker buildx imagetools inspect \
            "${repository}@${index_digest}" --raw >"${raw_index}"
        python3 - "${raw_index}" "${OUTPUTS_LOCK}" "${toolchain}" <<'PY'
import json
import sys

def unique(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise SystemExit(f"duplicate JSON key {key!r}")
        result[key] = value
    return result

with open(sys.argv[1], encoding="utf-8") as stream:
    index = json.load(stream, object_pairs_hook=unique)
with open(sys.argv[2], encoding="utf-8") as stream:
    lock = json.load(stream, object_pairs_hook=unique)
expected = {
    platform: record["manifest_digest"]
    for platform, record in lock["linux"][sys.argv[3]]["platforms"].items()
}
actual = {}
for descriptor in index.get("manifests", []):
    platform = descriptor.get("platform", {})
    key = f'{platform.get("os")}/{platform.get("architecture")}'
    if key in actual:
        raise SystemExit(f"duplicate remote manifest platform {key}")
    actual[key] = descriptor.get("digest")
if actual != expected:
    raise SystemExit(f"remote index differs from sealed lock: {actual!r}")
PY
    fi
}

for linux_spec in "${LINUX_NODE_SPECS[@]}"; do
    IFS='|' read -r node platform toolchain execution <<<"${linux_spec}"
    verify_remote_index "${node}"
    repository="$(policy_field "${node}" repository)"
    manifest_digest="$(policy_field "${node}" manifest_digest)"
    artifact_sha256="$(policy_field "${node}" toolchain_artifact_sha256)"
    [[ "${manifest_digest}" == "sha256:${artifact_sha256}" ]] \
        || die "manifest and artifact digest disagree for ${node}"
    docker pull "${repository}@${manifest_digest}" >/dev/null
done

run_amd64_smoke() {
    node=$1
    repository="$(policy_field "${node}" repository)"
    manifest_digest="$(policy_field "${node}" manifest_digest)"
    locked_flags="$(resolve_linux_flags "${node}")"
    compiler_family="$(policy_field "${node}" compiler_family)"
    compiler_revision="$(policy_field "${node}" compiler_revision)"
    compiler_version="$(policy_field "${node}" compiler_version)"
    compiler_target="$(policy_field "${node}" target)"
    compiler_stdlib="$(policy_field "${node}" stdlib)"
    image_ref="${repository}@${manifest_digest}"
    docker run --rm --platform linux/amd64 \
        -v "${repository_root}:/workspace:ro" \
        -e "TYPELAYOUT_SMOKE_NODE=${node}" \
        -e "LOCKED_FLAGS=${locked_flags}" \
        -e "COMPILER_FAMILY=${compiler_family}" \
        -e "COMPILER_REVISION=${compiler_revision}" \
        -e "COMPILER_VERSION=${compiler_version}" \
        -e "COMPILER_TARGET=${compiler_target}" \
        -e "COMPILER_STDLIB=${compiler_stdlib}" \
        "${image_ref}" bash -euo pipefail -c '
            test "$(uname -m)" = x86_64
            build_dir="$(mktemp -d /tmp/typelayout-amd64-smoke.XXXXXX)"
            trap '\''rm -rf -- "${build_dir}"'\'' EXIT
            cmake -S /workspace -B "${build_dir}" -G Ninja \
                -DCMAKE_BUILD_TYPE=Release \
                -DCMAKE_CXX_COMPILER="${CXX}" \
                -DCMAKE_CXX_FLAGS="${LOCKED_FLAGS}" \
                -DTYPELAYOUT_TOOLCHAIN_REVISION="${COMPILER_REVISION}"
            cmake --build "${build_dir}" \
                --target relocatable_world_platform_probe --parallel
            "${build_dir}/relocatable_world_platform_probe" \
                "${TYPELAYOUT_SMOKE_NODE}" "${build_dir}/probe.json" \
                --runner local-arm64-macos \
                --runner-image docker-desktop-linux/amd64 \
                --xcode-version none --xcode-build none \
                --sdk-version none --sdk-build none \
                --deployment-target none --sdk-locked true
            python3 - "${build_dir}/probe.json" <<'\''PY'\''
# BEGIN TASK7 AMD64 SMOKE VALIDATOR
import json
import os
import sys

with open(sys.argv[1], encoding="utf-8") as stream:
    record = json.load(stream)
expected_probe = {
    "char_bit": 8,
    "pointer_bits": 64,
    "endian": "little",
    "reflection": True,
    "memcpy_object_lifetime": True,
    "memcpy_array_lifetime": True,
}
expected_contracts = {
    "world": {
        "WorldSnapshot": True,
        "Entity": True,
        "EntityRelativePtr": True,
        "EntityIndexEntry": True,
    },
    "unit_handoff": {
        "UnitSnapshot": True,
        "Effect": True,
        "EffectRelativePtr": True,
        "AttributeEntry": True,
    },
}
if record.get("node") != os.environ["TYPELAYOUT_SMOKE_NODE"]:
    raise SystemExit("amd64 smoke node identity mismatch")
if (
    record.get("probe") != expected_probe
    or record.get("schema") != 2
    or record.get("contracts") != expected_contracts
):
    raise SystemExit("optimized amd64 platform probe failed")
compiler = record.get("compiler", {})
expected_compiler = {
    "family": os.environ["COMPILER_FAMILY"],
    "revision": os.environ["COMPILER_REVISION"],
    "version": os.environ["COMPILER_VERSION"],
    "target": os.environ["COMPILER_TARGET"],
    "stdlib": os.environ["COMPILER_STDLIB"],
}
for key, expected in expected_compiler.items():
    if compiler.get(key) != expected:
        raise SystemExit(f"amd64 smoke compiler {key} mismatch")
# END TASK7 AMD64 SMOKE VALIDATOR
PY
        '
}

run_amd64_smoke x86_64_linux_gcc
run_amd64_smoke x86_64_linux_clang

macos_producer_extract="${preflight_root}/macos-producer-toolchain"
macos_consumer_extract="${preflight_root}/macos-consumer-toolchain"
macos_producer_metadata="${preflight_root}/macos-producer-verification.json"
macos_consumer_metadata="${preflight_root}/macos-consumer-verification.json"
"${MACOS_VERIFIER}" \
    --sources "${SOURCES_LOCK}" --outputs "${OUTPUTS_LOCK}" \
    --node arm64_macos_clang --allow-unlocked-sdk \
    --extract-dir "${macos_producer_extract}" \
    --metadata-output "${macos_producer_metadata}"
"${MACOS_VERIFIER}" \
    --sources "${SOURCES_LOCK}" --outputs "${OUTPUTS_LOCK}" \
    --node arm64_macos_clang --allow-unlocked-sdk \
    --extract-dir "${macos_consumer_extract}" \
    --metadata-output "${macos_consumer_metadata}"

python3 - "${policy_json}" "${macos_producer_metadata}" \
        "${macos_consumer_metadata}" <<'PY'
import json
import sys

with open(sys.argv[1], encoding="utf-8") as stream:
    policy = json.load(stream)["arm64_macos_clang"]
records = []
for filename in sys.argv[2:]:
    with open(filename, encoding="utf-8") as stream:
        record = json.load(stream)
    expected_keys = {
        "schema", "node", "architecture", "archive_sha256",
        "compiler_revision", "compiler_version", "target", "stdlib",
        "xcode_version", "xcode_build", "sdk_version", "sdk_build",
        "deployment_target", "sdk_locked", "observed_runner", "environment",
        "flags",
    }
    if set(record) != expected_keys or record["schema"] != 1:
        raise SystemExit("macOS verifier metadata shape is not exact")
    if record["node"] != "arm64_macos_clang" or record["architecture"] != "arm64":
        raise SystemExit("macOS verifier returned the wrong node or architecture")
    for key in (
        "xcode_version", "xcode_build", "sdk_version", "sdk_build",
        "deployment_target",
    ):
        value = record[key]
        if not isinstance(value, str) or not value or any(
            character in value for character in "\t\r\n"
        ):
            raise SystemExit(f"macOS verifier identity field {key} is unsafe")
    comparisons = {
        "archive_sha256": "toolchain_artifact_sha256",
        "compiler_revision": "compiler_revision",
        "compiler_version": "compiler_version",
        "target": "target",
        "stdlib": "stdlib",
        "deployment_target": "deployment_target",
    }
    for actual, expected in comparisons.items():
        if record[actual] != policy[expected]:
            raise SystemExit(f"macOS verifier {actual} differs from sealed policy")
    if record["flags"] != policy["flags"]:
        raise SystemExit("macOS verifier flags differ from sealed policy")
    if type(record["sdk_locked"]) is not bool:
        raise SystemExit("macOS verifier sdk_locked must be boolean")
    if not isinstance(record["environment"], dict) or set(record["environment"]) != {
        "developer_dir", "sdkroot", "toolchain_root",
    }:
        raise SystemExit("macOS verifier environment shape is not exact")
    for key in ("developer_dir", "sdkroot", "toolchain_root"):
        value = record["environment"].get(key)
        if not isinstance(value, str) or not value or any(
            character in value for character in "\t\r\n"
        ):
            raise SystemExit(f"macOS verifier environment.{key} is unsafe")
    if not isinstance(record["observed_runner"], dict) or set(
        record["observed_runner"]
    ) != {"image_os", "image_version"}:
        raise SystemExit("macOS verifier observed_runner shape is not exact")
    if any(
        not isinstance(value, str) or any(
            character in value for character in "\t\r\n"
        )
        for value in record["observed_runner"].values()
    ):
        raise SystemExit("macOS verifier observed_runner value is unsafe")
    records.append(record)
actual_keys = (
    "xcode_version", "xcode_build", "sdk_version", "sdk_build",
    "deployment_target", "sdk_locked",
)
if any(records[0][key] != records[1][key] for key in actual_keys):
    raise SystemExit("producer and consumer macOS verification identities differ")
for key in ("developer_dir", "sdkroot"):
    if records[0]["environment"][key] != records[1]["environment"][key]:
        raise SystemExit("producer and consumer macOS environments differ")
PY

metadata_field() {
    python3 - "$1" "$2" <<'PY'
import json
import sys

with open(sys.argv[1], encoding="utf-8") as stream:
    value = json.load(stream)
for part in sys.argv[2].split("."):
    value = value[part]
if type(value) is bool:
    print("true" if value else "false")
elif isinstance(value, str) and value and not any(c in value for c in "\t\r\n"):
    print(value)
else:
    raise SystemExit(f"unsafe verifier metadata field {sys.argv[2]}")
PY
}

resolved_macos_flags() {
    python3 - "${policy_json}" "$1" <<'PY'
import json
import shlex
import sys

with open(sys.argv[1], encoding="utf-8") as stream:
    policy = json.load(stream)["arm64_macos_clang"]
with open(sys.argv[2], encoding="utf-8") as stream:
    metadata = json.load(stream)
environment = metadata["environment"]
replacements = {
    "${TOOLCHAIN_ROOT}": environment["toolchain_root"],
    "${SDKROOT}": environment["sdkroot"],
    "${TARGET_TRIPLE}": policy["target"],
}
resolved = []
for flag in shlex.split(metadata["flags"]):
    for placeholder, value in replacements.items():
        flag = flag.replace(placeholder, value)
    if "${" in flag or any(c in flag for c in "\t\r\n"):
        raise SystemExit("unexpanded or unsafe macOS compiler flags")
    resolved.append(flag)
if not resolved:
    raise SystemExit("macOS compiler flags are empty")
print(shlex.join(resolved))
PY
}

verify_macos_runtime() {
    local otool_libraries=$1
    local otool_load_commands=$2
    local dyld_libraries=$3
    local library_directory=$4
    python3 - "${otool_libraries}" "${otool_load_commands}" \
            "${dyld_libraries}" "${library_directory}" <<'PY'
# BEGIN TASK7 MACOS FINAL RUNTIME VALIDATOR
from pathlib import Path
import re
import sys


otool_libraries = Path(sys.argv[1])
otool_load_commands = Path(sys.argv[2])
dyld_libraries = Path(sys.argv[3])
library_directory = Path(sys.argv[4]).resolve()
runtime_name = re.compile(
    r"^(libc\+\+|libc\+\+abi|libunwind)(?:\.[^/]*)?\.dylib$"
)
dyld_record = re.compile(
    r"^dyld\[[0-9]+\]: "
    r"(?:<[0-9A-Fa-f]{8}(?:-[0-9A-Fa-f]{4}){3}-[0-9A-Fa-f]{12}> )?"
    r"(?P<path>.+)$"
)

expected_dependencies = {
    "libc++": {"@rpath/libc++.1.dylib"},
    "libc++abi": set(),
    "libunwind": set(),
}
observed_dependencies = {name: set() for name in expected_dependencies}
for line in otool_libraries.read_text(
    encoding="utf-8", errors="replace"
).splitlines()[1:]:
    candidate = line.strip().split(" (compatibility version", 1)[0]
    match = runtime_name.fullmatch(Path(candidate).name)
    if match:
        observed_dependencies[match.group(1)].add(candidate)
if observed_dependencies != expected_dependencies:
    raise SystemExit(
        "final executable runtime dependencies are not exact: "
        f"{observed_dependencies!r}"
    )

rpaths = []
load_commands = re.split(
    r"(?m)^Load command [0-9]+\s*$",
    otool_load_commands.read_text(encoding="utf-8", errors="replace"),
)
for command in load_commands:
    if not re.search(r"(?m)^\s*cmd LC_RPATH\s*$", command):
        continue
    paths = re.findall(
        r"(?m)^\s*path (.*?) \(offset [0-9]+\)\s*$", command
    )
    if len(paths) != 1:
        raise SystemExit(f"malformed LC_RPATH command: {command!r}")
    rpaths.append(paths[0])
expected_rpath = str(library_directory)
if rpaths != [expected_rpath]:
    raise SystemExit(
        "final executable rpaths do not contain only the archive library "
        f"directory: observed={rpaths!r}, expected={expected_rpath!r}"
    )

expected_loads = {
    "libc++": (library_directory / "libc++.1.dylib").resolve(),
    "libc++abi": (library_directory / "libc++abi.1.dylib").resolve(),
    "libunwind": (library_directory / "libunwind.1.dylib").resolve(),
}
for path in expected_loads.values():
    if not path.is_file():
        raise SystemExit(f"bundled runtime library is missing: {path}")
observed_loads = {name: set() for name in expected_loads}
for line in dyld_libraries.read_text(
    encoding="utf-8", errors="replace"
).splitlines():
    if not line.startswith("dyld["):
        continue
    record_match = dyld_record.fullmatch(line)
    if record_match is None:
        raise SystemExit(f"malformed dyld runtime record: {line!r}")
    candidate = record_match.group("path")
    if not candidate.startswith("/") and not Path(candidate).is_absolute():
        raise SystemExit(f"malformed dyld runtime record: {line!r}")
    match = runtime_name.fullmatch(Path(candidate).name)
    if match:
        observed_loads[match.group(1)].add(Path(candidate).resolve())
for name, expected_path in expected_loads.items():
    if observed_loads[name] != {expected_path}:
        raise SystemExit(
            f"runtime {name} did not load only from the archive: "
            f"observed={sorted(map(str, observed_loads[name]))!r}, "
            f"expected={expected_path}"
        )
# END TASK7 MACOS FINAL RUNTIME VALIDATOR
PY
}

verify_implementation_state
mkdir -p "$(dirname -- "${output_directory}")"
mkdir "${output_directory}"
printf '%s\n' "${source_sha}" >"${output_directory}/source-sha.txt.tmp"
mv "${output_directory}/source-sha.txt.tmp" "${output_directory}/source-sha.txt"
printf '%s\n' "${run_id}" >"${output_directory}/run-id.txt.tmp"
mv "${output_directory}/run-id.txt.tmp" "${output_directory}/run-id.txt"
for node in "${LOCAL_NODES[@]}"; do
    python3 "${EVIDENCE_TOOL}" fallback-provenance \
        --node "${node}" --reason "local producer did not complete" \
        --output "${output_directory}/${node}.provenance.json"
    python3 "${EVIDENCE_TOOL}" fallback-results \
        --profile local-arm64-macos --consumer "${node}" \
        --reason "local consumer did not complete" \
        --output "${output_directory}/${node}.results.json"
done
python3 "${EVIDENCE_TOOL}" fallback-agreements \
    --profile local-arm64-macos --reason "local Agreement did not complete" \
    --output "${output_directory}/agreements.json"
python3 "${EVIDENCE_TOOL}" fallback-closure \
    --profile local-arm64-macos --reason "local closure did not complete" \
    --output "${output_directory}/closure.json"

seal_producer_bundle() {
    node=$1
    execution=$2
    runner=$3
    artifact_sha256="$(policy_field "${node}" toolchain_artifact_sha256)"
    probe="${output_directory}/.${node}.producer.probe.json"
    facts="${output_directory}/${node}.producer-facts.json"
    seal=(
        python3 "${EVIDENCE_TOOL}" seal-producer
        --node "${node}" --profile local-arm64-macos --execution "${execution}"
        --probe "${probe}" --facts "${facts}"
        --sources-lock "${SOURCES_LOCK}" --outputs-lock "${OUTPUTS_LOCK}"
        --runner "${runner}" --source-sha "${source_sha}"
        --workflow-run "${run_id}"
        --toolchain-artifact-sha256 "${artifact_sha256}"
        --output "${output_directory}/${node}.provenance.json"
    )
    if [[ -f "${output_directory}/${node}.world.region" &&
          -f "${output_directory}/${node}.unit.region" ]]; then
        seal+=(
            --signature "${output_directory}/${node}.sig.hpp"
            --world-region "${output_directory}/${node}.world.region"
            --unit-region "${output_directory}/${node}.unit.region"
        )
    fi
    "${seal[@]}"
    rm -f -- "${probe}" "${facts}"
    python3 "${EVIDENCE_TOOL}" validate-provenance \
        "${output_directory}/${node}.provenance.json" >/dev/null
}

run_linux_producer() {
    node=$1
    platform=$2
    execution=$3
    repository="$(policy_field "${node}" repository)"
    manifest_digest="$(policy_field "${node}" manifest_digest)"
    locked_flags="$(resolve_linux_flags "${node}")"
    compiler_revision="$(policy_field "${node}" compiler_revision)"
    image_ref="${repository}@${manifest_digest}"
    docker run --rm --platform "${platform}" \
        -v "${repository_root}:/workspace:ro" \
        -v "${output_directory}:/artifacts" \
        -e "NODE=${node}" -e "PLATFORM=${platform}" \
        -e "LOCKED_FLAGS=${locked_flags}" \
        -e "COMPILER_REVISION=${compiler_revision}" \
        "${image_ref}" bash -euo pipefail -c '
            case "${PLATFORM}" in
                linux/amd64) test "$(uname -m)" = x86_64 ;;
                linux/arm64) case "$(uname -m)" in arm64|aarch64) ;; *) exit 41 ;; esac ;;
                *) exit 42 ;;
            esac
            build_dir="$(mktemp -d /tmp/typelayout-producer.XXXXXX)"
            trap '\''rm -rf -- "${build_dir}"'\'' EXIT
            cmake -S /workspace -B "${build_dir}" -G Ninja \
                -DCMAKE_BUILD_TYPE=Release \
                -DCMAKE_CXX_COMPILER="${CXX}" \
                -DCMAKE_CXX_FLAGS="${LOCKED_FLAGS}" \
                -DTYPELAYOUT_TOOLCHAIN_REVISION="${COMPILER_REVISION}"
            cmake --build "${build_dir}" \
                --target relocatable_world_platform_probe relocatable_world_producer \
                --parallel
            "${build_dir}/relocatable_world_platform_probe" \
                "${NODE}" "/artifacts/.${NODE}.producer.probe.json" \
                --runner local-arm64-macos \
                --runner-image "docker-desktop-${PLATFORM}" \
                --xcode-version none --xcode-build none \
                --sdk-version none --sdk-build none \
                --deployment-target none --sdk-locked true
            "${build_dir}/relocatable_world_producer" "${NODE}" /artifacts
            if test -f "/artifacts/${NODE}.world.region" || \
                test -f "/artifacts/${NODE}.unit.region"; then
                cmake --build "${build_dir}" \
                    --target relocatable_world_export_ok --parallel
                "${build_dir}/relocatable_world_export_ok" /artifacts "${NODE}"
            fi
        '
    seal_producer_bundle "${node}" "${execution}" "${LOCAL_RUNNER}"
}

for linux_spec in "${LINUX_NODE_SPECS[@]}"; do
    IFS='|' read -r node platform toolchain execution <<<"${linux_spec}"
    run_linux_producer "${node}" "${platform}" "${execution}"
done

run_macos_producer() {
    metadata=$1
    toolchain_root="$(metadata_field "${metadata}" environment.toolchain_root)"
    developer_dir="$(metadata_field "${metadata}" environment.developer_dir)"
    sdkroot="$(metadata_field "${metadata}" environment.sdkroot)"
    architecture="$(metadata_field "${metadata}" architecture)"
    compiler_revision="$(metadata_field "${metadata}" compiler_revision)"
    compiler_target="$(metadata_field "${metadata}" target)"
    xcode_version="$(metadata_field "${metadata}" xcode_version)"
    xcode_build="$(metadata_field "${metadata}" xcode_build)"
    sdk_version="$(metadata_field "${metadata}" sdk_version)"
    sdk_build="$(metadata_field "${metadata}" sdk_build)"
    deployment_target="$(metadata_field "${metadata}" deployment_target)"
    sdk_locked="$(metadata_field "${metadata}" sdk_locked)"
    locked_flags="$(resolved_macos_flags "${metadata}")"
    runner_image="macos-$(sw_vers -productVersion)-$(sw_vers -buildVersion)"
    build_dir="$(mktemp -d "${preflight_root}/macos-producer-build.XXXXXX")"
    probe="${output_directory}/.arm64_macos_clang.producer.probe.json"
    export DEVELOPER_DIR="${developer_dir}"
    export SDKROOT="${sdkroot}"
    cmake -S "${repository_root}" -B "${build_dir}" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_COMPILER="${toolchain_root}/bin/clang++" \
        -DCMAKE_CXX_COMPILER_TARGET="${compiler_target}" \
        -DCMAKE_OSX_ARCHITECTURES="${architecture}" \
        -DCMAKE_OSX_SYSROOT="${sdkroot}" \
        -DCMAKE_OSX_DEPLOYMENT_TARGET="${deployment_target}" \
        -DCMAKE_CXX_FLAGS="${locked_flags}" \
        -DTYPELAYOUT_TOOLCHAIN_REVISION="${compiler_revision}"
    cmake --build "${build_dir}" \
        --target relocatable_world_platform_probe relocatable_world_producer \
        --parallel
    "${build_dir}/relocatable_world_platform_probe" \
        arm64_macos_clang "${probe}" \
        --runner "${LOCAL_RUNNER}" --runner-image "${runner_image}" \
        --xcode-version "${xcode_version}" --xcode-build "${xcode_build}" \
        --sdk-version "${sdk_version}" --sdk-build "${sdk_build}" \
        --deployment-target "${deployment_target}" --sdk-locked "${sdk_locked}"
    producer="${build_dir}/relocatable_world_producer"
    otool -L "${producer}" >"${preflight_root}/macos-producer.otool-L"
    otool -l "${producer}" >"${preflight_root}/macos-producer.otool-l"
    DYLD_PRINT_LIBRARIES=1 "${producer}" arm64_macos_clang \
        "${output_directory}" 2>"${preflight_root}/macos-producer.dyld"
    verify_macos_runtime \
        "${preflight_root}/macos-producer.otool-L" \
        "${preflight_root}/macos-producer.otool-l" \
        "${preflight_root}/macos-producer.dyld" \
        "${toolchain_root}/lib"
    if [[ -f "${output_directory}/arm64_macos_clang.world.region" ||
          -f "${output_directory}/arm64_macos_clang.unit.region" ]]; then
        cmake --build "${build_dir}" \
            --target relocatable_world_export_ok --parallel
        "${build_dir}/relocatable_world_export_ok" \
            "${output_directory}" arm64_macos_clang
    fi
    seal_producer_bundle arm64_macos_clang native "${LOCAL_RUNNER}"
}

run_macos_producer "${macos_producer_metadata}"

agreement_generated="${preflight_root}/agreement-generated"
agreement_binary="${preflight_root}/agreement-check"
mkdir "${agreement_generated}"
python3 "${EVIDENCE_TOOL}" prepare-agreements \
    --profile local-arm64-macos --evidence "${output_directory}" \
    --expect-source-sha "${source_sha}" --expect-workflow-run "${run_id}" \
    --sources-lock "${SOURCES_LOCK}" --outputs-lock "${OUTPUTS_LOCK}" \
    --output-header "${agreement_generated}/relocatable_world_agreement_input.hpp"
c++ -std=c++20 -O2 \
    -I "${repository_root}/example/relocatable_world_demo" \
    -I "${agreement_generated}" \
    "${repository_root}/example/relocatable_world_demo/agreement_check.cpp" \
    -o "${agreement_binary}"
"${agreement_binary}" "${output_directory}/agreements.json"
python3 - "${EVIDENCE_TOOL}" "${output_directory}/agreements.json" <<'PY'
import importlib.util
import sys

spec = importlib.util.spec_from_file_location("evidence", sys.argv[1])
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
record = module.validate_agreements(sys.argv[2])
decisions = [
    decision
    for pair in record["pairs"]
    for scenario in ("world", "unit_handoff")
    for decision in pair["scenarios"][scenario]
]
if len(record["pairs"]) != 10 or len(decisions) != 80:
    raise SystemExit("local Agreement count mismatch")
if any(decision["status"] != "PERMIT" for decision in decisions):
    raise SystemExit("local Agreement contains a non-PERMIT decision")
PY

agreement_matrix_input="${preflight_root}/validated-agreement/agreements.json"
python3 - "${output_directory}/agreements.json" "${agreement_matrix_input}" <<'PY'
# BEGIN TASK7 AGREEMENT BYTE COPY
from pathlib import Path
import sys

source = Path(sys.argv[1])
destination = Path(sys.argv[2])
original = source.read_bytes()
destination.parent.mkdir(parents=True)
temporary = destination.with_name(f".{destination.name}.tmp")
temporary.write_bytes(original)
temporary.replace(destination)
if destination.read_bytes() != original:
    raise SystemExit("validated Agreement byte copy differs from retained artifact")
# END TASK7 AGREEMENT BYTE COPY
PY

run_linux_consumer() {
    node=$1
    platform=$2
    repository="$(policy_field "${node}" repository)"
    manifest_digest="$(policy_field "${node}" manifest_digest)"
    artifact_sha256="$(policy_field "${node}" toolchain_artifact_sha256)"
    locked_flags="$(resolve_linux_flags "${node}")"
    compiler_revision="$(policy_field "${node}" compiler_revision)"
    image_ref="${repository}@${manifest_digest}"
    docker run --rm --platform "${platform}" \
        -v "${repository_root}:/workspace:ro" \
        -v "${output_directory}:/artifacts" \
        -e "NODE=${node}" -e "PLATFORM=${platform}" \
        -e "LOCKED_FLAGS=${locked_flags}" \
        -e "COMPILER_REVISION=${compiler_revision}" \
        -e "ARTIFACT_SHA256=${artifact_sha256}" \
        -e "SOURCE_SHA=${source_sha}" -e "LOCAL_RUN_ID=${run_id}" \
        "${image_ref}" bash -euo pipefail -c '
            case "${PLATFORM}" in
                linux/amd64) test "$(uname -m)" = x86_64 ;;
                linux/arm64) case "$(uname -m)" in arm64|aarch64) ;; *) exit 51 ;; esac ;;
                *) exit 52 ;;
            esac
            build_dir="$(mktemp -d /tmp/typelayout-consumer.XXXXXX)"
            trap '\''rm -rf -- "${build_dir}"'\'' EXIT
            probe="${build_dir}/consumer.probe.json"
            cmake -S /workspace -B "${build_dir}" -G Ninja \
                -DCMAKE_BUILD_TYPE=Release \
                -DCMAKE_CXX_COMPILER="${CXX}" \
                -DCMAKE_CXX_FLAGS="${LOCKED_FLAGS}" \
                -DTYPELAYOUT_TOOLCHAIN_REVISION="${COMPILER_REVISION}" \
                -DTYPELAYOUT_RELOCATABLE_WORLD_EVIDENCE_MODE=production \
                -DTYPELAYOUT_RELOCATABLE_WORLD_PROFILE=local-arm64-macos \
                -DTYPELAYOUT_RELOCATABLE_WORLD_CONSUMER_NODE="${NODE}" \
                -DTYPELAYOUT_RELOCATABLE_WORLD_EVIDENCE_DIR=/artifacts \
                -DTYPELAYOUT_RELOCATABLE_WORLD_CONSUMER_PROBE="${probe}" \
                -DTYPELAYOUT_RELOCATABLE_WORLD_TOOLCHAIN_ARTIFACT_SHA256="${ARTIFACT_SHA256}" \
                -DTYPELAYOUT_RELOCATABLE_WORLD_SOURCE_SHA="${SOURCE_SHA}" \
                -DTYPELAYOUT_RELOCATABLE_WORLD_WORKFLOW_RUN="${LOCAL_RUN_ID}" \
                -DTYPELAYOUT_RELOCATABLE_WORLD_SOURCES_LOCK=/workspace/.github/docker/toolchain-sources.lock \
                -DTYPELAYOUT_RELOCATABLE_WORLD_OUTPUTS_LOCK=/workspace/.github/docker/toolchains.lock
            cmake --build "${build_dir}" \
                --target relocatable_world_platform_probe --parallel
            "${build_dir}/relocatable_world_platform_probe" \
                "${NODE}" "${probe}" \
                --runner local-arm64-macos \
                --runner-image "docker-desktop-${PLATFORM}" \
                --xcode-version none --xcode-build none \
                --sdk-version none --sdk-build none \
                --deployment-target none --sdk-locked true
            cmake --build "${build_dir}" \
                --target relocatable_world_consumer --parallel
            "${build_dir}/relocatable_world_consumer" \
                local-arm64-macos "${NODE}" /artifacts \
                "/artifacts/${NODE}.results.json"
        '
    python3 "${EVIDENCE_TOOL}" validate-results \
        "${output_directory}/${node}.results.json" >/dev/null
}

for linux_spec in "${LINUX_NODE_SPECS[@]}"; do
    IFS='|' read -r node platform toolchain execution <<<"${linux_spec}"
    run_linux_consumer "${node}" "${platform}"
done

run_macos_consumer() {
    metadata=$1
    toolchain_root="$(metadata_field "${metadata}" environment.toolchain_root)"
    developer_dir="$(metadata_field "${metadata}" environment.developer_dir)"
    sdkroot="$(metadata_field "${metadata}" environment.sdkroot)"
    architecture="$(metadata_field "${metadata}" architecture)"
    compiler_revision="$(metadata_field "${metadata}" compiler_revision)"
    compiler_target="$(metadata_field "${metadata}" target)"
    artifact_sha256="$(metadata_field "${metadata}" archive_sha256)"
    xcode_version="$(metadata_field "${metadata}" xcode_version)"
    xcode_build="$(metadata_field "${metadata}" xcode_build)"
    sdk_version="$(metadata_field "${metadata}" sdk_version)"
    sdk_build="$(metadata_field "${metadata}" sdk_build)"
    deployment_target="$(metadata_field "${metadata}" deployment_target)"
    sdk_locked="$(metadata_field "${metadata}" sdk_locked)"
    locked_flags="$(resolved_macos_flags "${metadata}")"
    runner_image="macos-$(sw_vers -productVersion)-$(sw_vers -buildVersion)"
    build_dir="$(mktemp -d "${preflight_root}/macos-consumer-build.XXXXXX")"
    probe="${build_dir}/consumer.probe.json"
    export DEVELOPER_DIR="${developer_dir}"
    export SDKROOT="${sdkroot}"
    cmake -S "${repository_root}" -B "${build_dir}" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_COMPILER="${toolchain_root}/bin/clang++" \
        -DCMAKE_CXX_COMPILER_TARGET="${compiler_target}" \
        -DCMAKE_OSX_ARCHITECTURES="${architecture}" \
        -DCMAKE_OSX_SYSROOT="${sdkroot}" \
        -DCMAKE_OSX_DEPLOYMENT_TARGET="${deployment_target}" \
        -DCMAKE_CXX_FLAGS="${locked_flags}" \
        -DTYPELAYOUT_TOOLCHAIN_REVISION="${compiler_revision}" \
        -DTYPELAYOUT_RELOCATABLE_WORLD_EVIDENCE_MODE=production \
        -DTYPELAYOUT_RELOCATABLE_WORLD_PROFILE=local-arm64-macos \
        -DTYPELAYOUT_RELOCATABLE_WORLD_CONSUMER_NODE=arm64_macos_clang \
        -DTYPELAYOUT_RELOCATABLE_WORLD_EVIDENCE_DIR="${output_directory}" \
        -DTYPELAYOUT_RELOCATABLE_WORLD_CONSUMER_PROBE="${probe}" \
        -DTYPELAYOUT_RELOCATABLE_WORLD_TOOLCHAIN_ARTIFACT_SHA256="${artifact_sha256}" \
        -DTYPELAYOUT_RELOCATABLE_WORLD_SOURCE_SHA="${source_sha}" \
        -DTYPELAYOUT_RELOCATABLE_WORLD_WORKFLOW_RUN="${run_id}" \
        -DTYPELAYOUT_RELOCATABLE_WORLD_SOURCES_LOCK="${repository_root}/${SOURCES_LOCK}" \
        -DTYPELAYOUT_RELOCATABLE_WORLD_OUTPUTS_LOCK="${repository_root}/${OUTPUTS_LOCK}"
    cmake --build "${build_dir}" \
        --target relocatable_world_platform_probe --parallel
    "${build_dir}/relocatable_world_platform_probe" \
        arm64_macos_clang "${probe}" \
        --runner "${LOCAL_RUNNER}" --runner-image "${runner_image}" \
        --xcode-version "${xcode_version}" --xcode-build "${xcode_build}" \
        --sdk-version "${sdk_version}" --sdk-build "${sdk_build}" \
        --deployment-target "${deployment_target}" --sdk-locked "${sdk_locked}"
    cmake --build "${build_dir}" \
        --target relocatable_world_consumer --parallel
    consumer="${build_dir}/relocatable_world_consumer"
    otool -L "${consumer}" >"${preflight_root}/macos-consumer.otool-L"
    otool -l "${consumer}" >"${preflight_root}/macos-consumer.otool-l"
    DYLD_PRINT_LIBRARIES=1 "${consumer}" \
        local-arm64-macos arm64_macos_clang "${output_directory}" \
        "${output_directory}/arm64_macos_clang.results.json" \
        2>"${preflight_root}/macos-consumer.dyld"
    verify_macos_runtime \
        "${preflight_root}/macos-consumer.otool-L" \
        "${preflight_root}/macos-consumer.otool-l" \
        "${preflight_root}/macos-consumer.dyld" \
        "${toolchain_root}/lib"
    python3 "${EVIDENCE_TOOL}" validate-results \
        "${output_directory}/arm64_macos_clang.results.json" >/dev/null
}

run_macos_consumer "${macos_consumer_metadata}"

matrix_generated="${preflight_root}/matrix-generated"
matrix_binary="${preflight_root}/matrix-check"
mkdir "${matrix_generated}"
python3 "${EVIDENCE_TOOL}" prepare-matrix \
    --profile local-arm64-macos \
    --evidence "${output_directory}" --results "${output_directory}" \
    --agreements "${agreement_matrix_input}" \
    --expect-source-sha "${source_sha}" --expect-workflow-run "${run_id}" \
    --sources-lock "${SOURCES_LOCK}" --outputs-lock "${OUTPUTS_LOCK}" \
    --output-header "${matrix_generated}/relocatable_world_matrix_input.hpp"
c++ -std=c++20 -O2 \
    -I "${repository_root}/example/relocatable_world_demo" \
    -I "${matrix_generated}" \
    "${repository_root}/example/relocatable_world_demo/matrix_check.cpp" \
    -o "${matrix_binary}"
"${matrix_binary}" "${output_directory}/closure.json"

python3 "${EVIDENCE_TOOL}" audit-run \
    --directory "${output_directory}" \
    --expect-source-sha "${source_sha}" --expect-workflow-run "${run_id}" \
    --sources-lock "${SOURCES_LOCK}" --outputs-lock "${OUTPUTS_LOCK}" \
    --expect-nodes 5 --expect-pairs 10 \
    --expect-named-permits 40 --expect-transfers 20 >/dev/null

verify_implementation_state
printf '%s\n' "${FINAL_LINE}"
