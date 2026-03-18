// sig_parser.hpp -- Signature string parsing.
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#ifndef BOOST_TYPELAYOUT_DETAIL_SIG_PARSER_HPP
#define BOOST_TYPELAYOUT_DETAIL_SIG_PARSER_HPP

#include <string_view>
#include <cstddef>

namespace boost {
namespace typelayout {
inline namespace v1 {
namespace detail {

/// Token-boundary-aware find: matches only when the preceding char is not alnum.
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
        pos = found + 1;
    }
    return false;
}

/// Result of padding analysis.  `truncated` is true when the field count
/// exceeds MAX_FIELDS and the result is a conservative approximation.
struct SigPaddingResult {
    bool has_padding;
    bool truncated;
};

/// Check one "record[s:N,...]{...}" block for coverage gaps.
constexpr SigPaddingResult check_one_record(std::string_view sig,
                                            std::size_t rec_pos) noexcept {
    // Phase 1: Parse record total size from "record[s:N..."
    std::size_t total_size = 0;
    std::size_t i = rec_pos + 9;  // length of "record[s:"
    while (i < sig.size() && sig[i] >= '0' && sig[i] <= '9') {
        total_size = total_size * 10 + (sig[i] - '0');
        ++i;
    }
    if (total_size == 0) return {false, false};

    // Phase 2: Find brace-delimited field body
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

    // Phase 3: Split on depth-0 commas, extract offset + size per field
    struct Interval { std::size_t start; std::size_t end; };
    constexpr std::size_t MAX_FIELDS = 2048;  // conservative limit for constexpr stack
    Interval intervals[MAX_FIELDS]{};
    std::size_t count = 0;

    std::size_t pos = 0;
    while (pos < content.size()) {
        if (count >= MAX_FIELDS)
            return {true, true};  // too many fields: conservative
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

        if (entry.size() > 1 && entry[0] == '@') {
            std::size_t offset = 0;
            std::size_t j = 1;
            while (j < entry.size() && entry[j] >= '0' && entry[j] <= '9') {
                offset = offset * 10 + (entry[j] - '0');
                ++j;
            }

            std::size_t field_size = 0;
            auto s_pos = entry.find("[s:");
            if (s_pos != std::string_view::npos) {
                std::size_t k = s_pos + 3;
                while (k < entry.size() && entry[k] >= '0' && entry[k] <= '9') {
                    field_size = field_size * 10 + (entry[k] - '0');
                    ++k;
                }
            } else {
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

        pos = entry_end;
        if (pos < content.size() && content[pos] == ',') ++pos;
    }

    if (count == 0) return {false, false};

    // Phase 4: Insertion sort intervals by start offset (constexpr-safe)
    for (std::size_t a = 1; a < count; ++a) {
        auto tmp = intervals[a];
        std::size_t b = a;
        while (b > 0 && intervals[b - 1].start > tmp.start) {
            intervals[b] = intervals[b - 1];
            --b;
        }
        intervals[b] = tmp;
    }

    // Phase 5: Scan sorted intervals for coverage gaps
    std::size_t covered_end = 0;
    for (std::size_t f = 0; f < count; ++f) {
        if (intervals[f].start > covered_end)
            return {true, false};  // gap before this field
        if (intervals[f].end > covered_end)
            covered_end = intervals[f].end;
    }
    return {covered_end < total_size, false};  // tail padding check
}

/// Scan all "record[s:...]" blocks in the signature for padding gaps.
/// Skips records nested inside union blocks.
constexpr SigPaddingResult sig_has_padding_impl(std::string_view sig) noexcept {
    constexpr std::string_view rec_needle = "record[s:";
    constexpr std::string_view uni_needle = "union[";

    std::size_t search_pos = 0;
    std::size_t depth_scan_pos = 0;  // how far we've scanned for depth
    std::size_t union_depth = 0;

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

        advance_depth_to(rec_pos);

        if (union_depth > 0) {
            search_pos = rec_pos + rec_needle.size();
            continue;
        }

        auto result = check_one_record(sig, rec_pos);
        if (result.has_padding) return result;

        search_pos = rec_pos + rec_needle.size();
    }

    return {false, false};
}

constexpr bool sig_has_padding(std::string_view sig) noexcept {
    return sig_has_padding_impl(sig).has_padding;
}

// Returns true if the cross-validation passes.
// When truncated (> MAX_FIELDS), we conservatively skip the check
// because the parser cannot accurately determine padding for very
// large flattened types.  Increase MAX_FIELDS if this is hit.
constexpr bool check_padding_consistency(bool ct_has_padding,
                                         std::string_view sig) noexcept {
    auto r = sig_has_padding_impl(sig);
    return r.truncated || (ct_has_padding == r.has_padding);
}

} // namespace detail
} // inline namespace v1
} // namespace typelayout
} // namespace boost

#endif // BOOST_TYPELAYOUT_DETAIL_SIG_PARSER_HPP
