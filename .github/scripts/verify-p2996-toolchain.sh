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

for command in python3 xcodebuild xcode-select xcrun tar zstd shasum otool stat; do
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
url = "-"
archive_sha256 = "-"
identity_mode = "candidate"
locked_compiler_version = "-"
locked_compiler_target = "-"
locked_stdlib = "-"
if sys.argv[2]:
    with open(sys.argv[2], encoding="utf-8") as source:
        output = json.load(source)["macos"][node]
    url = output["url"]
    archive_sha256 = output["archive_sha256"]
    identity_mode = "output"
    locked_compiler_version = output["compiler_version"]
    locked_compiler_target = output["target"]
    locked_stdlib = output["stdlib"]
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
    identity_mode,
    locked_compiler_version,
    locked_compiler_target,
    locked_stdlib,
)
if any(any(character in value for character in "\t\r\n") for value in values):
    raise SystemExit("toolchain lock value contains a control separator")
print("\t".join(values))
PY
)"
IFS=$'\t' read -r runner architecture locked_xcode locked_xcode_build \
    locked_sdk locked_sdk_build deployment_target locked_flags compiler_revision \
    archive_url locked_archive_sha256 identity_mode locked_compiler_version \
    locked_compiler_target locked_stdlib <<<"${lock_values}"

if [[ "$(uname -m)" != "${architecture}" ]]; then
    echo "node ${node} requires native host architecture ${architecture}" >&2
    exit 1
fi

temporary_root="$(mktemp -d "${TMPDIR:-/tmp}/typelayout-p2996-verify.XXXXXX")"
archive="${temporary_root}/toolchain.tar.zst"
cleanup() {
    rm -rf "${temporary_root}"
}
trap cleanup EXIT

if [[ -n "${outputs}" ]]; then
    command -v curl >/dev/null 2>&1 || {
        echo "required command is missing: curl" >&2
        exit 1
    }
    curl --fail --location --proto '=https' --tlsv1.2 --retry 3 \
        --max-filesize 2147483647 \
        --output "${archive}" "${archive_url}"
    expected_sha256="${locked_archive_sha256}"
else
    candidate_source="$(cd "$(dirname "${candidate_archive}")" && pwd)/$(basename "${candidate_archive}")"
    python3 - "${candidate_source}" "${archive}" <<'PY'
import sys


MAX_ARCHIVE_BYTES = 2147483648
CHUNK_BYTES = 1048576


total = 0
with open(sys.argv[1], "rb") as source, open(sys.argv[2], "xb") as destination:
    while True:
        chunk = source.read(CHUNK_BYTES)
        if not chunk:
            break
        total += len(chunk)
        if total >= MAX_ARCHIVE_BYTES:
            raise SystemExit("candidate archive is at least 2 GiB")
        destination.write(chunk)
if total == 0:
    raise SystemExit("candidate archive is empty")
PY
    expected_sha256="${candidate_sha256}"
fi

archive_size="$(stat -f '%z' "${archive}")"
if (( archive_size <= 0 || archive_size >= 2147483648 )); then
    echo "toolchain archive size is outside the verified range: ${archive_size}" >&2
    exit 1
fi
actual_sha256="$(shasum -a 256 "${archive}" | awk '{print $1}')"
if [[ "${actual_sha256}" != "${expected_sha256}" ]]; then
    echo "archive SHA256 mismatch: ${actual_sha256} != ${expected_sha256}" >&2
    exit 1
fi

# BEGIN TOOLCHAIN ARCHIVE VALIDATOR
archive_validator="${temporary_root}/validate-archive.py"
cat >"${archive_validator}" <<'PY'
import posixpath
from pathlib import PurePosixPath
import sys
import tarfile
import tempfile


ROOT = "p2996-toolchain"
MAX_MEMBERS = 200000
MAX_EXPANDED_BYTES = 8589934592
MAX_DECOMPRESSED_BYTES = 10737418240
MAX_RAW_HEADERS = 400000
MAX_EXTENDED_HEADER_BYTES = 1048576
MAX_TOTAL_EXTENDED_HEADER_BYTES = 16777216
COPY_CHUNK_BYTES = 1048576
TAR_BLOCK_BYTES = 512
EXTENDED_HEADER_TYPES = {b"x", b"g", b"X", b"L", b"K"}


