// Cross-platform compatibility checking (used in CI build steps).
//
// Public API:
//   - layout_match(a, b)          -- constexpr signature comparison
//   - CompatReporter              -- cross-platform compatibility report
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

/// Compare layout signatures. Usable in static_assert.
constexpr bool layout_match(const char* a, const char* b) noexcept {
    return std::string_view(a) == std::string_view(b);
}

namespace detail {

inline const char* safety_stars(SafetyLevel level) noexcept {
    switch (level) {
        case SafetyLevel::TrivialSafe:     return "***";
        case SafetyLevel::PointerRisk:     return "*!-";
        case SafetyLevel::PlatformVariant: return "*--";
        case SafetyLevel::Opaque:          return "---";
    }
    return "???";
}

inline const char* safety_reason(SafetyLevel level) noexcept {
    switch (level) {
        case SafetyLevel::TrivialSafe:     return "fixed-width scalars only";
        case SafetyLevel::PointerRisk:     return "contains pointers or references";
        case SafetyLevel::PlatformVariant: return "bit-fields or platform-dependent types (wchar_t, long double)";
        case SafetyLevel::Opaque:          return "contains opaque (unanalyzable) fields";
    }
    return "";
}

/// A parsed field from a signature's {...} member list.
struct SigField {
    std::string_view full;
    std::string_view offset;
    std::string_view type_sig;
};

inline SigField make_sig_field(std::string_view seg) noexcept {
    SigField f{seg, {}, {}};
    if (!seg.empty() && seg[0] == '@') {
        for (std::size_t j = 1; j < seg.size(); ++j) {
            if (seg[j] == ':') {
                f.offset = seg.substr(1, j - 1);
                f.type_sig = seg.substr(j + 1);
                break;
            }
        }
    }
    return f;
}

inline std::string_view sig_header(std::string_view sig) noexcept {
    auto brace = sig.find('{');
    return (brace == std::string_view::npos) ? sig : sig.substr(0, brace);
}

inline std::vector<SigField> parse_sig_fields(std::string_view sig) {
    auto open = sig.find('{');
    if (open == std::string_view::npos) return {};
    auto close = sig.rfind('}');
    if (close == std::string_view::npos || close <= open) return {};

    std::string_view content = sig.substr(open + 1, close - open - 1);
    if (content.empty()) return {};

    std::vector<SigField> fields;
    int depth = 0;
    std::size_t start = 0;
    for (std::size_t i = 0; i < content.size(); ++i) {
        char c = content[i];
        if (c == '[' || c == '{' || c == '<' || c == '(') ++depth;
        else if (c == ']' || c == '}' || c == '>' || c == ')') {
            if (depth > 0) --depth;
        } else if (c == ',' && depth == 0) {
            fields.push_back(make_sig_field(content.substr(start, i - start)));
            start = i + 1;
        }
    }
    if (start < content.size())
        fields.push_back(make_sig_field(content.substr(start)));

    return fields;
}

/// Result of comparing one type across platforms.
struct TypeResult {
    std::string name;
    bool        layout_match;
    bool        byte_copy_safe;
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

    bool abi_matches(const PlatformData& other) const noexcept {
        return pointer_size      == other.pointer_size &&
               sizeof_long       == other.sizeof_long &&
               sizeof_wchar_t    == other.sizeof_wchar_t &&
               sizeof_long_double == other.sizeof_long_double &&
               max_align         == other.max_align &&
               std::string_view(arch_prefix) == std::string_view(other.arch_prefix);
    }
};

} // namespace detail

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

    void add_platform(const detail::PlatformData& pd) { platforms_.push_back(pd); }

    void add_platform(const std::string& name,
                      const TypeEntry* types, std::size_t count) {
        platforms_.push_back({name, types, count, 0, 0, 0, 0, 0, "", {}});
    }

    /// Check if the specified types are transfer-safe across the
    /// specified platforms.
    ///
    /// Returns true when every type in @p type_names is byte-copy safe
    /// and has an identical layout signature across every platform.
    [[nodiscard]] bool are_transfer_safe(
            std::initializer_list<std::string_view> type_names,
            std::initializer_list<std::string_view> platform_names) const {
        return check_transfer_safe(type_names.begin(), type_names.end(),
                                   platform_names.begin(), platform_names.end());
    }

    /// Overload accepting vectors (for programmatic use).
    [[nodiscard]] bool are_transfer_safe(
            const std::vector<std::string>& type_names,
            const std::vector<std::string>& platform_names) const {
        return check_transfer_safe(type_names.begin(), type_names.end(),
                                   platform_names.begin(), platform_names.end());
    }

    /// Print the report with diff annotations for mismatched signatures.
    void print_diff_report(std::ostream& os = std::cout) const {
        print_report_impl(os, true);
    }

    /// Print the report to `os`.
    void print_report(std::ostream& os = std::cout) const {
        print_report_impl(os, false);
    }

    /// Returns true when every compared type has a matching layout signature
    /// and satisfies the exported byte-copy-safety preconditions.
    [[nodiscard]] bool all_types_transfer_safe() const {
        auto results = compare();
        return !results.empty() &&
               std::all_of(results.begin(), results.end(),
                           [](const detail::TypeResult& r) {
                               return r.layout_match && r.byte_copy_safe;
                           });
    }

