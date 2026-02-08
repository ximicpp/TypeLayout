// Shared type definitions for the cross-platform compatibility toolchain.
//
// This header defines TypeEntry and PlatformInfo — the canonical types
// shared between Phase 1 (.sig.hpp generation) and Phase 2 (compatibility
// checking). Both generated .sig.hpp files and compat_check.hpp include
// this header, eliminating duplicate definitions.
//
// C++17 compatible. No P2996 dependency.
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#ifndef BOOST_TYPELAYOUT_TOOLS_SIG_TYPES_HPP
#define BOOST_TYPELAYOUT_TOOLS_SIG_TYPES_HPP

#include <cstddef>

namespace boost {
namespace typelayout {

/// A single type's signature data, as stored in generated .sig.hpp files.
struct TypeEntry {
    const char* name;
    const char* layout_sig;
    const char* definition_sig;
};

/// Complete platform metadata and type registry.
/// Returned by get_platform_info() in each generated .sig.hpp.
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
