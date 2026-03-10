// sig_parser.hpp -- Signature string parsing utilities.
//
// Pure C++17, no P2996 required.  Provides:
//   - sig_contains_token: token-boundary-aware substring search
//   - sig_has_padding / sig_has_padding_impl: padding gap detection
//   - check_padding_consistency: cross-validation helper
//
// Used by:
//   - layout_traits.hpp    (compile-time cross-validation of has_padding)
//   - safety_level.hpp     (runtime classify_signature)
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#ifndef BOOST_TYPELAYOUT_DETAIL_SIG_PARSER_HPP
#define BOOST_TYPELAYOUT_DETAIL_SIG_PARSER_HPP

#include <string_view>
#include <cstddef>

namespace boost {
namespace typelayout {
namespace detail {

/// Token-boundary-aware substring search for std::string_view.
///
/// Finds `needle` in `haystack`, but only accepts a match when the character
/// immediately before it is NOT alphanumeric.  This prevents e.g.
/// "nullptr[" from being falsely matched by "ptr[".
///
/// In TypeLayout signatures, type markers are always preceded by '{', ',',
/// ':', or appear at the start of the string -- never preceded by a letter.
constexpr bool sig_contains_token(std::string_view haystack,
                                  std::string_view needle) noexcept {
    std::size_t pos = 0;
    while (pos < haystack.size()) {
        std::size_t found = haystack.find(needle, pos);
        if (found == std::string_view::npos) return false;
        if (found == 0) return true;
        char prev = haystack[found - 1];
        bool prev_is_alnum = (prev >= 'a' && prev <= 'z') ||
                             (prev >= 'A' && prev <= 'Z') ||
                             (prev >= '0' && prev <= '9');
        if (!prev_is_alnum) return true;
        // False match inside a longer token -- advance past it
        pos = found + 1;
    }
    return false;
}

/// Padding detection result: whether padding was found and whether the
/// result is reliable (false when the field count exceeded MAX_FIELDS).
/// When truncated is true, has_padding is conservatively set to true but
/// may be a false positive for types with no actual padding.
struct SigPaddingResult {
    bool has_padding;
    bool truncated;
};

/// Check a single record block for padding gaps.
///
/// Given a view of a full layout signature and the byte position of a
/// "record[s:" token, extracts the record's declared size, finds its
/// "{...}" body, and checks whether the top-level "@offset:type[s:N]"
/// fields cover all bytes in [0, declared_size).
///
/// Returns {false, false} if the record cannot be parsed or has no body.
/// Returns {true, true}  if the field count exceeds MAX_FIELDS (conservative).
/// Returns {true, false} if a coverage gap is found (definite padding).
/// Returns {false, false} if all bytes are covered (no padding at this level).
constexpr SigPaddingResult check_one_record(std::string_view sig,
                                            std::size_t rec_pos) noexcept {
    std::size_t total_size = 0;
    std::size_t i = rec_pos + 9;  // length of "record[s:"
    while (i < sig.size() && sig[i] >= '0' && sig[i] <= '9') {
        total_size = total_size * 10 + (sig[i] - '0');
        ++i;
    }
    if (total_size == 0) return {false, false};

    // Find the '{' and its matching '}'
    auto brace_start = sig.find('{', rec_pos);
    if (brace_start == std::string_view::npos) return {false, false};

    int depth = 1;
    std::size_t brace_end = brace_start + 1;
    while (brace_end < sig.size() && depth > 0) {
        if (sig[brace_end] == '{') ++depth;
        else if (sig[brace_end] == '}') --depth;
        ++brace_end;
    }
    --brace_end;  // point to closing '}'

    auto content = sig.substr(brace_start + 1, brace_end - brace_start - 1);
    if (content.empty()) return {false, false};  // empty body (empty class)

    // Split content into top-level entries (by depth-0 commas)
    // and parse each entry's (offset, size) pair.
    struct Interval { std::size_t start; std::size_t end; };
    constexpr std::size_t MAX_FIELDS = 512;
    Interval intervals[MAX_FIELDS]{};
    std::size_t count = 0;

    std::size_t pos = 0;
    while (pos < content.size()) {
        if (count >= MAX_FIELDS)
            return {true, true};  // too many fields: conservative
        // Delimit this entry: find next depth-0 comma or end
        std::size_t entry_start = pos;
        int d = 0;
        std::size_t entry_end = pos;
        while (entry_end < content.size()) {
            char c = content[entry_end];
            if (c == '{' || c == '<' || c == '[') ++d;
            else if (c == '}' || c == '>' || c == ']') --d;
            else if (c == ',' && d == 0) break;
            ++entry_end;
        }

        auto entry = content.substr(entry_start, entry_end - entry_start);

        // Parse @OFFSET from entry
        if (entry.size() > 1 && entry[0] == '@') {
            std::size_t offset = 0;
            std::size_t j = 1;
            while (j < entry.size() && entry[j] >= '0' && entry[j] <= '9') {
                offset = offset * 10 + (entry[j] - '0');
                ++j;
            }

            // Extract field size from this entry.
            // Two formats to handle:
            //   [s:SIZE,...] — standard (primitives, records, arrays, etc.)
            //   O(tag|SIZE|ALIGN) — opaque registered type
            std::size_t field_size = 0;
            auto s_pos = entry.find("[s:");
            if (s_pos != std::string_view::npos) {
                std::size_t k = s_pos + 3;
                while (k < entry.size() && entry[k] >= '0' && entry[k] <= '9') {
                    field_size = field_size * 10 + (entry[k] - '0');
                    ++k;
                }
            } else {
                // Try O(tag|SIZE|ALIGN) format: find the first '|',
                // then parse digits until the next '|'.
                auto o_pos = entry.find("O(");
                if (o_pos != std::string_view::npos) {
                    auto pipe1 = entry.find('|', o_pos + 2);
                    if (pipe1 != std::string_view::npos) {
                        std::size_t k = pipe1 + 1;
                        while (k < entry.size() && entry[k] >= '0' && entry[k] <= '9') {
                            field_size = field_size * 10 + (entry[k] - '0');
                            ++k;
                        }
                    }
                }
            }
            if (field_size > 0) {
                intervals[count++] = {offset, offset + field_size};
            }
        }

        // Advance past comma
        pos = entry_end;
        if (pos < content.size() && content[pos] == ',') ++pos;
    }

    if (count == 0) return {false, false};

    // Sort intervals by start offset (insertion sort)
    for (std::size_t a = 1; a < count; ++a) {
        auto tmp = intervals[a];
        std::size_t b = a;
        while (b > 0 && intervals[b - 1].start > tmp.start) {
            intervals[b] = intervals[b - 1];
            --b;
        }
        intervals[b] = tmp;
    }

    // Merge intervals and check coverage of [0, total_size)
    std::size_t covered_end = 0;
    for (std::size_t f = 0; f < count; ++f) {
        if (intervals[f].start > covered_end)
            return {true, false};  // gap before this field
        if (intervals[f].end > covered_end)
            covered_end = intervals[f].end;
    }
    return {covered_end < total_size, false};  // tail padding check
}

/// Parse a layout signature to detect padding gaps at any nesting level.
///
/// Scans the ENTIRE signature string for ALL "record[s:SIZE,...]{...}"
/// blocks — including nested ones inside array element types and base
/// class signatures — and checks each for coverage gaps.  Returns
/// {true, ...} as soon as any record with padding is found.
///
/// Records nested inside union blocks are SKIPPED because union padding
/// is semantically ambiguous (depends on the active member at runtime).
///
/// The union_depth counter is maintained incrementally as the search
/// advances, avoiding the O(n²) cost of rescanning from the start.
constexpr SigPaddingResult sig_has_padding_impl(std::string_view sig) noexcept {
    constexpr std::string_view rec_needle = "record[s:";
    constexpr std::string_view uni_needle = "union[";

    // Incremental union depth tracking: scan forward through the
    // signature maintaining a running depth count of open union blocks.
    // This replaces the previous union_depth_at() which rescanned from
    // position 0 for each record found (O(n²) worst case).
    std::size_t search_pos = 0;
    std::size_t depth_scan_pos = 0;  // how far we've scanned for depth
    std::size_t union_depth = 0;

    // Advance depth tracking from depth_scan_pos up to (but not including)
    // target_pos.  Updates union_depth and depth_scan_pos.
    auto advance_depth_to = [&](std::size_t target_pos) {
        while (depth_scan_pos < target_pos) {
            // Check for "union[" tag
            if (depth_scan_pos + uni_needle.size() <= sig.size() &&
                sig.substr(depth_scan_pos, uni_needle.size()) == uni_needle) {
                auto brace = sig.find('{', depth_scan_pos + uni_needle.size());
                if (brace != std::string_view::npos && brace < target_pos) {
                    ++union_depth;
                    depth_scan_pos = brace + 1;
                    continue;
                }
            }
            if (union_depth > 0 && sig[depth_scan_pos] == '{') {
                ++union_depth;
            } else if (union_depth > 0 && sig[depth_scan_pos] == '}') {
                --union_depth;
            }
            ++depth_scan_pos;
        }
    };

    while (search_pos < sig.size()) {
        auto rec_pos = sig.find(rec_needle, search_pos);
        if (rec_pos == std::string_view::npos) break;

        // Update union depth up to this record position.
        advance_depth_to(rec_pos);

        if (union_depth > 0) {
            // Inside a union block — skip this record.
            search_pos = rec_pos + rec_needle.size();
            continue;
        }

        auto result = check_one_record(sig, rec_pos);
        if (result.has_padding) return result;

        search_pos = rec_pos + rec_needle.size();
    }

    return {false, false};
}

/// Public wrapper: returns true if the signature has padding.
/// Note: may return a conservative true for types with >512 leaf fields.
constexpr bool sig_has_padding(std::string_view sig) noexcept {
    return sig_has_padding_impl(sig).has_padding;
}

/// Returns true if the two padding analyses agree, or if the signature-based
/// analysis was truncated (too many fields to verify).  Used by layout_traits
/// cross-validation to avoid false assertion failures on very large types.
constexpr bool check_padding_consistency(bool ct_has_padding,
                                         std::string_view sig) noexcept {
    auto r = sig_has_padding_impl(sig);
    return r.truncated || (ct_has_padding == r.has_padding);
}

} // namespace detail
} // namespace typelayout
} // namespace boost

#endif // BOOST_TYPELAYOUT_DETAIL_SIG_PARSER_HPP
