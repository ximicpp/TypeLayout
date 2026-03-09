// safety_level.hpp -- Unified safety classification enum and runtime
// signature-based classifier.
//
// This header does NOT require P2996 -- it is pure C++17.
// It provides the shared SafetyLevel enum and a runtime function
// classify_signature(string_view) that scans a layout signature string
// to produce a safety classification.
//
// Used by:
//   - classify.hpp       (compile-time classifier, wraps layout_traits)
//   - compat_check.hpp   (runtime cross-platform reporter)
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#ifndef BOOST_TYPELAYOUT_TOOLS_SAFETY_LEVEL_HPP
#define BOOST_TYPELAYOUT_TOOLS_SAFETY_LEVEL_HPP

#include <string_view>
#include <cstddef>

namespace boost {
namespace typelayout {

// =========================================================================
// SafetyLevel -- five-tier safety classification
// =========================================================================

/// Safety classification for zero-copy / memcpy / cross-boundary transfer.
///
/// Ordered from best (TrivialSafe = 0) to worst (Opaque = 4).
/// Higher integer value = higher risk.  classify<T> returns the
/// WORST (highest) applicable level.
///
/// Severity ordering (worst to best):
///   Opaque > PointerRisk > PlatformVariant > PaddingRisk > TrivialSafe
///
/// PointerRisk (3) > PlatformVariant (2) because pointer-containing types
/// cause dangling pointers / double-free on memcpy — a hard semantic error.
/// PlatformVariant types are merely layout-unstable across platforms, which
/// is a portability concern but not an immediate memory safety violation.
enum class SafetyLevel {
    /// The type is safe for memcpy, cross-process, and cross-platform transfer.
    /// No pointers, no padding, trivially copyable, platform-independent layout.
    TrivialSafe,     // = 0

    /// Layout is fixed and portable, but padding bytes exist.
    /// memcpy works correctly, but padding may leak uninitialized memory
    /// (information disclosure risk in serialization/network scenarios).
    PaddingRisk,     // = 1

    /// Layout differs across platforms due to platform-dependent types
    /// (wchar_t, long double / f80, bit-fields).
    /// Same-platform memcpy may be fine, but cross-platform transfer is unsafe.
    PlatformVariant, // = 2

    /// Layout contains pointer-like fields (ptr, fnptr, memptr, ref, rref).
    /// Byte-level copy produces dangling pointers or double-free scenarios.
    /// Also used conservatively when !is_trivially_copyable (e.g. vtable).
    PointerRisk,     // = 3

    /// Contains opaque (unanalyzable) fields.  Safety cannot be determined.
    /// The user is responsible for manual verification.
    Opaque,          // = 4
};

// =========================================================================
// safety_level_name -- human-readable label
// =========================================================================

constexpr const char* safety_level_name(SafetyLevel level) noexcept {
    switch (level) {
        case SafetyLevel::TrivialSafe:     return "TrivialSafe";
        case SafetyLevel::PaddingRisk:     return "PaddingRisk";
        case SafetyLevel::PointerRisk:     return "PointerRisk";
        case SafetyLevel::PlatformVariant: return "PlatformVariant";
        case SafetyLevel::Opaque:          return "Opaque";
    }
    return "?";
}

// =========================================================================
// Runtime signature classification
//
// Scans a layout signature string (std::string_view) to determine its
// safety level.  This is the runtime counterpart of classify<T> which
// works at compile time using layout_traits<T>.
//
// This function does NOT require P2996 -- it only performs string
// matching on signature patterns.
// =========================================================================

namespace detail {

/// Token-boundary-aware substring search for std::string_view.
///
/// Finds `needle` in `haystack`, but only accepts a match when the character
/// immediately before it is NOT an ASCII letter.  This prevents e.g.
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
            return {true, true};  // too many fields: conservative, result may be a false positive
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

