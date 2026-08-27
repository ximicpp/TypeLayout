#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
Usage:
  verify-p2996-toolchain.sh --sources PATH --outputs PATH --node NODE \
      (--require-locked-sdk | --allow-unlocked-sdk) [options]
  verify-p2996-toolchain.sh --sources PATH --node NODE \
      --candidate-archive PATH --candidate-sha256 HEX \
      (--require-locked-sdk | --allow-unlocked-sdk) [options]

Options:
  --candidate-archive PATH  Verify a local, unpublished candidate archive.
  --candidate-sha256 HEX    Expected SHA256 for the local candidate archive.
  --require-locked-sdk      Require the exact locked Xcode and macOS SDK.
  --allow-unlocked-sdk      Permit a local SDK mismatch and report sdk_locked=false.
  --extract-dir DIR         Keep the verified extracted toolchain in DIR.
  --metadata-output PATH    Write verification facts as JSON.
EOF
}

sources=""
outputs=""
node=""
candidate_archive=""
candidate_sha256=""
sdk_mode=""
extract_dir=""
metadata_output=""
while [[ $# -gt 0 ]]; do
    case "$1" in
        --sources) sources="${2-}"; shift 2 ;;
        --outputs) outputs="${2-}"; shift 2 ;;
        --node) node="${2-}"; shift 2 ;;
        --candidate-archive) candidate_archive="${2-}"; shift 2 ;;
        --candidate-sha256) candidate_sha256="${2-}"; shift 2 ;;
        --require-locked-sdk)
            [[ -z "${sdk_mode}" ]] || { echo "select one SDK mode" >&2; exit 2; }
            sdk_mode=require
            shift
            ;;
        --allow-unlocked-sdk)
            [[ -z "${sdk_mode}" ]] || { echo "select one SDK mode" >&2; exit 2; }
            sdk_mode=allow
            shift
            ;;
        --extract-dir) extract_dir="${2-}"; shift 2 ;;
        --metadata-output) metadata_output="${2-}"; shift 2 ;;
        --help|-h) usage; exit 0 ;;
        *) echo "unknown option: $1" >&2; usage >&2; exit 2 ;;
    esac
done

if [[ -z "${sources}" || -z "${node}" || -z "${sdk_mode}" ]]; then
    usage >&2
    exit 2
fi
if [[ -n "${outputs}" && -n "${candidate_archive}" ]]; then
    echo "--outputs and --candidate-archive are mutually exclusive" >&2
    exit 2
fi
if [[ -z "${outputs}" && -z "${candidate_archive}" ]]; then
    echo "provide --outputs or --candidate-archive" >&2
    exit 2
fi
if [[ -n "${candidate_archive}" && -z "${candidate_sha256}" ]]; then
    echo "--candidate-archive requires --candidate-sha256" >&2
    exit 2
fi
if [[ -n "${candidate_sha256}" && -z "${candidate_archive}" ]]; then
    echo "--candidate-sha256 requires --candidate-archive" >&2
    exit 2
fi
if [[ -n "${candidate_sha256}" && ! "${candidate_sha256}" =~ ^[0-9a-f]{64}$ ]]; then
    echo "candidate SHA256 must be 64 lowercase hexadecimal characters" >&2
    exit 2
fi

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repository_root="$(cd "${script_dir}/../.." && pwd)"
sources="$(cd "$(dirname "${sources}")" && pwd)/$(basename "${sources}")"
if [[ -n "${outputs}" ]]; then
    outputs="$(cd "$(dirname "${outputs}")" && pwd)/$(basename "${outputs}")"
fi

for command in python3 xcodebuild xcode-select xcrun tar zstd shasum otool; do
    command -v "${command}" >/dev/null 2>&1 || {
        echo "required command is missing: ${command}" >&2
        exit 1
    }
done

validator=(python3 "${script_dir}/validate-toolchain-locks.py"
    --sources "${sources}" --recipe-root "${repository_root}")
