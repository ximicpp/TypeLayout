// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#ifndef BOOST_TYPELAYOUT_CORE_SIGNATURE_HPP
#define BOOST_TYPELAYOUT_CORE_SIGNATURE_HPP

#include <boost/typelayout/core/config.hpp>
#include <boost/typelayout/core/compile_string.hpp>
#include <boost/typelayout/core/signature_detail.hpp>

namespace boost {
namespace typelayout {

[[nodiscard]] consteval auto get_arch_prefix() noexcept {
    if constexpr (sizeof(void*) == 8)
        return CompileString{TYPELAYOUT_LITTLE_ENDIAN ? "[64-le]" : "[64-be]"};
    else if constexpr (sizeof(void*) == 4)
        return CompileString{TYPELAYOUT_LITTLE_ENDIAN ? "[32-le]" : "[32-be]"};
    else
        static_assert(sizeof(void*) == 4 || sizeof(void*) == 8, "Unsupported pointer size");
}

// Layer 1: Layout — pure byte identity (flattened, no names)

template <typename T>
[[nodiscard]] consteval auto get_layout_signature() noexcept {
    return get_arch_prefix() + TypeSignature<T, SignatureMode::Layout>::calculate();
}

template <typename T1, typename T2>
[[nodiscard]] consteval bool layout_signatures_match() noexcept {
    return get_layout_signature<T1>() == get_layout_signature<T2>();
}

// Layer 2: Definition — full type structure (tree, with names)

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