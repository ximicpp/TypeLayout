// Cross-platform compatibility checking (C++17, no P2996).
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#ifndef BOOST_TYPELAYOUT_TOOLS_COMPAT_CHECK_HPP
#define BOOST_TYPELAYOUT_TOOLS_COMPAT_CHECK_HPP

#include <boost/typelayout/tools/sig_types.hpp>
#include <boost/typelayout/tools/safety_level.hpp>

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

/// Compare two signature strings. Usable in static_assert.
constexpr bool sig_match(const char* a, const char* b) noexcept {
    return std::string_view(a) == std::string_view(b);
}

/// Compare layout signatures.
constexpr bool layout_match(const char* a, const char* b) noexcept {
    return sig_match(a, b);
}

inline const char* safety_label(SafetyLevel level) noexcept {
    switch (level) {
        case SafetyLevel::TrivialSafe:     return "Safe";
        case SafetyLevel::PaddingRisk:     return "Pad";
        case SafetyLevel::PointerRisk:     return "Warn";
        case SafetyLevel::PlatformVariant: return "Risk";
        case SafetyLevel::Opaque:          return "Opaq";
    }
    return "?";
}

inline const char* safety_stars(SafetyLevel level) noexcept {
    switch (level) {
        case SafetyLevel::TrivialSafe:     return "***";
        case SafetyLevel::PaddingRisk:     return "**-";
        case SafetyLevel::PointerRisk:     return "*!-";
        case SafetyLevel::PlatformVariant: return "*--";
        case SafetyLevel::Opaque:          return "---";
    }
    return "???";
}

inline const char* safety_reason(SafetyLevel level) noexcept {
    switch (level) {
        case SafetyLevel::TrivialSafe:     return "fixed-width scalars only";
        case SafetyLevel::PaddingRisk:     return "has alignment padding";
        case SafetyLevel::PointerRisk:     return "contains pointers or references";
        case SafetyLevel::PlatformVariant: return "bit-fields or platform-dependent types (wchar_t, long double)";
        case SafetyLevel::Opaque:          return "contains opaque (unanalyzable) fields";
    }
    return "";
}

/// Result of comparing one type across platforms.
struct TypeResult {
    std::string name;
    bool        layout_match;
    SafetyLevel safety;
    std::vector<std::string> layout_sigs;
};

/// Platform info used by CompatReporter (runtime, owns strings).
struct PlatformData {
    std::string       name;
    const TypeEntry*  types;
    std::size_t       type_count;
    std::size_t       pointer_size      = 0;
    std::size_t       sizeof_long       = 0;
    std::size_t       sizeof_wchar_t    = 0;
    std::size_t       sizeof_long_double = 0;
    std::size_t       max_align         = 0;
    const char*       arch_prefix       = "";
};

/// Compares signatures across platforms and prints a compatibility matrix.
class CompatReporter {
public:
    /// Add a platform from a PlatformInfo (returned by .sig.hpp's get_platform_info()).
    void add_platform(const PlatformInfo& pi) {
        platforms_.push_back({
            pi.platform_name, pi.types, pi.type_count,
            pi.pointer_size, pi.sizeof_long, pi.sizeof_wchar_t,
            pi.sizeof_long_double, pi.max_align, pi.arch_prefix
        });
    }

    void add_platform(const PlatformData& pd) { platforms_.push_back(pd); }

    void add_platform(const std::string& name,
                      const TypeEntry* types, std::size_t count) {
        platforms_.push_back({name, types, count});
    }

    std::vector<TypeResult> compare() const {
        if (platforms_.empty()) return {};

        // Collect the union of all type names, preserving first-seen order.
        std::vector<std::string> all_names;
        for (const auto& plat : platforms_) {
            for (std::size_t i = 0; i < plat.type_count; ++i) {
                std::string name(plat.types[i].name);
                bool found = false;
                for (const auto& existing : all_names) {
                    if (existing == name) { found = true; break; }
                }
                if (!found) all_names.push_back(std::move(name));
            }
        }

        std::vector<TypeResult> results;
        results.reserve(all_names.size());

        for (const auto& name : all_names) {
            TypeResult tr;
            tr.name = name;
            tr.layout_match = true;

            SafetyLevel worst_safety = SafetyLevel::TrivialSafe;
            std::string first_sig;

            for (const auto& plat : platforms_) {
                const TypeEntry* entry = find_type(plat, name);
                if (!entry) {
                    tr.layout_sigs.emplace_back("<missing>");
                    tr.layout_match = false;
                    continue;
                }
                std::string sig(entry->layout_sig);
                if (first_sig.empty()) {
                    first_sig = sig;
                } else if (sig != first_sig) {
                    tr.layout_match = false;
                }
                tr.layout_sigs.push_back(std::move(sig));

                auto level = classify_signature(entry->layout_sig);
                if (static_cast<int>(level) > static_cast<int>(worst_safety))
                    worst_safety = level;
            }
            tr.safety = worst_safety;
            results.push_back(std::move(tr));
        }
        return results;
    }

    /// Print the report with character-level diff annotations for DIFFER entries.
    void print_diff_report(std::ostream& os = std::cout) const {
        print_report_impl(os, true);
    }

    /// Print the report to `os`.
    void print_report(std::ostream& os = std::cout) const {
        print_report_impl(os, false);
    }

private:
    std::vector<PlatformData> platforms_;