if [[ -n "${outputs}" ]]; then
    validator+=(--outputs "${outputs}")
fi
"${validator[@]}" >/dev/null

lock_values="$(python3 - "${sources}" "${outputs}" "${node}" <<'PY'
import json
import sys

with open(sys.argv[1], encoding="utf-8") as source:
    sources = json.load(source)
node = sys.argv[3]
try:
    record = sources["macos"]["nodes"][node]
except KeyError as error:
    raise SystemExit(f"unknown macOS node: {node}") from error
url = ""
archive_sha256 = ""
if sys.argv[2]:
    with open(sys.argv[2], encoding="utf-8") as source:
        output = json.load(source)["macos"][node]
    url = output["url"]
    archive_sha256 = output["archive_sha256"]
values = (
    record["runner"],
    record["architecture"],
    record["xcode_version"],
    record["xcode_build"],
    record["sdk_version"],
    record["sdk_build"],
    record["deployment_target"],
    record["flags"],
    sources["p2996"]["commit"],
    url,
    archive_sha256,
)
print("\t".join(values))
PY
)"
IFS=$'\t' read -r runner architecture locked_xcode locked_xcode_build \
    locked_sdk locked_sdk_build deployment_target locked_flags compiler_revision \
    archive_url locked_archive_sha256 <<<"${lock_values}"

if [[ "$(uname -m)" != "${architecture}" ]]; then
    echo "node ${node} requires native host architecture ${architecture}" >&2
    exit 1
fi

temporary_root="$(mktemp -d "${TMPDIR:-/tmp}/typelayout-p2996-verify.XXXXXX")"
cleanup() {
    rm -rf "${temporary_root}"
}
trap cleanup EXIT

if [[ -n "${outputs}" ]]; then
    command -v curl >/dev/null 2>&1 || {
        echo "required command is missing: curl" >&2
        exit 1
    }
    archive="${temporary_root}/toolchain.tar.zst"
    curl --fail --location --proto '=https' --tlsv1.2 --retry 3 \
        --output "${archive}" "${archive_url}"
    expected_sha256="${locked_archive_sha256}"
else
    archive="$(cd "$(dirname "${candidate_archive}")" && pwd)/$(basename "${candidate_archive}")"
    expected_sha256="${candidate_sha256}"
fi

actual_sha256="$(shasum -a 256 "${archive}" | awk '{print $1}')"
if [[ "${actual_sha256}" != "${expected_sha256}" ]]; then
    echo "archive SHA256 mismatch: ${actual_sha256} != ${expected_sha256}" >&2
    exit 1
fi

listing="${temporary_root}/archive.list"
zstd -dc "${archive}" | tar -tf - >"${listing}"
python3 - "${listing}" <<'PY'
from pathlib import PurePosixPath
import sys

members = open(sys.argv[1], encoding="utf-8", errors="strict").read().splitlines()
if not members:
    raise SystemExit("toolchain archive is empty")
for member in members:
    path = PurePosixPath(member)
    if path.is_absolute() or ".." in path.parts or not path.parts:
        raise SystemExit(f"unsafe archive member: {member!r}")
    if path.parts[0] != "p2996-toolchain":
        raise SystemExit(f"unexpected archive root: {member!r}")
PY

if [[ -n "${extract_dir}" ]]; then
    mkdir -p "${extract_dir}"
    extract_dir="$(cd "${extract_dir}" && pwd)"
    if find "${extract_dir}" -mindepth 1 -print -quit | grep -q .; then
        echo "--extract-dir must be empty: ${extract_dir}" >&2
        exit 1
    fi
else
    extract_dir="${temporary_root}/extracted"
    mkdir -p "${extract_dir}"
fi
zstd -dc "${archive}" | tar -xf - -C "${extract_dir}"
toolchain_root="${extract_dir}/p2996-toolchain"
if find -L "${toolchain_root}" -type l -print -quit | grep -q .; then
    echo "archive contains a broken symbolic link" >&2
    exit 1
