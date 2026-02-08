// Declarative Cross-Platform Compatibility Macros
//
// Phase 2 convenience macros for the two-phase pipeline.
// Provides TYPELAYOUT_CHECK_COMPAT (runtime report) and
// TYPELAYOUT_ASSERT_COMPAT (compile-time static_assert).
//
// C++17 compatible. No P2996 dependency.
//
// Usage (runtime report):
//   #include "sigs/x86_64_linux_clang.sig.hpp"
//   #include "sigs/arm64_macos_clang.sig.hpp"
//   #include <boost/typelayout/tools/compat_auto.hpp>
//   TYPELAYOUT_CHECK_COMPAT(x86_64_linux_clang, arm64_macos_clang)
//
// Usage (compile-time assert):
//   #include "sigs/x86_64_linux_clang.sig.hpp"
//   #include "sigs/arm64_macos_clang.sig.hpp"
//   #include <boost/typelayout/tools/compat_auto.hpp>
//   TYPELAYOUT_ASSERT_COMPAT(x86_64_linux_clang, arm64_macos_clang)
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#ifndef BOOST_TYPELAYOUT_TOOLS_COMPAT_AUTO_HPP
#define BOOST_TYPELAYOUT_TOOLS_COMPAT_AUTO_HPP

#include <boost/typelayout/tools/sig_types.hpp>
#include <boost/typelayout/tools/compat_check.hpp>

// Reuse FOR_EACH machinery from sig_export.hpp if already included,
// otherwise define it here.
#ifndef TYPELAYOUT_DETAIL_FOR_EACH

#define TYPELAYOUT_DETAIL_EXPAND(x) x
#define TYPELAYOUT_DETAIL_CAT_(a, b) a##b
#define TYPELAYOUT_DETAIL_CAT(a, b) TYPELAYOUT_DETAIL_CAT_(a, b)

#define TYPELAYOUT_DETAIL_NARG_(...) \
    TYPELAYOUT_DETAIL_EXPAND(TYPELAYOUT_DETAIL_ARG_N(__VA_ARGS__))
#define TYPELAYOUT_DETAIL_ARG_N( \
     _1, _2, _3, _4, _5, _6, _7, _8, _9,_10, \
    _11,_12,_13,_14,_15,_16,_17,_18,_19,_20, \
    _21,_22,_23,_24,_25,_26,_27,_28,_29,_30, \
    _31,_32, N, ...) N
#define TYPELAYOUT_DETAIL_RSEQ_N() \
    32,31,30,29,28,27,26,25,24,23, \
    22,21,20,19,18,17,16,15,14,13, \
    12,11,10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0
#define TYPELAYOUT_DETAIL_NARG(...) \
    TYPELAYOUT_DETAIL_NARG_(__VA_ARGS__, TYPELAYOUT_DETAIL_RSEQ_N())

#define TYPELAYOUT_DETAIL_FE_1(m, x)      m(x)
#define TYPELAYOUT_DETAIL_FE_2(m, x, ...) m(x) TYPELAYOUT_DETAIL_EXPAND(TYPELAYOUT_DETAIL_FE_1(m, __VA_ARGS__))
#define TYPELAYOUT_DETAIL_FE_3(m, x, ...) m(x) TYPELAYOUT_DETAIL_EXPAND(TYPELAYOUT_DETAIL_FE_2(m, __VA_ARGS__))
#define TYPELAYOUT_DETAIL_FE_4(m, x, ...) m(x) TYPELAYOUT_DETAIL_EXPAND(TYPELAYOUT_DETAIL_FE_3(m, __VA_ARGS__))
#define TYPELAYOUT_DETAIL_FE_5(m, x, ...) m(x) TYPELAYOUT_DETAIL_EXPAND(TYPELAYOUT_DETAIL_FE_4(m, __VA_ARGS__))
#define TYPELAYOUT_DETAIL_FE_6(m, x, ...) m(x) TYPELAYOUT_DETAIL_EXPAND(TYPELAYOUT_DETAIL_FE_5(m, __VA_ARGS__))
#define TYPELAYOUT_DETAIL_FE_7(m, x, ...) m(x) TYPELAYOUT_DETAIL_EXPAND(TYPELAYOUT_DETAIL_FE_6(m, __VA_ARGS__))
#define TYPELAYOUT_DETAIL_FE_8(m, x, ...) m(x) TYPELAYOUT_DETAIL_EXPAND(TYPELAYOUT_DETAIL_FE_7(m, __VA_ARGS__))
#define TYPELAYOUT_DETAIL_FE_9(m, x, ...) m(x) TYPELAYOUT_DETAIL_EXPAND(TYPELAYOUT_DETAIL_FE_8(m, __VA_ARGS__))
#define TYPELAYOUT_DETAIL_FE_10(m, x, ...) m(x) TYPELAYOUT_DETAIL_EXPAND(TYPELAYOUT_DETAIL_FE_9(m, __VA_ARGS__))
#define TYPELAYOUT_DETAIL_FE_11(m, x, ...) m(x) TYPELAYOUT_DETAIL_EXPAND(TYPELAYOUT_DETAIL_FE_10(m, __VA_ARGS__))
#define TYPELAYOUT_DETAIL_FE_12(m, x, ...) m(x) TYPELAYOUT_DETAIL_EXPAND(TYPELAYOUT_DETAIL_FE_11(m, __VA_ARGS__))
#define TYPELAYOUT_DETAIL_FE_13(m, x, ...) m(x) TYPELAYOUT_DETAIL_EXPAND(TYPELAYOUT_DETAIL_FE_12(m, __VA_ARGS__))
#define TYPELAYOUT_DETAIL_FE_14(m, x, ...) m(x) TYPELAYOUT_DETAIL_EXPAND(TYPELAYOUT_DETAIL_FE_13(m, __VA_ARGS__))
#define TYPELAYOUT_DETAIL_FE_15(m, x, ...) m(x) TYPELAYOUT_DETAIL_EXPAND(TYPELAYOUT_DETAIL_FE_14(m, __VA_ARGS__))
#define TYPELAYOUT_DETAIL_FE_16(m, x, ...) m(x) TYPELAYOUT_DETAIL_EXPAND(TYPELAYOUT_DETAIL_FE_15(m, __VA_ARGS__))

