// Cross-platform compatibility checking.
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
#include <initializer_list>
#include <iostream>
#include <iomanip>
#include <cstddef>
#include <algorithm>

namespace boost {
namespace typelayout {
inline namespace v1 {
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
        case SafetyLevel::PlatformVariant: return "bit-fields or platform-dependent types (wchar_t, fld/long double)";
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
    std::string       data_model;

    /// ABI fingerprint: the tuple of platform-level parameters that affect
    /// type layout.  Matching fingerprints are a necessary (but not
    /// sufficient) condition for identical signatures — compilers may
    /// still differ in struct packing rules.
    bool abi_matches(const PlatformData& other) const noexcept {
        return pointer_size      == other.pointer_size &&
               sizeof_long       == other.sizeof_long &&
               sizeof_wchar_t    == other.sizeof_wchar_t &&
               sizeof_long_double == other.sizeof_long_double &&
               max_align         == other.max_align &&
               std::string_view(arch_prefix) == std::string_view(other.arch_prefix);
    }
};

/// Compares signatures across platforms and prints a compatibility matrix.
class CompatReporter {
public:
    /// Add a platform from a PlatformInfo (returned by .sig.hpp's get_platform_info()).
    void add_platform(const PlatformInfo& pi) {
        platforms_.push_back({
            pi.platform_name, pi.types, pi.type_count,
            pi.pointer_size, pi.sizeof_long, pi.sizeof_wchar_t,
            pi.sizeof_long_double, pi.max_align, pi.arch_prefix,
            pi.data_model ? pi.data_model : ""
        });
    }

    void add_platform(const PlatformData& pd) { platforms_.push_back(pd); }

    void add_platform(const std::string& name,
                      const TypeEntry* types, std::size_t count) {
        platforms_.push_back({name, types, count});
    }

    // Serialization-free criteria for a type across a set of platforms:
    //   (1) trivially_copyable — guaranteed by SigExporter::add<T>'s static_assert
    //   (2) no pointer/reference members — detected as PointerRisk
    //   (3) layout signature matches across the specified platforms
    // Opaque types are allowed: the user registered them via
    // TYPELAYOUT_REGISTER_OPAQUE and takes responsibility for their layout.
    // If opaque signatures (O(Tag|N|A)) match across platforms, the type
    // is considered serialization-free.
    // PaddingRisk and PlatformVariant do NOT disqualify: padding is an
    // info-leak concern, and platform-variant types that match on the
    // given platforms are fine for those platforms.

    /// Check if the specified types are serialization-free across the
    /// specified platforms.  This is the core query for the use case:
    /// "given type set T and platform set P, can I memcpy safely?"
    ///
    /// Returns true when every type in @p type_names has an identical
    /// layout signature across every platform in @p platform_names,
    /// and none of those signatures contain pointers.
    ///
    /// Usage (brace-init):
    ///   reporter.are_serialization_free(
    ///       {"PacketHeader", "SensorRecord"},
    ///       {"x86_64_linux_clang", "arm64_macos_clang"});
    ///
    /// Usage (programmatic):
    ///   std::vector<std::string> types = read_config("types.txt");
    ///   std::vector<std::string> plats = read_config("platforms.txt");
    ///   reporter.are_serialization_free(types, plats);
    [[nodiscard]] bool are_serialization_free(
            std::initializer_list<std::string_view> type_names,
            std::initializer_list<std::string_view> platform_names) const {
        return check_serialization_free(type_names.begin(), type_names.end(),
                                        platform_names.begin(), platform_names.end());
    }

    /// Overload accepting vectors (for programmatic use).
    [[nodiscard]] bool are_serialization_free(
            const std::vector<std::string>& type_names,
            const std::vector<std::string>& platform_names) const {
        // Build string_view spans over the owned strings.
        std::vector<std::string_view> tv(type_names.begin(), type_names.end());
        std::vector<std::string_view> pv(platform_names.begin(), platform_names.end());
        return check_serialization_free(tv.begin(), tv.end(),
                                        pv.begin(), pv.end());
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
        int transfer_safe = 0;
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
            if (!p.data_model.empty())
                os << " " << p.data_model;
            os << "\n";
            if (p.pointer_size > 0) {
                os << "    pointer=" << p.pointer_size << "B"
                   << ", long=" << p.sizeof_long << "B"
                   << ", wchar_t=" << p.sizeof_wchar_t << "B"
                   << ", long_double=" << p.sizeof_long_double << "B"
                   << ", max_align=" << p.max_align << "B\n";
            }
        }

        // ABI equivalence groups: platforms with identical ABI fingerprints
        // are likely (but not guaranteed) to produce identical signatures.
        auto groups = abi_equivalence_groups();
        if (!groups.empty()) {
            os << "\n  ABI equivalence (identical ABI fingerprint → layouts likely match):\n";
            for (const auto& g : groups) {
                os << "    {";
                for (std::size_t i = 0; i < g.size(); ++i) {
                    if (i > 0) os << ", ";
                    os << g[i];
                }
                os << "}\n";
            }
        }
        os << "\n";

        os << "Safety: *** = serialization-free, **- = transfer-safe (padding risk),\n"
           << "        *!- = pointer risk, *-- = platform-variant, --- = opaque.\n\n";

