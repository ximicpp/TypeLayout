// Boost.TypeLayout - Two-Layer Signature API (v2.0 Minimal Core)
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#ifndef BOOST_TYPELAYOUT_CORE_SIGNATURE_HPP
#define BOOST_TYPELAYOUT_CORE_SIGNATURE_HPP

#include <boost/typelayout/core/config.hpp>
#include <boost/typelayout/core/compile_string.hpp>
#include <boost/typelayout/core/type_signature.hpp>

namespace boost {
namespace typelayout {

// Architecture prefix: "[64-le]", "[64-be]", "[32-le]", "[32-be]"
[[nodiscard]] consteval auto get_arch_prefix() noexcept {
    if constexpr (sizeof(void*) == 8) {
        return CompileString{TYPELAYOUT_LITTLE_ENDIAN ? "[64-le]" : "[64-be]"};
    } else if constexpr (sizeof(void*) == 4) {
        return CompileString{TYPELAYOUT_LITTLE_ENDIAN ? "[32-le]" : "[32-be]"};
    } else {
        auto bits = CompileString<32>::from_number(sizeof(void*) * 8);
        return CompileString{"["} + bits + CompileString{TYPELAYOUT_LITTLE_ENDIAN ? "-le]" : "-be]"};
    }
}

// =============================================================================
// Layer 1: Layout Signature — Pure byte layout (flattened, no names)
// =============================================================================

template <typename T>
[[nodiscard]] consteval auto get_layout_signature() noexcept {
    return get_arch_prefix() + TypeSignature<T, SignatureMode::Layout>::calculate();
}

template <typename T1, typename T2>
[[nodiscard]] consteval bool layout_signatures_match() noexcept {
    return get_layout_signature<T1>() == get_layout_signature<T2>();
}

// =============================================================================
// Layer 2: Definition Signature — Full type definition (tree, with names)
// =============================================================================

template <typename T>
[[nodiscard]] consteval auto get_definition_signature() noexcept {
    return get_arch_prefix() + TypeSignature<T, SignatureMode::Definition>::calculate();
}

template <typename T1, typename T2>
[[nodiscard]] consteval bool definition_signatures_match() noexcept {
    return get_definition_signature<T1>() == get_definition_signature<T2>();
}

} // namespace typelayout
} // namespace boost

#endif // BOOST_TYPELAYOUT_CORE_SIGNATURE_HPP