#define TYPELAYOUT_DETAIL_FOR_EACH(macro, ...)                          \
    TYPELAYOUT_DETAIL_EXPAND(                                           \
        TYPELAYOUT_DETAIL_CAT(TYPELAYOUT_DETAIL_FE_,                    \
            TYPELAYOUT_DETAIL_NARG(__VA_ARGS__))(macro, __VA_ARGS__))

#endif // TYPELAYOUT_DETAIL_FOR_EACH

// =========================================================================
// TYPELAYOUT_CHECK_COMPAT(...) — Runtime report
//
// Generates main() that prints a compatibility report.
// =========================================================================

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

// =========================================================================
// TYPELAYOUT_ASSERT_COMPAT(...) — Compile-time static_assert
//
// Asserts that ALL types have identical layout signatures across all
// listed platforms. Compilation fails if any type differs.
//
// Implementation: compares every platform's type_count and each type's
// layout_sig against the first platform. Uses a constexpr helper.
// =========================================================================

namespace boost {
namespace typelayout {
namespace compat {
namespace detail {

/// Constexpr comparison of all types between two platforms.
inline constexpr bool all_layouts_match(const PlatformInfo& a,
                                        const PlatformInfo& b) {
    if (a.type_count != b.type_count) return false;
    for (int i = 0; i < a.type_count; ++i) {
        // Compare layout signatures character by character
        const char* sa = a.types[i].layout_sig;
        const char* sb = b.types[i].layout_sig;
        int j = 0;
        while (sa[j] != '\0' && sb[j] != '\0') {
            if (sa[j] != sb[j]) return false;
            ++j;
        }
        if (sa[j] != sb[j]) return false; // different lengths
    }
    return true;
}

} // namespace detail
} // namespace compat
} // namespace typelayout
} // namespace boost

// Assert each platform pair against the first.
// We expand a static_assert for each platform after the first.
#define TYPELAYOUT_DETAIL_ASSERT_FIRST(ns) /* first platform — nothing to assert */

