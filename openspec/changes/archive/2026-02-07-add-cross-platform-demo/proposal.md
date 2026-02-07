# Change: Add Cross-Platform Serialization-Free Compatibility Demo

## Why
TypeLayout's core value is answering: "Can type T be shared directly across platforms without serialization?"
Currently the repo has zero examples demonstrating this. We need a compelling demo that:
1. Defines types users care about (network packets, shared-memory structs, file headers)
2. Extracts their layout signatures on the actual compilation platform
3. Outputs machine-comparable results so users can diff across architectures

## What Changes
- Add `example/cross_platform_check.cpp` — a signature extraction program
- Add `scripts/compare_signatures.py` — a Python script that diffs signature outputs from multiple platforms
- Add `example/README.md` — usage instructions for the multi-platform workflow

## Impact
- Affected specs: signature (no API changes, demo only)
- Affected code: new files only, no modifications to existing code