    void print_report_impl(std::ostream& os, bool with_diff) const {
        auto results = compare();
        int serialization_free = 0;
        int layout_compatible = 0;
        int total = static_cast<int>(results.size());

        os << std::string(72, '=') << "\n";
        os << "  Cross-Platform Compatibility Report";
        if (with_diff) os << " (with diff annotations)";
        os << "\n";
        os << std::string(72, '=') << "\n\n";

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

        os << "Safety: *** = zero-copy ok, **- = padding risk, *!- = pointer risk, *-- = platform-variant.\n\n";

        os << std::string(72, '-') << "\n";
        os << "  " << std::left << std::setw(24) << "Type"
           << std::right << std::setw(8) << "Layout"
           << std::setw(8) << "Safety"
           << "  Verdict\n";
        os << std::string(72, '-') << "\n";

        for (const auto& r : results) {
            std::string layout_str = r.layout_match ? "MATCH" : "DIFFER";
            std::string verdict = format_verdict(r, serialization_free,
                                                 layout_compatible);

            os << "  " << std::left << std::setw(24) << r.name
               << std::right << std::setw(8) << layout_str
               << "    " << safety_stars(r.safety)
               << "  " << verdict << "\n";
        }

        os << std::string(72, '-') << "\n\n";

        // Emit DIFFER blocks.
        for (const auto& r : results) {
            if (!r.layout_match) {
                os << "  [DIFFER] " << r.name << " layout signatures:\n";

                if (with_diff) {
                    // Compute the widest platform name for alignment
                    std::size_t max_name = 0;
                    for (const auto& p : platforms_)
                        if (p.name.size() > max_name) max_name = p.name.size();
                    const std::size_t prefix_w = 4 + max_name + 2;

                    // Find first non-missing sig as reference
                    std::string ref_sig;
                    for (const auto& s : r.layout_sigs) {
                        if (s != "<missing>") { ref_sig = s; break; }
                    }

                    for (std::size_t i = 0; i < platforms_.size(); ++i) {
                        os << "    " << platforms_[i].name;
                        for (std::size_t pad = platforms_[i].name.size(); pad < max_name; ++pad)
                            os << ' ';
                        os << ": " << r.layout_sigs[i] << "\n";

                        if (i > 0 && r.layout_sigs[i] != ref_sig) {
                            std::string ann = format_diff(ref_sig, r.layout_sigs[i], prefix_w);
                            if (!ann.empty())
                                os << ann << "\n";
                        }
                    }
                } else {
                    for (std::size_t i = 0; i < platforms_.size(); ++i) {
                        os << "    " << platforms_[i].name << ": "
                           << r.layout_sigs[i] << "\n";
                    }
                }
                os << "\n";
            }
        }

        // Safety warnings
        bool has_warnings = false;
        for (const auto& r : results) {
            if (r.layout_match && r.safety != SafetyLevel::TrivialSafe) {
                if (!has_warnings) {
                    os << "  Safety warnings:\n";
                    has_warnings = true;
                }
                os << "  [" << safety_stars(r.safety) << "] "
                   << r.name << " -- " << safety_reason(r.safety) << "\n";
            }
        }
        if (has_warnings) os << "\n";

        // Summary
        os << std::string(72, '=') << "\n";
        if (serialization_free == total) {
            os << "  ALL " << total
               << " type(s) are serialization-free across all platforms!\n";
        } else {
            int zst_pct = total > 0 ? (serialization_free * 100 / total) : 0;
            os << "  Serialization-free (C1+C2): " << serialization_free
               << "/" << total << " (" << zst_pct << "%)\n";
            if (layout_compatible > serialization_free) {
                os << "  Layout-compatible (C1):     " << layout_compatible
                   << "/" << total
                   << " (layout matches but has pointers/bit-fields)\n";
            }
            os << "  Needs serialization:        " << (total - layout_compatible)
               << "/" << total << "\n";
        }
        os << std::string(72, '=') << "\n\n";

        os << "  Assumptions:\n";
        os << "  - IEEE 754 floating point on all compared platforms\n";
        os << "  - Identical struct packing / alignment rules\n";
        os << "  - Fixed-width integers have the same representation\n";
        os << "  - Enums with explicit underlying types are stable\n\n";
    }

    static std::string format_verdict(const TypeResult& r,
                                      int& serialization_free,
                                      int& layout_compatible) {
        if (r.layout_match) {
            ++layout_compatible;
            if (r.safety == SafetyLevel::TrivialSafe) {
                ++serialization_free;
                return "Serialization-free";
            } else if (r.safety == SafetyLevel::PaddingRisk)
                return "Layout OK (padding may leak uninitialized bytes)";
            else if (r.safety == SafetyLevel::PointerRisk)
                return "Layout OK (pointer values not portable)";
            else if (r.safety == SafetyLevel::Opaque)
                return "Layout OK (contains opaque fields, verify manually)";
            else
                return "Layout OK (verify bit-fields manually)";
        }
        return "Needs serialization";
    }

    static std::string format_diff(const std::string& a, const std::string& b,
                                   std::size_t prefix_width) {
        std::size_t pos = 0;
        while (pos < a.size() && pos < b.size() && a[pos] == b[pos]) ++pos;
        if (pos == a.size() && pos == b.size()) return "";  // identical

        std::string arrow(prefix_width + pos, ' ');
        arrow += "^--- diverges at position ";
        arrow += std::to_string(pos);
        if (pos < a.size() && pos < b.size()) {
            arrow += " ('";
            arrow += a[pos];
            arrow += "' vs '";
            arrow += b[pos];
            arrow += "')";
        } else if (pos >= a.size()) {
            arrow += " (first string ends here)";
        } else {
            arrow += " (second string ends here)";
        }
        return arrow;
    }

    static const TypeEntry* find_type(const PlatformData& plat,
                                      const std::string& name) {
        for (std::size_t i = 0; i < plat.type_count; ++i) {
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