#define TYPELAYOUT_DETAIL_ASSERT_VS_FIRST(ns)                                   \
    static_assert(                                                               \
        ::boost::typelayout::compat::detail::all_layouts_match(                  \
            ::boost::typelayout::platform::                                      \
                TYPELAYOUT_DETAIL_ASSERT_REF_NS::get_platform_info(),            \
            ::boost::typelayout::platform::ns::get_platform_info()),             \
        "TypeLayout: layout mismatch between "                                   \
        #ns " and reference platform");

// Two-pass: first macro captures the reference, rest assert against it.
// We use a helper that stores the first arg as the reference namespace.
#define TYPELAYOUT_ASSERT_COMPAT_2(ref, ...)                                    \
    namespace { namespace typelayout_assert_detail {                             \
        inline constexpr auto ref_info =                                        \
            ::boost::typelayout::platform::ref::get_platform_info();            \
    }}                                                                          \
    TYPELAYOUT_DETAIL_ASSERT_COMPAT_EACH(ref, __VA_ARGS__)

#define TYPELAYOUT_DETAIL_ASSERT_PAIR(ref, other)                               \
    static_assert(                                                               \
        ::boost::typelayout::compat::detail::all_layouts_match(                  \
            ::boost::typelayout::platform::ref::get_platform_info(),             \
            ::boost::typelayout::platform::other::get_platform_info()),          \
        "TypeLayout: layout mismatch between " #ref " and " #other);

// For 2 platforms
#define TYPELAYOUT_DETAIL_ASSERT_COMPAT_EACH_1(ref, x) \
    TYPELAYOUT_DETAIL_ASSERT_PAIR(ref, x)
// For 3 platforms
#define TYPELAYOUT_DETAIL_ASSERT_COMPAT_EACH_2(ref, x, ...) \
    TYPELAYOUT_DETAIL_ASSERT_PAIR(ref, x) \
    TYPELAYOUT_DETAIL_EXPAND(TYPELAYOUT_DETAIL_ASSERT_COMPAT_EACH_1(ref, __VA_ARGS__))
// For 4 platforms
#define TYPELAYOUT_DETAIL_ASSERT_COMPAT_EACH_3(ref, x, ...) \
    TYPELAYOUT_DETAIL_ASSERT_PAIR(ref, x) \
    TYPELAYOUT_DETAIL_EXPAND(TYPELAYOUT_DETAIL_ASSERT_COMPAT_EACH_2(ref, __VA_ARGS__))
// For 5 platforms
#define TYPELAYOUT_DETAIL_ASSERT_COMPAT_EACH_4(ref, x, ...) \
    TYPELAYOUT_DETAIL_ASSERT_PAIR(ref, x) \
    TYPELAYOUT_DETAIL_EXPAND(TYPELAYOUT_DETAIL_ASSERT_COMPAT_EACH_3(ref, __VA_ARGS__))
// For 6 platforms
#define TYPELAYOUT_DETAIL_ASSERT_COMPAT_EACH_5(ref, x, ...) \
    TYPELAYOUT_DETAIL_ASSERT_PAIR(ref, x) \
    TYPELAYOUT_DETAIL_EXPAND(TYPELAYOUT_DETAIL_ASSERT_COMPAT_EACH_4(ref, __VA_ARGS__))
// For 7 platforms
#define TYPELAYOUT_DETAIL_ASSERT_COMPAT_EACH_6(ref, x, ...) \
    TYPELAYOUT_DETAIL_ASSERT_PAIR(ref, x) \
    TYPELAYOUT_DETAIL_EXPAND(TYPELAYOUT_DETAIL_ASSERT_COMPAT_EACH_5(ref, __VA_ARGS__))
// For 8 platforms
#define TYPELAYOUT_DETAIL_ASSERT_COMPAT_EACH_7(ref, x, ...) \
    TYPELAYOUT_DETAIL_ASSERT_PAIR(ref, x) \
    TYPELAYOUT_DETAIL_EXPAND(TYPELAYOUT_DETAIL_ASSERT_COMPAT_EACH_6(ref, __VA_ARGS__))

#define TYPELAYOUT_DETAIL_ASSERT_NARG_(...) \
    TYPELAYOUT_DETAIL_EXPAND(TYPELAYOUT_DETAIL_ASSERT_ARG_N(__VA_ARGS__))
#define TYPELAYOUT_DETAIL_ASSERT_ARG_N(_1,_2,_3,_4,_5,_6,_7, N, ...) N

#define TYPELAYOUT_DETAIL_ASSERT_COMPAT_EACH(ref, ...)                          \
    TYPELAYOUT_DETAIL_EXPAND(                                                   \
        TYPELAYOUT_DETAIL_CAT(TYPELAYOUT_DETAIL_ASSERT_COMPAT_EACH_,            \
            TYPELAYOUT_DETAIL_ASSERT_NARG_(__VA_ARGS__, 7,6,5,4,3,2,1)          \
        )(ref, __VA_ARGS__))

/// Main entry: TYPELAYOUT_ASSERT_COMPAT(plat1, plat2, ...)
/// First platform is the reference; all others are compared against it.
#define TYPELAYOUT_ASSERT_COMPAT(first, ...)                                    \
    TYPELAYOUT_DETAIL_ASSERT_COMPAT_EACH(first, __VA_ARGS__)

#endif // BOOST_TYPELAYOUT_TOOLS_COMPAT_AUTO_HPP