fi
cxx="${toolchain_root}/bin/clang++"
library_dir="${toolchain_root}/lib"
include_dir="${toolchain_root}/include/c++/v1"
[[ -x "${cxx}" && -d "${include_dir}" && -d "${library_dir}" ]] || {
    echo "archive is missing clang++ or bundled libc++" >&2
    exit 1
}

locked_developer_dir="/Applications/Xcode_${locked_xcode}.app/Contents/Developer"
if [[ -d "${locked_developer_dir}" ]]; then
    developer_dir="${locked_developer_dir}"
elif [[ "${sdk_mode}" == require ]]; then
    echo "locked Xcode is missing: ${locked_developer_dir}" >&2
    exit 1
else
    developer_dir="$(xcode-select -p)"
fi
export DEVELOPER_DIR="${developer_dir}"
actual_xcode="$(xcodebuild -version | sed -n '1s/^Xcode //p')"
actual_xcode_build="$(xcodebuild -version | sed -n '2s/^Build version //p')"
actual_sdk="$(xcrun --sdk macosx --show-sdk-version)"
actual_sdk_build="$(xcrun --sdk macosx --show-sdk-build-version)"
sdkroot="$(xcrun --sdk macosx --show-sdk-path)"
sdk_locked=false
if [[ "${actual_xcode}" == "${locked_xcode}" \
    && "${actual_xcode_build}" == "${locked_xcode_build}" \
    && "${actual_sdk}" == "${locked_sdk}" \
    && "${actual_sdk_build}" == "${locked_sdk_build}" ]]; then
    sdk_locked=true
fi
if [[ "${sdk_mode}" == require && "${sdk_locked}" != true ]]; then
    echo "Apple toolchain mismatch: Xcode ${actual_xcode}/${actual_xcode_build}, " \
         "SDK ${actual_sdk}/${actual_sdk_build}; expected Xcode " \
         "${locked_xcode}/${locked_xcode_build}, SDK " \
         "${locked_sdk}/${locked_sdk_build}" >&2
    exit 1
fi

case "${architecture}" in
    arm64) target="arm64-apple-macosx${deployment_target}.0" ;;
    x86_64) target="x86_64-apple-macosx${deployment_target}.0" ;;
    *) echo "unsupported locked architecture: ${architecture}" >&2; exit 1 ;;
esac

effective_flags=(--target="${target}")
while IFS= read -r -d '' flag; do
    effective_flags+=("${flag}")
