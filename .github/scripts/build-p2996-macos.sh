#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
Usage: build-p2996-macos.sh --sources PATH --node NODE --output-dir DIR

Build the locked Bloomberg P2996 Clang and matching libc++ natively for one
macOS node. NODE is arm64_macos_clang or x86_64_macos_clang.
EOF
}

sources=""
node=""
output_dir=""
while [[ $# -gt 0 ]]; do
    case "$1" in
        --sources) sources="${2-}"; shift 2 ;;
        --node) node="${2-}"; shift 2 ;;
        --output-dir) output_dir="${2-}"; shift 2 ;;
        --help|-h) usage; exit 0 ;;
        *) echo "unknown option: $1" >&2; usage >&2; exit 2 ;;
    esac
done

if [[ -z "${sources}" || -z "${node}" || -z "${output_dir}" ]]; then
    usage >&2
    exit 2
fi

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repository_root="$(cd "${script_dir}/../.." && pwd)"
sources="$(cd "$(dirname "${sources}")" && pwd)/$(basename "${sources}")"
mkdir -p "${output_dir}"
output_dir="$(cd "${output_dir}" && pwd)"

for command in python3 git cmake ninja xcodebuild xcrun sysctl vm_stat tar zstd shasum; do
    command -v "${command}" >/dev/null 2>&1 || {
        echo "required command is missing: ${command}" >&2
        exit 1
    }
done

python3 "${script_dir}/validate-toolchain-locks.py" \
    --sources "${sources}" --recipe-root "${repository_root}" >/dev/null

lock_values="$(python3 - "${sources}" "${node}" <<'PY'
import json
import sys

with open(sys.argv[1], encoding="utf-8") as source:
    lock = json.load(source)
try:
    record = lock["macos"]["nodes"][sys.argv[2]]
except KeyError as error:
    raise SystemExit(f"unknown macOS node: {sys.argv[2]}") from error
values = (
    record["architecture"],
    record["llvm_target"],
    record["xcode_version"],
    record["xcode_build"],
    record["sdk_version"],
    record["sdk_build"],
    record["deployment_target"],
    lock["p2996"]["repository"],
    lock["p2996"]["commit"],
)
print("\t".join(values))
PY
)"
IFS=$'\t' read -r architecture llvm_target xcode_version xcode_build \
    sdk_version sdk_build deployment_target p2996_repository p2996_commit \
    <<<"${lock_values}"

host_architecture="$(uname -m)"
if [[ "${host_architecture}" != "${architecture}" ]]; then
    echo "node ${node} requires ${architecture}, host is ${host_architecture}" >&2
    exit 1
fi

developer_dir="/Applications/Xcode_${xcode_version}.app/Contents/Developer"
if [[ ! -d "${developer_dir}" ]]; then
    echo "locked Xcode is missing: ${developer_dir}" >&2
    exit 1
fi
export DEVELOPER_DIR="${developer_dir}"

actual_xcode="$(xcodebuild -version | sed -n '1s/^Xcode //p')"
actual_xcode_build="$(xcodebuild -version | sed -n '2s/^Build version //p')"
actual_sdk="$(xcrun --sdk macosx --show-sdk-version)"
actual_sdk_build="$(xcrun --sdk macosx --show-sdk-build-version)"
sdkroot="$(xcrun --sdk macosx --show-sdk-path)"
[[ "${actual_xcode}" == "${xcode_version}" ]] || {
    echo "Xcode version mismatch: ${actual_xcode} != ${xcode_version}" >&2
    exit 1
}
[[ "${actual_xcode_build}" == "${xcode_build}" ]] || {
    echo "Xcode build mismatch: ${actual_xcode_build} != ${xcode_build}" >&2
    exit 1
}
[[ "${actual_sdk}" == "${sdk_version}" ]] || {
    echo "SDK version mismatch: ${actual_sdk} != ${sdk_version}" >&2
    exit 1
}
[[ "${actual_sdk_build}" == "${sdk_build}" ]] || {
    echo "SDK build mismatch: ${actual_sdk_build} != ${sdk_build}" >&2
    exit 1
}

work_dir="$(mktemp -d "${TMPDIR:-/tmp}/typelayout-p2996-build.XXXXXX")"
trap 'rm -rf "${work_dir}"' EXIT
source_dir="${work_dir}/source"
build_dir="${work_dir}/build"
stage_dir="${work_dir}/stage"
toolchain_root="${stage_dir}/p2996-toolchain"

git init "${source_dir}"
git -C "${source_dir}" remote add origin "${p2996_repository}"
git -C "${source_dir}" fetch --depth=1 origin "${p2996_commit}"
git -C "${source_dir}" checkout --detach FETCH_HEAD
[[ "$(git -C "${source_dir}" rev-parse HEAD)" == "${p2996_commit}" ]] || {
    echo "fetched compiler commit does not match lock" >&2
    exit 1
}

cmake -S "${source_dir}/llvm" -B "${build_dir}" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="${toolchain_root}" \
    -DCMAKE_OSX_ARCHITECTURES="${architecture}" \
    -DCMAKE_OSX_SYSROOT="${sdkroot}" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET="${deployment_target}" \
    -DLLVM_ENABLE_PROJECTS="clang" \
    -DLLVM_ENABLE_RUNTIMES="libcxx;libcxxabi;libunwind" \
    -DLLVM_TARGETS_TO_BUILD="${llvm_target}" \
    -DLLVM_INCLUDE_TESTS=OFF \
    -DCLANG_INCLUDE_TESTS=OFF \
    -DLLVM_INCLUDE_EXAMPLES=OFF \
    -DLLVM_INCLUDE_BENCHMARKS=OFF \
    -DLLVM_INCLUDE_DOCS=OFF \
    -DCLANG_BUILD_EXAMPLES=OFF \
    -DCLANG_DEFAULT_CXX_STDLIB=libc++ \
    -DLLVM_INSTALL_TOOLCHAIN_ONLY=ON \
    -DLLVM_ENABLE_PER_TARGET_RUNTIME_DIR=OFF \
    -DLLVM_PARALLEL_LINK_JOBS=1

