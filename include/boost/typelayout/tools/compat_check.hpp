// Compatibility checking utilities for comparing .sig.hpp data across platforms.
// Provides constexpr comparators (for static_assert) and a runtime reporter.
// Requires C++17. Does not require P2996.
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

/// Compare two signature strings. Usable in static_assert.
constexpr bool sig_match(const char* a, const char* b) noexcept {
    return std::string_view(a) == std::string_view(b);
}

/// Compare layout signatures.
constexpr bool layout_match(const char* a, const char* b) noexcept {
    return sig_match(a, b);
}

/// Compare definition signatures.
constexpr bool definition_match(const char* a, const char* b) noexcept {
    return sig_match(a, b);
}

enum class SafetyLevel {
    Safe,       // no pointers, no bit-fields
    Warning,    // has pointers or vptr
    Risk        // has bit-fields or platform-dependent types
};

/// Scan a layout signature for pointers, bit-fields, etc.
inline SafetyLevel classify_safety(std::string_view sig) noexcept {
    if (sig.find("bits<") != std::string_view::npos)
        return SafetyLevel::Risk;
    if (sig.find("wchar[") != std::string_view::npos)
        return SafetyLevel::Risk;
    if (sig.find("f80[") != std::string_view::npos)
        return SafetyLevel::Risk;
    if (sig.find("ptr[") != std::string_view::npos ||
        sig.find("fnptr[") != std::string_view::npos ||
        sig.find("memptr[") != std::string_view::npos ||
        sig.find("ref[") != std::string_view::npos ||
        sig.find("rref[") != std::string_view::npos)
        return SafetyLevel::Warning;
    if (sig.find(",vptr") != std::string_view::npos)
        return SafetyLevel::Warning;

    return SafetyLevel::Safe;
}

inline const char* safety_label(SafetyLevel level) noexcept {
    switch (level) {
        case SafetyLevel::Safe:    return "Safe";
        case SafetyLevel::Warning: return "Warn";
        case SafetyLevel::Risk:    return "Risk";
    }
    return "?";
}

inline const char* safety_stars(SafetyLevel level) noexcept {
    switch (level) {
        case SafetyLevel::Safe:    return "***";
        case SafetyLevel::Warning: return "**-";
        case SafetyLevel::Risk:    return "*--";
    }
    return "???";
}

inline const char* safety_reason(SafetyLevel level) noexcept {
    switch (level) {
        case SafetyLevel::Safe:    return "fixed-width scalars only";
        case SafetyLevel::Warning: return "contains pointers or vptr";
        case SafetyLevel::Risk:    return "bit-fields or platform-dependent types (wchar_t, long double)";
    }
    return "";
}

/// Result of comparing one type across platforms.
struct TypeResult {
    std::string name;
    bool        layout_match;
    bool        definition_match;
    SafetyLevel safety;
    std::vector<std::string> layout_sigs;
    std::vector<std::string> definition_sigs;
};

/// Platform info used by CompatReporter (runtime, owns strings).
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
                      const TypeEntry* types, int count) {
        platforms_.push_back({name, types, count});
    }

    /// Compare all types across registered platforms.
    std::vector<TypeResult> compare() const {
        if (platforms_.empty()) return {};

        const auto& ref = platforms_[0];
        std::vector<TypeResult> results;
        results.reserve(static_cast<std::size_t>(ref.type_count));

        for (int i = 0; i < ref.type_count; ++i) {
            TypeResult tr;
            tr.name = ref.types[i].name;
            tr.layout_match = true;
            tr.definition_match = true;

            SafetyLevel worst_safety = SafetyLevel::Safe;

            for (const auto& plat : platforms_) {
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
                    std::string_view(ref.types[i].layout_sig))
                    tr.layout_match = false;
                if (std::string_view(entry->definition_sig) !=
                    std::string_view(ref.types[i].definition_sig))
                    tr.definition_match = false;

                auto level = classify_safety(entry->layout_sig);
                if (static_cast<int>(level) > static_cast<int>(worst_safety))
                    worst_safety = level;
            }
            tr.safety = worst_safety;
            results.push_back(std::move(tr));
        }
        return results;
    }

    /// Print the report to `os`.
    void print_report(std::ostream& os = std::cout) const {
        auto results = compare();
        int serialization_free = 0;
        int layout_compatible = 0;
        int total = static_cast<int>(results.size());

        os << std::string(72, '=') << "\n";
        os << "  Cross-Platform Compatibility Report\n";
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

        os << "Safety: *** = zero-copy ok, **- = has pointers/vptr, *-- = bit-fields.\n\n";

        os << std::string(80, '-') << "\n";
        os << "  " << std::left << std::setw(24) << "Type"
           << std::right << std::setw(8) << "Layout"
           << std::setw(12) << "Definition"
           << std::setw(8) << "Safety"
           << "  Verdict\n";
        os << std::string(80, '-') << "\n";

        for (const auto& r : results) {
            std::string layout_str  = r.layout_match     ? "MATCH" : "DIFFER";
            std::string defn_str    = r.definition_match  ? "MATCH" : "DIFFER";
            std::string verdict;

            if (r.layout_match) {
                ++layout_compatible;
                if (r.safety == SafetyLevel::Safe) {
                    ++serialization_free;
                    verdict = "Serialization-free";
                } else if (r.safety == SafetyLevel::Warning)
                    verdict = "Layout OK (pointer values not portable)";
                else
                    verdict = "Layout OK (verify bit-fields manually)";
            } else {
                verdict = "Needs serialization";
            }

            os << "  " << std::left << std::setw(24) << r.name
               << std::right << std::setw(8) << layout_str
               << std::setw(12) << defn_str
               << "    " << safety_stars(r.safety)
               << "  " << verdict << "\n";
        }

        os << std::string(80, '-') << "\n\n";

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

        bool has_warnings = false;
        for (const auto& r : results) {
            if (r.layout_match && r.safety != SafetyLevel::Safe) {
                if (!has_warnings) {
                    os << "  Safety warnings:\n";
                    has_warnings = true;
                }
                os << "  [" << safety_stars(r.safety) << "] "
                   << r.name << " — " << safety_reason(r.safety) << "\n";
            }
        }
        if (has_warnings) os << "\n";

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

        // Assumptions that underlie the safety classification
        os << "  Assumptions:\n";
        os << "  - IEEE 754 floating point on all compared platforms\n";
        os << "  - Identical struct packing / alignment rules\n";
        os << "  - Fixed-width integers have the same representation\n";
        os << "  - Enums with explicit underlying types are stable\n\n";
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
