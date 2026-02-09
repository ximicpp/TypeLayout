// TypeEntry and PlatformInfo — shared between .sig.hpp files and compat_check.hpp.
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#ifndef BOOST_TYPELAYOUT_TOOLS_SIG_TYPES_HPP
#define BOOST_TYPELAYOUT_TOOLS_SIG_TYPES_HPP

#include <cstddef>

namespace boost {
namespace typelayout {

struct TypeEntry {
    const char* name;
    const char* layout_sig;
    const char* definition_sig;
};

struct PlatformInfo {
    const char*      platform_name;
    const char*      arch_prefix;
    const TypeEntry* types;
    int              type_count;
    std::size_t      pointer_size;
    std::size_t      sizeof_long;
    std::size_t      sizeof_wchar_t;
    std::size_t      sizeof_long_double;
    std::size_t      max_align;
};

} // namespace typelayout
} // namespace boost

#endif // BOOST_TYPELAYOUT_TOOLS_SIG_TYPES_HPP
