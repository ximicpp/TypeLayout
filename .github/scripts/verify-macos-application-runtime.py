#!/usr/bin/env python3

"""Verify a matrix application's locked macOS runtime linkage contract."""

import argparse
import posixpath
import re
from pathlib import Path, PurePosixPath


RELEVANT_RUNTIME = re.compile(
    r"^(libc\+\+|libc\+\+abi|libunwind)(?:\.[0-9]+)*\.dylib$"
)
DYLD_RECORD = re.compile(
    r"^dyld\[(?P<pid>[0-9]+)\]: "
    r"(?:<[0-9A-Fa-f]{8}(?:-[0-9A-Fa-f]{4}){3}-[0-9A-Fa-f]{12}> )?"
    r"(?P<path>.+)$"
)
DYLD_MOVE_STATUS = re.compile(
    r"^dyld\[(?P<pid>[0-9]+)\]: move "
    r"(?:loaded to delayed|delayed to loaded): [^/\r\n]+$"
)
DYLD_WEAK_STATUS = re.compile(
    r"^dyld\[(?P<pid>[0-9]+)\]: [^/\r\n]+ has weak-def "
    r"\(or flat lookup\) symbol used by [^/\r\n]+, so cannot be delayed$"
)
DYLD_INTERPOSE_STATUS = re.compile(
    r"^dyld\[(?P<pid>[0-9]+)\]: has interposing tuples so cannot be "
    r"delayed: [^/\r\n]+$"
)


class VerificationError(RuntimeError):
    pass


def read_text(path):
    try:
        return Path(path).read_text(encoding="utf-8", errors="strict")
    except (OSError, UnicodeError) as error:
        raise VerificationError(f"cannot read runtime evidence {path!r}") from error


def runtime_leaf(candidate):
    return PurePosixPath(candidate).name


def verify_load_commands(text):
    observed = []
    for raw_line in text.splitlines()[1:]:
        fields = raw_line.strip().split()
        if not fields:
            continue
        candidate = fields[0]
        if RELEVANT_RUNTIME.fullmatch(runtime_leaf(candidate)):
            observed.append(candidate)

    expected = ["@rpath/libc++.1.dylib", "@rpath/libunwind.1.dylib"]
    if observed != expected:
        raise VerificationError(
            "application C++ runtime load commands are not exact: "
            f"{observed!r} != {expected!r}"
        )


def load_command_blocks(text):
    blocks = []
    current = []
    for line in text.splitlines():
        if re.fullmatch(r"Load command [0-9]+", line):
            if current:
                blocks.append(current)
            current = [line]
        elif current:
            current.append(line)
    if current:
        blocks.append(current)
    return blocks


def verify_rpaths(text, library_dir):
    observed = []
    for block in load_command_blocks(text):
        if not any(re.fullmatch(r"\s*cmd LC_RPATH", line) for line in block):
            continue
        paths = []
        for line in block:
            match = re.fullmatch(r"\s*path (?P<path>.+) \(offset [0-9]+\)", line)
            if match is not None:
                paths.append(posixpath.normpath(match.group("path")))
        if len(paths) != 1:
            raise VerificationError(f"malformed LC_RPATH command: {block!r}")
        observed.extend(paths)

    expected = posixpath.normpath(library_dir)
    if observed != [expected]:
        raise VerificationError(
            f"application LC_RPATH is not exact: {observed!r} != {[expected]!r}"
        )


def verify_trace(text, library_dir):
    system_cache_paths = {
        "/usr/lib/libc++.1.dylib",
        "/usr/lib/libc++abi.dylib",
        "/usr/lib/system/libunwind.dylib",
    }
    pids = set()
    observed_paths = set()
    observed_families = set()

    for line in text.splitlines():
        if not line.startswith("dyld["):
            continue
        status = None
        for pattern in (
            DYLD_MOVE_STATUS,
            DYLD_WEAK_STATUS,
            DYLD_INTERPOSE_STATUS,
        ):
            status = pattern.fullmatch(line)
            if status is not None:
                pids.add(status.group("pid"))
                break
        if status is not None:
            continue

        record = DYLD_RECORD.fullmatch(line)
        if record is None:
            raise VerificationError(f"malformed dyld library record: {line!r}")
        pids.add(record.group("pid"))
        candidate = record.group("path")
        if not candidate.startswith("/"):
            raise VerificationError(f"dyld library path is not absolute: {candidate!r}")
        canonical = posixpath.normpath(candidate)
        match = RELEVANT_RUNTIME.fullmatch(runtime_leaf(canonical))
        if match is None:
            continue
        from_archive = posixpath.dirname(canonical) == library_dir
        if not from_archive and canonical not in system_cache_paths:
            raise VerificationError(f"unexpected runtime library path: {canonical}")
        if canonical in observed_paths:
            raise VerificationError(f"duplicate runtime library record: {canonical}")
        observed_paths.add(canonical)
        observed_families.add(match.group(1))

    if len(pids) != 1:
        raise VerificationError(
            f"dyld runtime trace must contain one process: {sorted(pids)!r}"
        )
    if "libc++" not in observed_families:
        raise VerificationError("dyld runtime trace does not contain libc++")


def parse_arguments():
    parser = argparse.ArgumentParser()
    parser.add_argument("--load-commands", required=True)
    parser.add_argument("--rpaths", required=True)
    parser.add_argument("--trace", required=True)
    parser.add_argument("--library-dir", required=True)
    return parser.parse_args()


def main():
    arguments = parse_arguments()
    library_dir = posixpath.normpath(arguments.library_dir)
    if not library_dir.startswith("/"):
        raise VerificationError("toolchain library directory must be absolute")
    verify_load_commands(read_text(arguments.load_commands))
    verify_rpaths(read_text(arguments.rpaths), library_dir)
    verify_trace(read_text(arguments.trace), library_dir)
    print("MACOS APPLICATION RUNTIME PASS")


if __name__ == "__main__":
    try:
        main()
    except VerificationError as error:
        raise SystemExit(str(error)) from error