hardware_threads="$(sysctl -n hw.ncpu)"
available_memory="$(vm_stat | awk '
    /page size of/ {
        page_size = $8
        gsub(/[^0-9]/, "", page_size)
    }
    /Pages free:|Pages inactive:|Pages speculative:/ {
        pages = $NF
        gsub(/[^0-9]/, "", pages)
        available_pages += pages
    }
    END { printf "%.0f\\n", page_size * available_pages }
')"
if [[ -z "${available_memory}" || "${available_memory}" == 0 ]]; then
    available_memory="$(sysctl -n hw.memsize)"
fi
memory_jobs="$(( available_memory / 2147483648 ))"
(( memory_jobs >= 1 )) || memory_jobs=1
jobs="${hardware_threads}"
(( jobs <= memory_jobs )) || jobs="${memory_jobs}"
cmake --build "${build_dir}" --parallel "${jobs}"
cmake --install "${build_dir}" --strip

[[ -x "${toolchain_root}/bin/clang" && -x "${toolchain_root}/bin/clang++" ]]
[[ -f "${toolchain_root}/include/c++/v1/vector" ]]
find "${toolchain_root}/lib" -name 'libc++.*' -print -quit | grep -q .
find "${toolchain_root}/lib" -name 'libc++abi.*' -print -quit | grep -q .
find "${toolchain_root}/lib" -name 'libunwind.*' -print -quit | grep -q .
if find -L "${toolchain_root}" -type l -print -quit | grep -q .; then
    echo "installed toolchain contains a broken symbolic link" >&2
    exit 1
fi

archive="${output_dir}/p2996-macos-${architecture}-${p2996_commit}.tar.zst"
metadata="${output_dir}/p2996-macos-${architecture}-${p2996_commit}.metadata.json"
verification="${output_dir}/p2996-macos-${architecture}-${p2996_commit}.verification.json"
COPYFILE_DISABLE=1 tar -cf - -C "${stage_dir}" p2996-toolchain \
    | zstd -19 -T0 -o "${archive}"
archive_size="$(stat -f '%z' "${archive}")"
if (( archive_size >= 2147483648 )); then
    echo "toolchain archive is ${archive_size} bytes; limit is below 2 GiB" >&2
    exit 1
fi
archive_sha256="$(shasum -a 256 "${archive}" | awk '{print $1}')"

"${script_dir}/verify-p2996-toolchain.sh" \
    --sources "${sources}" \
    --node "${node}" \
    --candidate-archive "${archive}" \
    --candidate-sha256 "${archive_sha256}" \
    --require-locked-sdk \
    --metadata-output "${verification}"

export TYPELAYOUT_BUILD_NODE="${node}"
export TYPELAYOUT_BUILD_ARCH="${architecture}"
export TYPELAYOUT_BUILD_COMMIT="${p2996_commit}"
export TYPELAYOUT_BUILD_ARCHIVE="$(basename "${archive}")"
export TYPELAYOUT_BUILD_ARCHIVE_SHA256="${archive_sha256}"
export TYPELAYOUT_BUILD_ARCHIVE_SIZE="${archive_size}"
export TYPELAYOUT_BUILD_XCODE="${actual_xcode}"
export TYPELAYOUT_BUILD_XCODE_BUILD="${actual_xcode_build}"
export TYPELAYOUT_BUILD_SDK="${actual_sdk}"
export TYPELAYOUT_BUILD_SDK_BUILD="${actual_sdk_build}"
export TYPELAYOUT_BUILD_DEPLOYMENT="${deployment_target}"
export TYPELAYOUT_BUILD_IMAGE_OS="${ImageOS-}"
export TYPELAYOUT_BUILD_IMAGE_VERSION="${ImageVersion-}"
python3 - "${metadata}" <<'PY'
import json
import os
import sys

record = {
    "schema": 1,
    "node": os.environ["TYPELAYOUT_BUILD_NODE"],
    "architecture": os.environ["TYPELAYOUT_BUILD_ARCH"],
    "compiler_revision": os.environ["TYPELAYOUT_BUILD_COMMIT"],
    "archive": os.environ["TYPELAYOUT_BUILD_ARCHIVE"],
    "archive_sha256": os.environ["TYPELAYOUT_BUILD_ARCHIVE_SHA256"],
    "archive_size": int(os.environ["TYPELAYOUT_BUILD_ARCHIVE_SIZE"]),
    "xcode_version": os.environ["TYPELAYOUT_BUILD_XCODE"],
    "xcode_build": os.environ["TYPELAYOUT_BUILD_XCODE_BUILD"],
    "sdk_version": os.environ["TYPELAYOUT_BUILD_SDK"],
    "sdk_build": os.environ["TYPELAYOUT_BUILD_SDK_BUILD"],
    "deployment_target": os.environ["TYPELAYOUT_BUILD_DEPLOYMENT"],
    "observed_runner": {
        "image_os": os.environ.get("TYPELAYOUT_BUILD_IMAGE_OS", ""),
        "image_version": os.environ.get("TYPELAYOUT_BUILD_IMAGE_VERSION", ""),
    },
}
with open(sys.argv[1], "w", encoding="utf-8", newline="\n") as output:
    json.dump(record, output, indent=2, sort_keys=True)
    output.write("\n")
PY

echo "P2996 MACOS BUILD PASS node=${node} archive=${archive} sha256=${archive_sha256}"