private:
    std::vector<detail::PlatformData> platforms_;

    void print_report_impl(std::ostream& os, bool with_diff) const {
        auto results = compare();
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

        auto groups = abi_equivalence_groups();
        if (!groups.empty()) {
            os << "\n  ABI equivalence (identical ABI fingerprint):\n";
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

        os << "Safety: *** = transfer-safe, *!- = pointer risk,\n"
           << "        *-- = platform-variant, --- = opaque.\n\n";

        os << std::string(72, '-') << "\n";
        os << "  " << std::left << std::setw(24) << "Type"
           << std::right << std::setw(8) << "Layout"
           << std::setw(8) << "Safety"
           << "  Verdict\n";
        os << std::string(72, '-') << "\n";

        for (const auto& r : results) {
            std::string layout_str = r.layout_match ? "MATCH" : "DIFFER";
            std::string verdict = format_verdict(r, transfer_safe,
                                                 layout_compatible);

            os << "  " << std::left << std::setw(24) << r.name
               << std::right << std::setw(8) << layout_str
               << "    " << detail::safety_stars(r.safety)
               << "  " << verdict << "\n";
        }

        os << std::string(72, '-') << "\n\n";

        for (const auto& r : results) {
            if (!r.layout_match) {
                os << "  [DIFFER] " << r.name << " layout signatures:\n";
                if (with_diff) {
                    std::size_t max_name = 0;
                    for (const auto& p : platforms_)
                        if (p.name.size() > max_name) max_name = p.name.size();
                    const std::size_t prefix_w = 4 + max_name + 2;

                    std::string ref_sig;
                    for (const auto& s : r.layout_sigs) {
                        if (s != "<missing>") {
                            ref_sig = s;
                            break;
                        }
                    }

                    for (std::size_t i = 0; i < platforms_.size(); ++i) {
                        os << "    " << platforms_[i].name;
                        for (std::size_t pad = platforms_[i].name.size();
                             pad < max_name; ++pad) {
                            os << ' ';
                        }
                        os << ": " << r.layout_sigs[i] << "\n";

                        if (i > 0 && r.layout_sigs[i] != ref_sig) {
                            std::string ann =
                                format_diff(ref_sig, r.layout_sigs[i], prefix_w);
                            if (!ann.empty())
                                os << ann << "\n";
                            format_field_diff(os, ref_sig, r.layout_sigs[i]);
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

        bool has_warnings = false;
        for (const auto& r : results) {
            if (r.layout_match && r.safety != detail::SafetyLevel::TrivialSafe) {
                if (!has_warnings) {
                    os << "  Safety warnings:\n";
                    has_warnings = true;
                }
                os << "  [" << detail::safety_stars(r.safety) << "] "
                   << r.name << " -- " << detail::safety_reason(r.safety) << "\n";
            }
        }
        if (has_warnings) os << "\n";

        os << std::string(72, '=') << "\n";
        if (transfer_safe == total) {
            os << "  ALL " << total
               << " type(s) are transfer-safe across all platforms!\n";
        } else {
            int ts_pct = total > 0 ? (transfer_safe * 100 / total) : 0;
            os << "  Transfer-safe:              " << transfer_safe
               << "/" << total << " (" << ts_pct << "%)"
               << " (byte-copy safe + layout match)\n";
            if (layout_compatible > transfer_safe) {
                os << "  Layout-compatible:          " << layout_compatible
                   << "/" << total
                   << " (layout matches but has pointers)\n";
            }
            os << "  Layout mismatch:            " << (total - layout_compatible)
               << "/" << total << "\n";
        }
        os << std::string(72, '=') << "\n\n";

        os << "  Layout mismatches can be enforced with generated checks and "
              "compile-time assertions; transport preconditions are reported "
              "here and can be surfaced in CI.\n\n";
    }

    std::vector<detail::TypeResult> compare() const {
        if (platforms_.empty()) return {};

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

        std::vector<detail::TypeResult> results;
        results.reserve(all_names.size());

        for (const auto& name : all_names) {
            detail::TypeResult tr;
            tr.name = name;
            tr.layout_match = true;
            tr.byte_copy_safe = true;

            detail::SafetyLevel worst_safety = detail::SafetyLevel::TrivialSafe;
            std::string first_sig;

            for (const auto& plat : platforms_) {
                const TypeEntry* entry = find_type(plat, name);
                if (!entry) {
                    tr.layout_sigs.emplace_back("<missing>");
                    tr.layout_match = false;
                    tr.byte_copy_safe = false;
                    continue;
                }
                std::string sig(entry->layout_sig);
                if (first_sig.empty()) {
                    first_sig = sig;
                } else if (sig != first_sig) {
                    tr.layout_match = false;
                }
                tr.layout_sigs.push_back(std::move(sig));

                if (!entry->byte_copy_safe)
                    tr.byte_copy_safe = false;

                auto level = detail::classify_signature(entry->layout_sig);
                if (static_cast<int>(level) > static_cast<int>(worst_safety))
                    worst_safety = level;
            }
            tr.safety = worst_safety;
            results.push_back(std::move(tr));
        }
        return results;
    }

    static std::string format_verdict(const detail::TypeResult& r,
                                      int& transfer_safe,
                                      int& layout_compatible) {
        if (!r.layout_match)
            return "Layout mismatch";

        ++layout_compatible;

        if (!r.byte_copy_safe)
            return "Layout match (not byte-copy safe)";

        ++transfer_safe;

        switch (r.safety) {
            case detail::SafetyLevel::TrivialSafe:
                return "Transfer-safe";
            case detail::SafetyLevel::PlatformVariant:
                return "Transfer-safe (platform-variant fields matched)";
            case detail::SafetyLevel::Opaque:
                return "Transfer-safe (contains opaque fields)";
            default:
                return "Transfer-safe";
        }
    }

    static std::string format_diff(const std::string& a, const std::string& b,
                                   std::size_t prefix_width) {
        std::size_t pos = 0;
        while (pos < a.size() && pos < b.size() && a[pos] == b[pos]) ++pos;
        if (pos == a.size() && pos == b.size()) return "";

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

    static void format_field_diff(std::ostream& os,
                                  const std::string& ref_sig,
                                  const std::string& other_sig) {
        auto ref_fields = detail::parse_sig_fields(ref_sig);
        auto oth_fields = detail::parse_sig_fields(other_sig);

        if (ref_fields.empty() && oth_fields.empty()) return;

        std::size_t max_n = std::max(ref_fields.size(), oth_fields.size());
        std::size_t diff_count = 0;
        for (std::size_t i = 0; i < max_n; ++i) {
            bool match = (i < ref_fields.size() && i < oth_fields.size() &&
                          ref_fields[i].full == oth_fields[i].full);
            if (!match) ++diff_count;
        }
        if (diff_count == 0) return;

        auto ref_hdr = detail::sig_header(ref_sig);
        auto oth_hdr = detail::sig_header(other_sig);
        bool hdr_diff = (ref_hdr != oth_hdr);

        os << "    Field diff: " << diff_count << " of "
           << max_n << " field(s) differ";
        if (hdr_diff) os << "; header differs";
        os << "\n";

        if (hdr_diff) {
            os << "      header: " << ref_hdr << "\n"
               << "          vs: " << oth_hdr << "\n";
        }
        for (std::size_t i = 0; i < max_n; ++i) {
            if (i < ref_fields.size() && i < oth_fields.size()) {
                if (ref_fields[i].full != oth_fields[i].full) {
                    os << "      #" << (i + 1) << ": "
                       << ref_fields[i].full << " vs "
                       << oth_fields[i].full << "\n";
                }
            } else if (i < ref_fields.size()) {
                os << "      #" << (i + 1) << ": "
                   << ref_fields[i].full << " (only in reference)\n";
            } else {
                os << "      #" << (i + 1) << ": "
                   << oth_fields[i].full << " (only in other)\n";
            }
        }
    }

    template <typename TIter, typename PIter>
    bool check_transfer_safe(TIter t_begin, TIter t_end,
                                  PIter p_begin, PIter p_end) const {
        if (t_begin == t_end || p_begin == p_end)
            return false;

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

        for (auto it = t_begin; it != t_end; ++it) {
            std::string_view tname(*it);
            std::string_view first_sig;
            for (std::size_t pi : plat_idx) {
                const TypeEntry* entry = find_type(platforms_[pi],
                                                   std::string(tname));
                if (!entry) return false;

                if (!entry->byte_copy_safe)
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

    std::vector<std::vector<std::string>> abi_equivalence_groups() const {
        std::vector<std::vector<std::string>> groups;
        std::vector<bool> visited(platforms_.size(), false);

        for (std::size_t i = 0; i < platforms_.size(); ++i) {
            if (visited[i]) continue;
            if (platforms_[i].pointer_size == 0) continue;

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

    static const TypeEntry* find_type(const detail::PlatformData& plat,
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
