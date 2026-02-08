// Cross-Platform Compatibility Check Utilities
//
// Phase 2 of the two-phase cross-platform compatibility pipeline.
// Include .sig.hpp headers from multiple platforms and use these utilities
// to verify binary compatibility at compile time (static_assert) or
// generate a human-readable runtime report.
//
// This header does NOT require P2996 — any C++17 compiler works.
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#ifndef BOOST_TYPELAYOUT_TOOLS_COMPAT_CHECK_HPP
#define BOOST_TYPELAYOUT_TOOLS_COMPAT_CHECK_HPP

#include <boost/typelayout/tools/sig_types.hpp>

#include <string_view>
#include <string>
#include <vector>
#include <iostream>
#include <iomanip>
#include <cstddef>
#include <algorithm>

namespace boost {
namespace typelayout {
namespace compat {

// =========================================================================
// Compile-Time Comparison (for use with static_assert)
// =========================================================================

/// Compare two signature strings for equality. Usable in static_assert.
constexpr bool sig_match(const char* a, const char* b) noexcept {
    return std::string_view(a) == std::string_view(b);
}

/// Compare two layout signatures. Alias for sig_match.
constexpr bool layout_match(const char* a, const char* b) noexcept {
    return sig_match(a, b);
}

/// Compare two definition signatures. Alias for sig_match.
constexpr bool definition_match(const char* a, const char* b) noexcept {
    return sig_match(a, b);
}

// =========================================================================
// Runtime Compatibility Reporter
// =========================================================================

/// Per-type comparison result.
struct TypeResult {
    std::string name;
    bool        layout_match;
    bool        definition_match;
    std::vector<std::string> layout_sigs;     // one per platform
    std::vector<std::string> definition_sigs; // one per platform
};

/// Runtime compatibility reporter.
///
/// Collects signature data from multiple platforms and produces a
/// formatted compatibility matrix.
///
/// Usage:
///   CompatReporter r;
///   r.add_platform("x86_64_linux_clang", plat_a::types, plat_a::type_count);
///   r.add_platform("arm64_linux_clang",  plat_b::types, plat_b::type_count);
///   r.print_report();
/// Platform data for the reporter (runtime, owns strings).
struct PlatformData {
    std::string       name;
    const TypeEntry*  types;
    int               type_count;
    std::size_t       pointer_size      = 0;
    std::size_t       sizeof_long       = 0;
    std::size_t       sizeof_wchar_t    = 0;
    std::size_t       sizeof_long_double = 0;
    std::size_t       max_align         = 0;
    const char*       arch_prefix       = "";
};

class CompatReporter {
public:
    /// Register from PlatformInfo (constexpr, from .sig.hpp get_platform_info()).
    void add_platform(const PlatformInfo& pi) {
        platforms_.push_back({
            pi.platform_name, pi.types, pi.type_count,
            pi.pointer_size, pi.sizeof_long, pi.sizeof_wchar_t,
            pi.sizeof_long_double, pi.max_align, pi.arch_prefix
        });
    }

    /// Register with explicit fields (legacy).
    void add_platform(const PlatformData& pd) {
        platforms_.push_back(pd);
    }

    /// Register a platform's signature data (basic form).
    void add_platform(const std::string& name,
                      const TypeEntry* types,
                      int count) {
        platforms_.push_back({name, types, count});
    }

    /// Compute comparison results.
    std::vector<TypeResult> compare() const {
        if (platforms_.empty()) return {};

        // Use the first platform's type list as the reference
        const auto& ref = platforms_[0];
        std::vector<TypeResult> results;
        results.reserve(static_cast<std::size_t>(ref.type_count));

        for (int i = 0; i < ref.type_count; ++i) {
            TypeResult tr;
            tr.name = ref.types[i].name;
            tr.layout_match = true;
            tr.definition_match = true;

            for (const auto& plat : platforms_) {
                // Find the matching type by name in this platform
                const TypeEntry* entry = find_type(plat, tr.name);
                if (!entry) {
                    tr.layout_sigs.emplace_back("<missing>");
                    tr.definition_sigs.emplace_back("<missing>");
                    tr.layout_match = false;
                    tr.definition_match = false;
                    continue;
                }
                tr.layout_sigs.emplace_back(entry->layout_sig);
                tr.definition_sigs.emplace_back(entry->definition_sig);

                if (std::string_view(entry->layout_sig) !=
                    std::string_view(ref.types[i].layout_sig)) {
                    tr.layout_match = false;
                }
                if (std::string_view(entry->definition_sig) !=
                    std::string_view(ref.types[i].definition_sig)) {
                    tr.definition_match = false;
                }
            }
            results.push_back(std::move(tr));
        }
        return results;
    }