done < <(python3 - "${locked_flags}" "${toolchain_root}" "${sdkroot}" "${target}" <<'PY'
import shlex
import sys

flags = sys.argv[1]
replacements = {
    "${TOOLCHAIN_ROOT}": sys.argv[2],
    "${SDKROOT}": sys.argv[3],
    "${TARGET_TRIPLE}": sys.argv[4],
}
for placeholder, value in replacements.items():
    flags = flags.replace(placeholder, value)
if "${" in flags:
    raise SystemExit(f"unexpanded toolchain flag placeholder: {flags}")
for flag in shlex.split(flags):
    sys.stdout.buffer.write(flag.encode("utf-8") + b"\0")
PY
)
(( ${#effective_flags[@]} > 1 )) || {
    echo "locked compiler flags expanded to an empty list" >&2
    exit 1
}

reported_target="$("${cxx}" --target="${target}" -dumpmachine)"
if [[ "${reported_target}" != "${target}" ]]; then
    echo "compiler target mismatch: ${reported_target} != ${target}" >&2
    exit 1
fi

include_trace="${temporary_root}/include-search.txt"
printf '#include <vector>\n' \
    | "${cxx}" "${effective_flags[@]}" -E -x c++ -v - >/dev/null \
        2>"${include_trace}"
python3 - "${include_trace}" "${include_dir}" <<'PY'
from pathlib import Path
import sys

lines = Path(sys.argv[1]).read_text(encoding="utf-8", errors="replace").splitlines()
expected = str(Path(sys.argv[2]).resolve())
try:
    begin = lines.index('#include <...> search starts here:') + 1
    end = lines.index('End of search list.', begin)
except ValueError as error:
    raise SystemExit("clang did not report an include search list") from error
paths = []
for line in lines[begin:end]:
    value = line.strip().removesuffix(" (framework directory)")
    if value:
        paths.append(str(Path(value).resolve()))
if not paths or paths[0] != expected:
    raise SystemExit(f"bundled libc++ is not first in include search: {paths!r}")
for path in paths:
    if path.endswith("/include/c++/v1") and path != expected:
        raise SystemExit(f"host libc++ leaked into include search: {path}")
PY

probe_binary="${temporary_root}/platform-probe"
probe_json="${temporary_root}/platform-probe.json"
"${cxx}" "${effective_flags[@]}" \
    -I"${repository_root}/include" \
    -I"${repository_root}/example/relocatable_world_demo" \
    "-DTYPELAYOUT_TOOLCHAIN_REVISION=\"${compiler_revision}\"" \
    "-DTYPELAYOUT_COMPILER_TARGET=\"${target}\"" \
    "${repository_root}/example/relocatable_world_demo/platform_probe.cpp" \
    -o "${probe_binary}"

otool_output="${temporary_root}/otool-libraries.txt"
otool -L "${probe_binary}" >"${otool_output}"
grep -F '@rpath/libc++.1.dylib' "${otool_output}" >/dev/null
libcxx_otool_output="${temporary_root}/otool-libcxx-libraries.txt"
otool -L "${library_dir}/libc++.1.dylib" >"${libcxx_otool_output}"
grep -F '@rpath/libc++.1.dylib' "${libcxx_otool_output}" >/dev/null
grep -F '@rpath/libc++abi.1.dylib' "${libcxx_otool_output}" >/dev/null
rpath_output="${temporary_root}/otool-rpaths.txt"
otool -l "${probe_binary}" >"${rpath_output}"
python3 - "${rpath_output}" "${library_dir}" <<'PY'
from pathlib import Path
import re
import sys

text = Path(sys.argv[1]).read_text(encoding="utf-8", errors="replace")
rpaths = re.findall(r"\n\s*cmd LC_RPATH\n.*?\n\s*path (\S+) \(offset", text)
expected = str(Path(sys.argv[2]).resolve())
if rpaths != [expected]:
    raise SystemExit(f"executable rpaths do not resolve only to archive: {rpaths!r}")
PY

runner_image="${ImageOS-}/${ImageVersion-}"
dyld_output="${temporary_root}/dyld-libraries.txt"
DYLD_PRINT_LIBRARIES=1 "${probe_binary}" \
    "${node}" "${probe_json}" \
    --runner "${runner}" \
    --runner-image "${runner_image}" \
    --xcode-version "${actual_xcode}" \
    --xcode-build "${actual_xcode_build}" \
    --sdk-version "${actual_sdk}" \
    --sdk-build "${actual_sdk_build}" \
    --deployment-target "${deployment_target}" \
    --sdk-locked "${sdk_locked}" \
    2>"${dyld_output}"
grep -F "${library_dir}/libc++.1.dylib" "${dyld_output}" >/dev/null
grep -F "${library_dir}/libc++abi.1.dylib" "${dyld_output}" >/dev/null

if [[ -z "${metadata_output}" ]]; then
    metadata_output="${PWD}/p2996-${node}.verification.json"
fi
mkdir -p "$(dirname "${metadata_output}")"
metadata_output="$(cd "$(dirname "${metadata_output}")" && pwd)/$(basename "${metadata_output}")"
compiler_version="$("${cxx}" --version)"
stdlib="$(python3 - "${probe_json}" <<'PY'
import json
import sys
print(json.load(open(sys.argv[1], encoding="utf-8"))["compiler"]["stdlib"])
PY
)"
export TYPELAYOUT_VERIFY_NODE="${node}"
export TYPELAYOUT_VERIFY_ARCH="${architecture}"
export TYPELAYOUT_VERIFY_SHA256="${actual_sha256}"
export TYPELAYOUT_VERIFY_REVISION="${compiler_revision}"
export TYPELAYOUT_VERIFY_VERSION="${compiler_version}"
export TYPELAYOUT_VERIFY_TARGET="${target}"
export TYPELAYOUT_VERIFY_STDLIB="${stdlib}"
export TYPELAYOUT_VERIFY_XCODE="${actual_xcode}"
export TYPELAYOUT_VERIFY_XCODE_BUILD="${actual_xcode_build}"
export TYPELAYOUT_VERIFY_SDK="${actual_sdk}"
export TYPELAYOUT_VERIFY_SDK_BUILD="${actual_sdk_build}"
export TYPELAYOUT_VERIFY_DEPLOYMENT="${deployment_target}"
export TYPELAYOUT_VERIFY_SDK_LOCKED="${sdk_locked}"
export TYPELAYOUT_VERIFY_IMAGE_OS="${ImageOS-}"
export TYPELAYOUT_VERIFY_IMAGE_VERSION="${ImageVersion-}"
export TYPELAYOUT_VERIFY_DEVELOPER_DIR="${developer_dir}"
export TYPELAYOUT_VERIFY_SDKROOT="${sdkroot}"
export TYPELAYOUT_VERIFY_ROOT="${toolchain_root}"
export TYPELAYOUT_VERIFY_FLAGS="${locked_flags}"
python3 - "${metadata_output}" <<'PY'
import json
import os
import sys

record = {
    "schema": 1,
    "node": os.environ["TYPELAYOUT_VERIFY_NODE"],
    "architecture": os.environ["TYPELAYOUT_VERIFY_ARCH"],
    "archive_sha256": os.environ["TYPELAYOUT_VERIFY_SHA256"],
    "compiler_revision": os.environ["TYPELAYOUT_VERIFY_REVISION"],
    "compiler_version": os.environ["TYPELAYOUT_VERIFY_VERSION"],
    "target": os.environ["TYPELAYOUT_VERIFY_TARGET"],
    "stdlib": os.environ["TYPELAYOUT_VERIFY_STDLIB"],
    "xcode_version": os.environ["TYPELAYOUT_VERIFY_XCODE"],
    "xcode_build": os.environ["TYPELAYOUT_VERIFY_XCODE_BUILD"],
    "sdk_version": os.environ["TYPELAYOUT_VERIFY_SDK"],
    "sdk_build": os.environ["TYPELAYOUT_VERIFY_SDK_BUILD"],
    "deployment_target": os.environ["TYPELAYOUT_VERIFY_DEPLOYMENT"],
    "sdk_locked": os.environ["TYPELAYOUT_VERIFY_SDK_LOCKED"] == "true",
    "observed_runner": {
        "image_os": os.environ.get("TYPELAYOUT_VERIFY_IMAGE_OS", ""),
        "image_version": os.environ.get("TYPELAYOUT_VERIFY_IMAGE_VERSION", ""),
    },
    "environment": {
        "developer_dir": os.environ["TYPELAYOUT_VERIFY_DEVELOPER_DIR"],
        "sdkroot": os.environ["TYPELAYOUT_VERIFY_SDKROOT"],
        "toolchain_root": os.environ["TYPELAYOUT_VERIFY_ROOT"],
    },
    "flags": os.environ["TYPELAYOUT_VERIFY_FLAGS"],
}
with open(sys.argv[1], "w", encoding="utf-8", newline="\n") as output:
    json.dump(record, output, indent=2, sort_keys=True)
    output.write("\n")
PY

echo "P2996 TOOLCHAIN PASS node=${node} sdk_locked=${sdk_locked} sha256=${actual_sha256}"
echo "DEVELOPER_DIR=${developer_dir}"
echo "SDKROOT=${sdkroot}"
echo "TOOLCHAIN_ROOT=${toolchain_root}"
echo "FLAGS=${effective_flags[*]}"
echo "METADATA=${metadata_output}"