            // Find [s:SIZE in this entry to get the field size
            auto s_pos = entry.find("[s:");
            if (s_pos != std::string_view::npos) {
                std::size_t field_size = 0;
                std::size_t k = s_pos + 3;
                while (k < entry.size() && entry[k] >= '0' && entry[k] <= '9') {
                    field_size = field_size * 10 + (entry[k] - '0');
                    ++k;
                }
                if (field_size > 0) {
                    intervals[count++] = {offset, offset + field_size};
                }
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

/// Helper: compute the "union nesting depth" at a given position.
/// Scans sig[0..pos) and counts how many union block braces are open.
/// A union block starts with "union[" ... "{" and ends with the matching "}".
constexpr std::size_t union_depth_at(std::string_view sig,
                                     std::size_t pos) noexcept {
    constexpr std::string_view union_tag = "union[";
    std::size_t depth = 0;
    std::size_t i = 0;
    while (i < pos) {
        if (i + union_tag.size() <= sig.size() &&
            sig.substr(i, union_tag.size()) == union_tag) {
            // Found "union[" — skip to its '{' to enter the block.
            auto brace = sig.find('{', i + union_tag.size());
            if (brace != std::string_view::npos && brace < pos) {
                ++depth;
                i = brace + 1;
                continue;
            }
        }
        if (depth > 0 && sig[i] == '{') {
            ++depth;
        } else if (depth > 0 && sig[i] == '}') {
            --depth;
        }
        ++i;
    }
    return depth;
}

/// Parse a layout signature to detect padding gaps at any nesting level.
///
/// Scans the ENTIRE signature string for ALL "record[s:SIZE,...]{...}"
/// blocks — including nested ones inside array element types and base
/// class signatures — and checks each for coverage gaps.  Returns
/// {true, ...} as soon as any record with padding is found.
///
/// This detects padding inside array elements such as:
///   struct Foo { PaddedStruct arr[2]; };
/// where the outer record has no outer gap, but the element type does.
///
/// Records nested inside union blocks are SKIPPED because union padding
/// is semantically ambiguous (depends on the active member at runtime).
/// See compute_has_padding in layout_traits.hpp for rationale.
///
/// Returns {false, false} if no record with padding is found.
/// Returns {true, true}  if any record exceeded MAX_FIELDS (conservative).
constexpr SigPaddingResult sig_has_padding_impl(std::string_view sig) noexcept {
    std::size_t search_pos = 0;
    constexpr std::string_view needle = "record[s:";
    bool any_truncated = false;

    while (search_pos < sig.size()) {
        auto rec_pos = sig.find(needle, search_pos);
        if (rec_pos == std::string_view::npos) break;

        // Skip records nested inside union blocks.
        if (union_depth_at(sig, rec_pos) > 0) {
            search_pos = rec_pos + needle.size();
            continue;
        }

        auto result = check_one_record(sig, rec_pos);
        if (result.has_padding) return result;
        if (result.truncated) any_truncated = true;

        // Advance past this "record[s:" to find the next one.
        search_pos = rec_pos + needle.size();
    }

    return {false, any_truncated};
}

/// Public wrapper: returns true if the signature has padding.
/// Note: may return a conservative true for types with >512 leaf fields.
/// Use sig_has_padding_impl() directly to check whether the result is reliable.
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

/// Classify a layout signature string at runtime.
///
/// Priority order (matches the compile-time classify<T>):
///   1. Opaque          -- contains "O(" or "O!" marker
///   2. PointerRisk     -- ptr[, fnptr[, memptr[, ref[, rref[
///   3. PlatformVariant -- wchar[, f80[, bits<
///   4. PaddingRisk     -- record size exceeds coverage of leaf fields
///   5. TrivialSafe     -- none of the above
///
/// Rationale for PointerRisk > PlatformVariant:
///   Pointers make memcpy semantically incorrect (dangling pointers)
///   on ANY platform, which is a more severe and actionable risk than
///   cross-platform size differences.  This ordering matches the
///   compile-time classifier in classify.hpp.
///
/// PaddingRisk is detected by parsing the signature to find gaps between
/// leaf field entries.  The record's total size (from the "record[s:N,...]"
/// header) is compared against the coverage of all "@offset:type[s:N,...]"
/// entries.  Any uncovered byte indicates padding.
inline SafetyLevel classify_signature(std::string_view sig) noexcept {
    using detail::sig_contains_token;

    // 1. Opaque: look for opaque markers.
    //    "O(" -- new-style (TYPELAYOUT_REGISTER_OPAQUE)
    //    "O!" -- legacy-style (TYPELAYOUT_OPAQUE_TYPE / _CONTAINER / _MAP)
    if (sig.find("O(") != std::string_view::npos ||
        sig.find("O!") != std::string_view::npos)
        return SafetyLevel::Opaque;

    // 2. Pointer risk: pointer-like fields make memcpy produce dangling refs.
    //    Pointers are also platform-variant (32 vs 64 bit), but the
    //    dangling-pointer risk is more severe and takes priority.
    bool has_pointer =
        sig_contains_token(sig, "ptr[") ||
        sig_contains_token(sig, "fnptr[") ||
        sig_contains_token(sig, "memptr[") ||
        sig_contains_token(sig, "ref[") ||
        sig_contains_token(sig, "rref[");

    if (has_pointer)
        return SafetyLevel::PointerRisk;

    // 3. Union types: inspect their content for pointer-like fields.
    //    Only classify as PointerRisk if the union body actually contains
    //    pointer/reference markers.  A union of plain integers is safe.
    //    Note: this is a conservative approximation -- the runtime
    //    classifier cannot fully reconstruct type-level semantics.
    //    Unions whose content cannot be parsed are left for later checks.
    // (union content is examined by the pointer/platform checks above
    //  and the padding check below -- no special union gate needed here)

    // 4. Platform-variant: bit-fields and platform-dependent primitive types
    bool has_platform_variant =
        sig.find("bits<") != std::string_view::npos ||
        sig.find("wchar[") != std::string_view::npos ||
        sig.find("f80[") != std::string_view::npos;

    if (has_platform_variant)
        return SafetyLevel::PlatformVariant;

    // 5. Padding risk: record has uncovered bytes between or after fields
    if (detail::sig_has_padding(sig))
        return SafetyLevel::PaddingRisk;

    // 6. TrivialSafe
    return SafetyLevel::TrivialSafe;
}

} // namespace typelayout
} // namespace boost

#endif // BOOST_TYPELAYOUT_TOOLS_SAFETY_LEVEL_HPP
