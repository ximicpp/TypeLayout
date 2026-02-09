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

#define TYPELAYOUT_DETAIL_ASSERT_COMPAT_EACH_1(ref, x) \
    TYPELAYOUT_DETAIL_ASSERT_PAIR(ref, x)
#define TYPELAYOUT_DETAIL_ASSERT_COMPAT_EACH_2(ref, x, ...) \
    TYPELAYOUT_DETAIL_ASSERT_PAIR(ref, x) \
    TYPELAYOUT_DETAIL_EXPAND(TYPELAYOUT_DETAIL_ASSERT_COMPAT_EACH_1(ref, __VA_ARGS__))
#define TYPELAYOUT_DETAIL_ASSERT_COMPAT_EACH_3(ref, x, ...) \
    TYPELAYOUT_DETAIL_ASSERT_PAIR(ref, x) \
    TYPELAYOUT_DETAIL_EXPAND(TYPELAYOUT_DETAIL_ASSERT_COMPAT_EACH_2(ref, __VA_ARGS__))
#define TYPELAYOUT_DETAIL_ASSERT_COMPAT_EACH_4(ref, x, ...) \
    TYPELAYOUT_DETAIL_ASSERT_PAIR(ref, x) \
    TYPELAYOUT_DETAIL_EXPAND(TYPELAYOUT_DETAIL_ASSERT_COMPAT_EACH_3(ref, __VA_ARGS__))
#define TYPELAYOUT_DETAIL_ASSERT_COMPAT_EACH_5(ref, x, ...) \
    TYPELAYOUT_DETAIL_ASSERT_PAIR(ref, x) \
    TYPELAYOUT_DETAIL_EXPAND(TYPELAYOUT_DETAIL_ASSERT_COMPAT_EACH_4(ref, __VA_ARGS__))
#define TYPELAYOUT_DETAIL_ASSERT_COMPAT_EACH_6(ref, x, ...) \
    TYPELAYOUT_DETAIL_ASSERT_PAIR(ref, x) \
    TYPELAYOUT_DETAIL_EXPAND(TYPELAYOUT_DETAIL_ASSERT_COMPAT_EACH_5(ref, __VA_ARGS__))
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

#define TYPELAYOUT_ASSERT_COMPAT(first, ...)                                    \
    TYPELAYOUT_DETAIL_ASSERT_COMPAT_EACH(first, __VA_ARGS__)

#endif // BOOST_TYPELAYOUT_TOOLS_COMPAT_AUTO_HPP