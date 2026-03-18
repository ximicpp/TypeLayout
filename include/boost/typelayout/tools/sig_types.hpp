// TypeEntry and PlatformInfo — shared between .sig.hpp files and compat_check.hpp.
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#ifndef BOOST_TYPELAYOUT_TOOLS_SIG_TYPES_HPP
#define BOOST_TYPELAYOUT_TOOLS_SIG_TYPES_HPP

#include <cstddef>

namespace boost {
namespace typelayout {
inline namespace v1 {

struct TypeEntry {
    const char* name;
    const char* layout_sig;
};

struct PlatformInfo {
    const char*      platform_name;
    const char*      arch_prefix;
    const TypeEntry* types;
    std::size_t      type_count;
    std::size_t      pointer_size;
    std::size_t      sizeof_long;
    std::size_t      sizeof_wchar_t;
    std::size_t      sizeof_long_double;
    std::size_t      max_align;
    const char*      data_model;        // "LP64", "LLP64", "ILP32", etc.
};

} // inline namespace v1
} // namespace typelayout
} // namespace boost

#endif // BOOST_TYPELAYOUT_TOOLS_SIG_TYPES_HPP