    /// Print a formatted compatibility report to the given stream.
    void print_report(std::ostream& os = std::cout) const {
        auto results = compare();
        int compatible = 0;
        int total = static_cast<int>(results.size());

        os << std::string(72, '=') << "\n";
        os << "  Boost.TypeLayout — Cross-Platform Compatibility Report\n";
        os << std::string(72, '=') << "\n\n";

        // Platform summary
        os << "Platforms compared: " << platforms_.size() << "\n";
        for (const auto& p : platforms_) {
            os << "  * " << p.name;
            if (p.arch_prefix[0] != '\0')
                os << " " << p.arch_prefix;
            os << "\n";
            if (p.pointer_size > 0) {
                os << "    pointer=" << p.pointer_size << "B"
                   << ", long=" << p.sizeof_long << "B"
                   << ", wchar_t=" << p.sizeof_wchar_t << "B"
                   << ", long_double=" << p.sizeof_long_double << "B"
                   << ", max_align=" << p.max_align << "B\n";
            }
        }
        os << "\n";

        // Type matrix
        os << std::string(72, '-') << "\n";
        os << "  " << std::left << std::setw(28) << "Type"
           << std::right << std::setw(10) << "Layout"
           << std::setw(13) << "Definition"
           << "  Verdict\n";
        os << std::string(72, '-') << "\n";

        for (const auto& r : results) {
            std::string layout_str  = r.layout_match     ? "MATCH" : "DIFFER";
            std::string defn_str    = r.definition_match  ? "MATCH" : "DIFFER";
            std::string verdict;

            if (r.layout_match) {
                verdict = "Serialization-free";
                ++compatible;
            } else {
                verdict = "Needs serialization";
            }

            os << "  " << std::left << std::setw(28) << r.name
               << std::right << std::setw(10) << layout_str
               << std::setw(13) << defn_str
               << "  " << verdict << "\n";
        }

        os << std::string(72, '-') << "\n\n";

        // Mismatched details
        for (const auto& r : results) {
            if (!r.layout_match) {
                os << "  [DIFFER] " << r.name << " layout signatures:\n";
                for (std::size_t i = 0; i < platforms_.size(); ++i) {
                    os << "    " << platforms_[i].name << ": "
                       << r.layout_sigs[i] << "\n";
                }
                os << "\n";
            }
        }

        // Summary
        os << std::string(72, '=') << "\n";
        if (compatible == total) {
            os << "  ALL " << total
               << " type(s) are serialization-free across all platforms!\n";
        } else {
            int pct = total > 0 ? (compatible * 100 / total) : 0;
            os << "  " << pct << "% of types (" << compatible << "/" << total
               << ") are serialization-free across all platforms.\n";
            os << "  " << (total - compatible)
               << " type(s) need serialization for cross-platform use.\n";
        }
        os << std::string(72, '=') << "\n";
    }

private:
    std::vector<PlatformData> platforms_;

    static const TypeEntry* find_type(const PlatformData& plat,
                                      const std::string& name) {
        for (int i = 0; i < plat.type_count; ++i) {
            if (std::string_view(plat.types[i].name) == name)
                return &plat.types[i];
        }
        return nullptr;
    }
};

} // namespace compat
} // namespace typelayout
} // namespace boost

#endif // BOOST_TYPELAYOUT_TOOLS_COMPAT_CHECK_HPP
