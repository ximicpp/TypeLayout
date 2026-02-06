#!/usr/bin/env python3
"""
Cross-Platform Signature Comparison Tool for Boost.TypeLayout

Reads JSON outputs from cross_platform_check (compiled on multiple platforms)
and produces a compatibility report showing which types can be shared
directly without serialization.

Usage:
    python3 compare_signatures.py signatures_x86_64.json signatures_arm64.json [...]

Output:
    - Per-type compatibility matrix
    - Summary: which types are universally portable
    - Warnings: which types differ and why

Copyright (c) 2024-2026 TypeLayout Development Team
Distributed under the Boost Software License, Version 1.0.
"""

import json
import sys
from pathlib import Path
from collections import defaultdict


def load_platform_data(filepath: str) -> dict:
    """Load a platform's signature JSON file."""
    with open(filepath, 'r') as f:
        return json.load(f)


def extract_platform_label(data: dict, filepath: str) -> str:
    """Generate a human-readable platform label."""
    plat = data.get("platform", {})
    prefix = plat.get("arch_prefix", "unknown")
    # Strip brackets: "[64-le]" → "64-le"
    label = prefix.strip("[]")
    # Add filename stem for disambiguation
    stem = Path(filepath).stem
    return f"{label} ({stem})"


def compare_signatures(platforms: list[tuple[str, dict]]) -> dict:
    """
    Compare signatures across all platforms.

    Returns a dict of:
      type_name -> {
        "layout_match": bool,      # All platforms have identical layout signature
        "definition_match": bool,  # All platforms have identical definition signature
        "layout_sigs": {platform_label: signature},
        "definition_sigs": {platform_label: signature},
        "sizes": {platform_label: size},
        "aligns": {platform_label: align},
      }
    """
    # Build per-type data
    type_data = defaultdict(lambda: {
        "layout_sigs": {},
        "definition_sigs": {},
        "sizes": {},
        "aligns": {},
    })

    for label, data in platforms:
        for t in data.get("types", []):
            name = t["name"]
            type_data[name]["layout_sigs"][label] = t["layout_signature"]
            type_data[name]["definition_sigs"][label] = t["definition_signature"]
            type_data[name]["sizes"][label] = t["size"]
            type_data[name]["aligns"][label] = t["align"]

    # Compute matches
    results = {}
    for name, info in type_data.items():
        layout_vals = list(info["layout_sigs"].values())
        defn_vals = list(info["definition_sigs"].values())

        results[name] = {
            "layout_match": len(set(layout_vals)) == 1,
            "definition_match": len(set(defn_vals)) == 1,
            "layout_sigs": info["layout_sigs"],
            "definition_sigs": info["definition_sigs"],
            "sizes": info["sizes"],
            "aligns": info["aligns"],
        }

    return results


def print_report(platforms: list[tuple[str, dict]], results: dict):
    """Print a human-readable compatibility report."""
    labels = [label for label, _ in platforms]
    n = len(labels)

    print("=" * 72)
    print("  Boost.TypeLayout — Cross-Platform Compatibility Report")
    print("=" * 72)
    print()

    # Platform summary
    print(f"Platforms compared: {n}")
    for label, data in platforms:
        plat = data.get("platform", {})
        print(f"  • {label}")
        print(f"    pointer={plat.get('pointer_size', '?')}B, "
              f"long={plat.get('sizeof_long', '?')}B, "
              f"wchar_t={plat.get('sizeof_wchar_t', '?')}B, "
              f"long_double={plat.get('sizeof_long_double', '?')}B, "
              f"max_align={plat.get('max_align', '?')}B")
    print()

    # Compatibility matrix
    portable = []
    incompatible = []

    print("-" * 72)
    print(f"  {'Type':<25} {'Size':>8} {'Layout':>10} {'Definition':>12}  Verdict")
    print("-" * 72)

    for name, info in results.items():
        # Size display
        sizes = list(info["sizes"].values())
        size_str = str(sizes[0]) if len(set(sizes)) == 1 else "/".join(str(s) for s in sizes)

        layout_ok = info["layout_match"]
        defn_ok = info["definition_match"]

        layout_str = "✅ MATCH" if layout_ok else "❌ DIFFER"
        defn_str = "✅ MATCH" if defn_ok else "❌ DIFFER"

        if layout_ok:
            verdict = "🟢 Serialization-free"
            portable.append(name)
        else:
            verdict = "🔴 Needs serialization"
            incompatible.append(name)

        print(f"  {name:<25} {size_str:>8} {layout_str:>10} {defn_str:>12}  {verdict}")

    print("-" * 72)
    print()

    # Summary
    print(f"✅ Serialization-free types ({len(portable)}/{len(results)}):")
    for name in portable:
        print(f"   • {name}")
    print()

    if incompatible:
        print(f"❌ Types requiring serialization ({len(incompatible)}/{len(results)}):")
        for name in incompatible:
            info = results[name]
            print(f"   • {name}")
            # Show what differs
            if not info["layout_match"]:
                print(f"     Layout signatures differ:")
                for label, sig in info["layout_sigs"].items():
                    print(f"       {label}: {sig}")
            print()

    # Final verdict
    print("=" * 72)
    if not incompatible:
        print("  🎉 ALL types are serialization-free across all platforms!")
    else:
        pct = len(portable) / len(results) * 100 if results else 0
        print(f"  {pct:.0f}% of types are serialization-free across all platforms.")
        print(f"  {len(incompatible)} type(s) need serialization for cross-platform use.")
    print("=" * 72)


def main():
    if len(sys.argv) < 2:
        print("Usage: compare_signatures.py <sig1.json> <sig2.json> [...]")
        print()
        print("Provide at least one JSON file from cross_platform_check.")
        print("With a single file, shows the extracted signatures.")
        print("With multiple files, compares across platforms.")
        sys.exit(1)

    # Load all platform data
    platforms = []
    for filepath in sys.argv[1:]:
        try:
            data = load_platform_data(filepath)
            label = extract_platform_label(data, filepath)
            platforms.append((label, data))
        except (json.JSONDecodeError, FileNotFoundError) as e:
            print(f"Error loading {filepath}: {e}", file=sys.stderr)
            sys.exit(1)

    if len(platforms) == 1:
        # Single platform: just show signatures
        label, data = platforms[0]
        print(f"Platform: {label}")
        print()
        for t in data.get("types", []):
            print(f"  {t['name']} (size={t['size']}, align={t['align']})")
            print(f"    Layout:     {t['layout_signature']}")
            print(f"    Definition: {t['definition_signature']}")
            print()
        return

    # Multi-platform comparison
    results = compare_signatures(platforms)
    print_report(platforms, results)


if __name__ == "__main__":
    main()