        os << std::string(72, '-') << "\n";
        os << "  " << std::left << std::setw(24) << "Type"
           << std::right << std::setw(8) << "Layout"
           << std::setw(8) << "Safety"
           << "  Verdict\n";
        os << std::string(72, '-') << "\n";

        for (const auto& r : results) {
            std::string layout_str = r.layout_match ? "MATCH" : "DIFFER";
            std::string verdict = format_verdict(r, serialization_free,
                                                 transfer_safe,
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
        if (transfer_safe == total) {
            if (serialization_free == total) {
                os << "  ALL " << total
                   << " type(s) are serialization-free across all platforms!\n";
            } else {
                os << "  ALL " << total
                   << " type(s) are transfer-safe across all platforms!\n";
                os << "  Serialization-free (strict): " << serialization_free
                   << "/" << total << " (TrivialSafe only)\n";
            }
        } else {
            int ts_pct = total > 0 ? (transfer_safe * 100 / total) : 0;
            os << "  Transfer-safe:              " << transfer_safe
               << "/" << total << " (" << ts_pct << "%)"
               << " (byte-copy safe + layout match)\n";
            if (serialization_free > 0 && serialization_free < transfer_safe) {
                os << "  Serialization-free (strict): " << serialization_free
                   << "/" << total << " (TrivialSafe only)\n";
            }
            if (layout_compatible > transfer_safe) {
                os << "  Layout-compatible:          " << layout_compatible
                   << "/" << total
                   << " (layout matches but has pointers)\n";
            }
            os << "  Needs serialization:        " << (total - layout_compatible)
               << "/" << total << "\n";
        }
        os << std::string(72, '=') << "\n\n";

        os << "  All preconditions are enforced by signature matching and "
              "compile-time assertions.\n\n";
    }

    static std::string format_verdict(const TypeResult& r,
                                      int& serialization_free,
                                      int& transfer_safe,
                                      int& layout_compatible) {
        if (r.layout_match) {
            ++layout_compatible;
            if (r.safety == SafetyLevel::TrivialSafe) {
                ++serialization_free;
                ++transfer_safe;
                return "Serialization-free";
            } else if (r.safety == SafetyLevel::PaddingRisk) {
                ++transfer_safe;
                return "Transfer-safe (padding may leak uninitialized bytes)";
            } else if (r.safety == SafetyLevel::PointerRisk)
                return "Layout match (pointer values not portable)";
            else if (r.safety == SafetyLevel::Opaque) {
                ++transfer_safe;
                return "Transfer-safe (contains opaque fields, verify manually)";
            } else {
                ++transfer_safe;
                return "Transfer-safe (verify bit-fields manually)";
            }
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

    template <typename TIter, typename PIter>
    bool check_serialization_free(TIter t_begin, TIter t_end,
                                  PIter p_begin, PIter p_end) const {
        if (t_begin == t_end || p_begin == p_end)
            return false;

        // Resolve platform indices.
        std::vector<std::size_t> plat_idx;
        for (auto it = p_begin; it != p_end; ++it) {
            std::string_view pname(*it);
            bool found = false;
            for (std::size_t i = 0; i < platforms_.size(); ++i) {
                if (platforms_[i].name == pname) {
                    plat_idx.push_back(i);
                    found = true;
                    break;
                }
            }
            if (!found) return false;
        }

        // For each type, verify signature match + safety across selected platforms.
        for (auto it = t_begin; it != t_end; ++it) {
            std::string_view tname(*it);
            std::string_view first_sig;
            for (std::size_t pi : plat_idx) {
                const TypeEntry* entry = find_type(platforms_[pi],
                                                   std::string(tname));
                if (!entry) return false;

                auto level = classify_signature(entry->layout_sig);
                if (level == SafetyLevel::PointerRisk)
                    return false;

                std::string_view sig(entry->layout_sig);
                if (first_sig.empty())
                    first_sig = sig;
                else if (sig != first_sig)
                    return false;
            }
        }
        return true;
    }

    /// Find groups of platforms with identical ABI fingerprints.
    /// Matching fingerprints suggest (but do not guarantee) identical layouts.
    /// Only returns groups of size >= 2 (singletons are not interesting).
    std::vector<std::vector<std::string>> abi_equivalence_groups() const {
        std::vector<std::vector<std::string>> groups;
        std::vector<bool> visited(platforms_.size(), false);

        for (std::size_t i = 0; i < platforms_.size(); ++i) {
            if (visited[i]) continue;
            if (platforms_[i].pointer_size == 0) continue;  // no metadata

            std::vector<std::string> group;
            group.push_back(platforms_[i].name);

            for (std::size_t j = i + 1; j < platforms_.size(); ++j) {
                if (visited[j]) continue;
                if (platforms_[i].abi_matches(platforms_[j])) {
                    group.push_back(platforms_[j].name);
                    visited[j] = true;
                }
            }
            if (group.size() >= 2)
                groups.push_back(std::move(group));
        }
        return groups;
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
} // inline namespace v1
} // namespace typelayout
} // namespace boost

#endif // BOOST_TYPELAYOUT_TOOLS_COMPAT_CHECK_HPP
