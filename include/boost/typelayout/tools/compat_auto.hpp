// Convenience macros: TYPELAYOUT_CHECK_COMPAT (runtime report) and
// TYPELAYOUT_ASSERT_COMPAT (compile-time static_assert). C++17 only.
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#ifndef BOOST_TYPELAYOUT_TOOLS_COMPAT_AUTO_HPP
#define BOOST_TYPELAYOUT_TOOLS_COMPAT_AUTO_HPP

#include <boost/typelayout/tools/sig_types.hpp>
#include <boost/typelayout/tools/compat_check.hpp>
#include <boost/typelayout/tools/detail/foreach.hpp>

// Generates main() that prints a compatibility report.

#define TYPELAYOUT_DETAIL_ADD_PLATFORM(ns)                              \
    reporter.add_platform(                                              \
        ::boost::typelayout::platform::ns::get_platform_info());

#define TYPELAYOUT_CHECK_COMPAT(...)                                    \
    int main() {                                                        \
        ::boost::typelayout::compat::CompatReporter reporter;           \
        TYPELAYOUT_DETAIL_FOR_EACH(TYPELAYOUT_DETAIL_ADD_PLATFORM,      \
                                   __VA_ARGS__)                         \
        reporter.print_report();                                        \
        return 0;                                                       \
    }

// static_assert that all types match across listed platforms.

namespace boost {
namespace typelayout {
namespace compat {
namespace detail {

inline constexpr bool all_layouts_match(const PlatformInfo& a,
                                        const PlatformInfo& b) {
    if (a.type_count != b.type_count) return false;
    for (int i = 0; i < a.type_count; ++i) {
        const char* sa = a.types[i].layout_sig;
        const char* sb = b.types[i].layout_sig;
        int j = 0;
        while (sa[j] != '\0' && sb[j] != '\0') {
            if (sa[j] != sb[j]) return false;
            ++j;
        }
        if (sa[j] != sb[j]) return false;
    }
    return true;
}

/// Check C1 (layout match) AND C2 (safety == Safe) for all types.
/// This is the compile-time ZST (Zero-Serialization Transfer) predicate:
///   ZST(TypeSet, Ps, Pd) ⟺ C1 ∧ C2 ∧ A1
/// where A1 (IEEE 754) is a documented background axiom.
inline constexpr bool all_serialization_free(const PlatformInfo& a,
                                              const PlatformInfo& b) {
    if (a.type_count != b.type_count) return false;
    for (int i = 0; i < a.type_count; ++i) {
        const char* sa = a.types[i].layout_sig;
        const char* sb = b.types[i].layout_sig;
        // C1: layout signature match
        int j = 0;
        while (sa[j] != '\0' && sb[j] != '\0') {
            if (sa[j] != sb[j]) return false;
            ++j;
        }
        if (sa[j] != sb[j]) return false;
        // C2: safety classification == Safe
        if (classify_safety(std::string_view(sa)) != SafetyLevel::Safe)
            return false;
    }
    return true;
}

} // namespace detail
} // namespace compat
} // namespace typelayout
} // namespace boost

#define TYPELAYOUT_DETAIL_ASSERT_PAIR(ref, other)                               \
    static_assert(                                                               \
        ::boost::typelayout::compat::detail::all_layouts_match(                  \
            ::boost::typelayout::platform::ref::get_platform_info(),             \
            ::boost::typelayout::platform::other::get_platform_info()),          \
        "TypeLayout: layout mismatch between " #ref " and " #other);

// Reuses FOR_EACH_CTX from foreach.hpp -- supports up to 32 platforms
// (previously hand-rolled to 7).

#define TYPELAYOUT_ASSERT_COMPAT(first, ...)                                    \
    TYPELAYOUT_DETAIL_FOR_EACH_CTX(TYPELAYOUT_DETAIL_ASSERT_PAIR,               \
                                   first, __VA_ARGS__)

// static_assert that all types are serialization-free (C1 + C2) across
// listed platforms. Unlike TYPELAYOUT_ASSERT_COMPAT which only checks C1
// (layout match), this also verifies C2 (safety == Safe), i.e., no
// pointers, bit-fields, unions, wchar_t, long double, or vptr.

#define TYPELAYOUT_DETAIL_ASSERT_ZST_PAIR(ref, other)                           \
    static_assert(                                                               \
        ::boost::typelayout::compat::detail::all_serialization_free(             \
            ::boost::typelayout::platform::ref::get_platform_info(),             \
            ::boost::typelayout::platform::other::get_platform_info()),          \
        "TypeLayout: serialization-free check failed between " #ref             \
        " and " #other " (layout mismatch or unsafe type detected)");

#define TYPELAYOUT_ASSERT_SERIALIZATION_FREE(first, ...)                        \
    TYPELAYOUT_DETAIL_FOR_EACH_CTX(TYPELAYOUT_DETAIL_ASSERT_ZST_PAIR,           \
                                   first, __VA_ARGS__)

#endif // BOOST_TYPELAYOUT_TOOLS_COMPAT_AUTO_HPP