def require_inside_root(value, where):
    if not value or posixpath.isabs(value):
        raise SystemExit(f"unsafe {where}: {value!r}")
    normalized = posixpath.normpath(value)
    path = PurePosixPath(normalized)
    if not path.parts or path.parts[0] != ROOT or ".." in path.parts:
        raise SystemExit(f"unsafe {where}: {value!r}")
    return normalized


def tar_number(field):
    if field[0] == 0x80:
        return int.from_bytes(field, "big") & ((1 << (len(field) * 8 - 1)) - 1)
    if field[0] == 0xFF:
        return int.from_bytes(field, "big", signed=True)
    value = field.rstrip(b"\0 ").lstrip(b" ")
    try:
        return int(value or b"0", 8)
    except ValueError as error:
        raise SystemExit("archive contains an invalid tar size") from error


with tempfile.TemporaryFile() as raw_archive:
    decompressed_bytes = 0
    while True:
        chunk = sys.stdin.buffer.read(COPY_CHUNK_BYTES)
        if not chunk:
            break
        decompressed_bytes += len(chunk)
        if decompressed_bytes > MAX_DECOMPRESSED_BYTES:
            raise SystemExit(
                f"toolchain archive expands beyond {MAX_DECOMPRESSED_BYTES} tar bytes"
            )
        raw_archive.write(chunk)
    if decompressed_bytes == 0:
        raise SystemExit("toolchain archive is empty")

    raw_archive.seek(0)
    raw_headers = 0
    extended_header_bytes = 0
    while True:
        header = raw_archive.read(TAR_BLOCK_BYTES)
        if not header:
            break
        if len(header) != TAR_BLOCK_BYTES:
            raise SystemExit("archive contains a truncated tar header")
        if header == bytes(TAR_BLOCK_BYTES):
            break
        raw_headers += 1
        if raw_headers > MAX_RAW_HEADERS:
            raise SystemExit(f"toolchain archive exceeds {MAX_RAW_HEADERS} tar headers")
        size = tar_number(header[124:136])
        if size < 0:
            raise SystemExit("archive contains a negative tar member size")
        member_type = header[156:157]
        if member_type == b"S":
            raise SystemExit("GNU sparse archive members are forbidden")
        if member_type in EXTENDED_HEADER_TYPES:
            if size > MAX_EXTENDED_HEADER_BYTES:
                raise SystemExit(
                    f"extended tar header exceeds {MAX_EXTENDED_HEADER_BYTES} bytes"
                )
            extended_header_bytes += size
            if extended_header_bytes > MAX_TOTAL_EXTENDED_HEADER_BYTES:
                raise SystemExit("archive contains too much extended tar metadata")
        padded_size = ((size + TAR_BLOCK_BYTES - 1) // TAR_BLOCK_BYTES) * TAR_BLOCK_BYTES
        next_header = raw_archive.tell() + padded_size
        if next_header > decompressed_bytes:
            raise SystemExit("archive contains truncated tar member data")
        raw_archive.seek(next_header)

    raw_archive.seek(0)
    count = 0
    expanded_bytes = 0
    members = {}
    links = {}
    with tarfile.open(fileobj=raw_archive, mode="r:") as archive:
        for member in archive:
            count += 1
            if count > MAX_MEMBERS:
                raise SystemExit(f"toolchain archive exceeds {MAX_MEMBERS} members")
            sparse_keys = {
                key
                for key in member.pax_headers
                if key.startswith("GNU.sparse")
                or key.startswith("SCHILY.sparse")
                or key in {
                    "SCHILY.realsize",
                    "SCHILY.offset",
                    "SCHILY.numbytes",
                }
                or (
                    key == "SCHILY.filetype"
                    and member.pax_headers[key].casefold() == "sparse"
                )
            }
            if member.sparse is not None or sparse_keys:
                raise SystemExit(
                    f"sparse archive member is forbidden: {member.name!r}"
                )
            if ".." in PurePosixPath(member.name).parts:
                raise SystemExit(f"unsafe archive member: {member.name!r}")
            member_name = require_inside_root(member.name, "archive member")
            if member_name in members:
                raise SystemExit(
                    f"duplicate normalized archive member: {member_name!r}"
                )
            if member.isfile():
                members[member_name] = "file"
                expanded_bytes += member.size
                if expanded_bytes > MAX_EXPANDED_BYTES:
                    raise SystemExit(
                        "toolchain archive expands beyond "
                        f"{MAX_EXPANDED_BYTES} regular-file bytes"
                    )
                continue
            if member.isdir():
                members[member_name] = "directory"
                continue
            if member.issym():
                target = posixpath.join(
                    posixpath.dirname(member_name), member.linkname
                )
                target = require_inside_root(
                    target, f"symbolic link target for {member.name!r}"
                )
                members[member_name] = "link"
                links[member_name] = target
                continue
            if member.islnk():
                target = require_inside_root(
                    member.linkname, f"hard link target for {member.name!r}"
                )
                members[member_name] = "link"
                links[member_name] = target
                continue
            raise SystemExit(f"special archive member is forbidden: {member.name!r}")
    if count == 0:
        raise SystemExit("toolchain archive is empty")
    resolved_links = {}
    for link in links:
        path = []
        positions = {}
        current = link
        while current in links and current not in resolved_links:
            if current in positions:
                raise SystemExit(
                    f"archive link graph contains a cycle at {current!r}"
                )
            positions[current] = len(path)
            path.append(current)
            current = links[current]
            if current not in members:
                raise SystemExit(
                    f"archive link has an unresolved target: {link!r} -> {current!r}"
                )
        terminal = resolved_links.get(current, current)
        for path_link in reversed(path):
            resolved_links[path_link] = terminal
PY
# END TOOLCHAIN ARCHIVE VALIDATOR
zstd -dc "${archive}" | python3 "${archive_validator}"

if [[ -n "${extract_dir}" ]]; then
    mkdir -p "${extract_dir}"
    extract_dir="$(cd "${extract_dir}" && pwd)"
    extract_contents="$(find "${extract_dir}" -mindepth 1 -print -quit)" || {
        echo "cannot inspect --extract-dir: ${extract_dir}" >&2
        exit 1
    }
    if [[ -n "${extract_contents}" ]]; then
        echo "--extract-dir must be empty: ${extract_dir}" >&2
        exit 1
    fi
else
    extract_dir="${temporary_root}/extracted"
    mkdir -p "${extract_dir}"
fi
zstd -dc "${archive}" | tar -xf - -C "${extract_dir}"
toolchain_root="${extract_dir}/p2996-toolchain"
broken_link="$(find -L "${toolchain_root}" -type l -print -quit)" || {
    echo "cannot inspect extracted toolchain: ${toolchain_root}" >&2
    exit 1
}
if [[ -n "${broken_link}" ]]; then
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

flag_stream="${temporary_root}/locked-flags.nul"
python3 - "${locked_flags}" "${toolchain_root}" "${sdkroot}" "${target}" \
    >"${flag_stream}" <<'PY'
# BEGIN LOCKED FLAG EXPANDER
import shlex
import sys

tokens = shlex.split(sys.argv[1])
replacements = {
    "${TOOLCHAIN_ROOT}": sys.argv[2],
    "${SDKROOT}": sys.argv[3],
    "${TARGET_TRIPLE}": sys.argv[4],
}
for token in tokens:
    for placeholder, value in replacements.items():
        token = token.replace(placeholder, value)
    if "${" in token:
        raise SystemExit(f"unexpanded toolchain flag placeholder: {token}")
    sys.stdout.buffer.write(token.encode("utf-8") + b"\0")
# END LOCKED FLAG EXPANDER
PY
effective_flags=(--target="${target}")
while IFS= read -r -d '' flag; do
    effective_flags+=("${flag}")
done <"${flag_stream}"
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
libcxxabi_otool_output="${temporary_root}/otool-libcxxabi-libraries.txt"
otool -L "${library_dir}/libc++abi.1.dylib" >"${libcxxabi_otool_output}"
grep -F '@rpath/libc++abi.1.dylib' "${libcxxabi_otool_output}" >/dev/null
grep -F '@rpath/libunwind.1.dylib' "${libcxxabi_otool_output}" >/dev/null
rpath_output="${temporary_root}/otool-rpaths.txt"
otool -l "${probe_binary}" >"${rpath_output}"
python3 - "${rpath_output}" "${library_dir}" <<'PY'
# BEGIN MACOS RPATH VALIDATOR
from pathlib import Path
import re
import sys

text = Path(sys.argv[1]).read_text(encoding="utf-8", errors="replace")
rpaths = re.findall(r"\n\s*cmd LC_RPATH\n.*?\n\s*path (.*?) \(offset", text)
if len(rpaths) != 1 or not Path(rpaths[0]).is_absolute():
    raise SystemExit(f"executable rpaths do not resolve only to archive: {rpaths!r}")
try:
    observed = Path(rpaths[0]).resolve(strict=True)
    expected = Path(sys.argv[2]).resolve(strict=True)
except OSError as error:
    raise SystemExit(f"executable rpath cannot be resolved: {rpaths!r}") from error
if observed != expected:
    raise SystemExit(
        f"executable rpaths do not resolve only to archive: "
        f"observed={rpaths!r}, resolved={str(observed)!r}"
    )
# END MACOS RPATH VALIDATOR
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
python3 - "${repository_root}" "${probe_json}" "${node}" "${runner}" \
    "${compiler_revision}" "${target}" "${actual_xcode}" \
    "${actual_xcode_build}" "${actual_sdk}" "${actual_sdk_build}" \
    "${deployment_target}" "${sdk_locked}" <<'PY'
# BEGIN MACOS PLATFORM PROBE VALIDATOR
import pathlib
import sys


sys.path.insert(0, str(pathlib.Path(sys.argv[1], "tools").resolve()))
import relocatable_world_evidence as evidence


probe = evidence.validate_probe(pathlib.Path(sys.argv[2]))
expected_gates = {
    "char_bit": 8,
    "pointer_bits": 64,
    "endian": "little",
    "reflection": True,
    "memcpy_object_lifetime": True,
    "memcpy_array_lifetime": True,
}
if probe["node"] != sys.argv[3] or probe["probe"] != expected_gates:
    raise SystemExit("platform probe identity or capability mismatch")
if set(probe["admission"].values()) != {True}:
    raise SystemExit("candidate must admit all four contract types")
if probe["environment"]["runner"] != sys.argv[4]:
    raise SystemExit("platform probe runner mismatch")
compiler = probe["compiler"]
expected_compiler = {
    "family": "clang",
    "revision": sys.argv[5],
    "target": sys.argv[6],
    "xcode_version": sys.argv[7],
    "xcode_build": sys.argv[8],
    "sdk_version": sys.argv[9],
    "sdk_build": sys.argv[10],
    "deployment_target": sys.argv[11],
    "sdk_locked": sys.argv[12] == "true",
}
for key, expected in expected_compiler.items():
    if compiler[key] != expected:
        raise SystemExit(
            f"platform probe compiler {key} mismatch: "
            f"{compiler[key]!r} != {expected!r}"
        )
# END MACOS PLATFORM PROBE VALIDATOR
PY
python3 - "${dyld_output}" "${library_dir}" <<'PY'
# BEGIN MACOS RUNTIME LOAD VALIDATOR
from pathlib import Path
import re
import sys


relevant = re.compile(r"^(libc\+\+|libc\+\+abi|libunwind)(?:\.[^/]*)?\.dylib$")
dyld_record = re.compile(
    r"^dyld\[[0-9]+\]: "
    r"(?:<[0-9A-Fa-f]{8}(?:-[0-9A-Fa-f]{4}){3}-[0-9A-Fa-f]{12}> )?"
    r"(?P<path>.+)$"
)
dyld_delay_status = re.compile(
    r"^dyld\[[0-9]+\]: move loaded to delayed: [^/\r\n]+$"
)
library_dir = Path(sys.argv[2]).resolve()
expected = {
    "libc++": (library_dir / "libc++.1.dylib").resolve(),
    "libc++abi": (library_dir / "libc++abi.1.dylib").resolve(),
    "libunwind": (library_dir / "libunwind.1.dylib").resolve(),
}
for path in expected.values():
    if not path.is_file():
        raise SystemExit(f"bundled runtime library is missing: {path}")
observed = {key: set() for key in expected}
for line in Path(sys.argv[1]).read_text(
    encoding="utf-8", errors="replace"
).splitlines():
    if not line.startswith("dyld["):
        continue
    if dyld_delay_status.fullmatch(line):
        continue
    record = dyld_record.fullmatch(line)
    if record is None:
        raise SystemExit(f"malformed dyld library record: {line!r}")
    candidate = record.group("path")
    if not candidate.startswith("/") and not Path(candidate).is_absolute():
        raise SystemExit(f"malformed dyld library record: {line!r}")
    name = Path(candidate).name
    match = relevant.fullmatch(name)
    if match:
        observed[match.group(1)].add(Path(candidate).resolve())
for name, expected_path in expected.items():
    if observed[name] != {expected_path}:
        raise SystemExit(
            f"runtime {name} did not load only from the archive: "
            f"observed={sorted(map(str, observed[name]))!r}, expected={expected_path}"
        )
# END MACOS RUNTIME LOAD VALIDATOR
PY

if [[ -z "${metadata_output}" ]]; then
    metadata_output="${PWD}/p2996-${node}.verification.json"
fi
mkdir -p "$(dirname "${metadata_output}")"
metadata_output="$(cd "$(dirname "${metadata_output}")" && pwd)/$(basename "${metadata_output}")"
probe_identity="$(python3 - "${probe_json}" <<'PY'
import json
import sys

with open(sys.argv[1], encoding="utf-8") as source:
    compiler = json.load(source)["compiler"]
values = [compiler["version"], compiler["target"], compiler["stdlib"]]
if any(not isinstance(value, str) or not value for value in values):
    raise SystemExit("probe compiler identity must contain nonempty strings")
if any(any(character in value for character in "\t\r\n") for value in values):
    raise SystemExit("probe compiler identity contains a control separator")
print("\t".join(values))
PY
)"
IFS=$'\t' read -r compiler_version probe_target stdlib <<<"${probe_identity}"
[[ "${probe_target}" == "${target}" ]] || {
    echo "probe target mismatch: ${probe_target} != ${target}" >&2
    exit 1
}
if [[ "${identity_mode}" == output ]]; then
    [[ "${compiler_version}" == "${locked_compiler_version}" ]] || {
        echo "probe compiler version mismatch: ${compiler_version} != ${locked_compiler_version}" >&2
        exit 1
    }
    [[ "${probe_target}" == "${locked_compiler_target}" ]] || {
        echo "probe locked target mismatch: ${probe_target} != ${locked_compiler_target}" >&2
        exit 1
    }
    [[ "${stdlib}" == "${locked_stdlib}" ]] || {
        echo "probe standard library mismatch: ${stdlib} != ${locked_stdlib}" >&2
        exit 1
    }
fi
export TYPELAYOUT_VERIFY_NODE="${node}"
export TYPELAYOUT_VERIFY_ARCH="${architecture}"
export TYPELAYOUT_VERIFY_SHA256="${actual_sha256}"
export TYPELAYOUT_VERIFY_REVISION="${compiler_revision}"
export TYPELAYOUT_VERIFY_VERSION="${compiler_version}"
export TYPELAYOUT_VERIFY_TARGET="${probe_target}"
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
