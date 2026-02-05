// Boost.TypeLayout - Core Signature Generation API
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#ifndef BOOST_TYPELAYOUT_CORE_SIGNATURE_HPP
#define BOOST_TYPELAYOUT_CORE_SIGNATURE_HPP

#include <boost/typelayout/core/config.hpp>
#include <boost/typelayout/core/compile_string.hpp>
#include <boost/typelayout/core/type_signature.hpp>
#include <boost/typelayout/core/hash.hpp>

namespace boost {
namespace typelayout {

// Architecture prefix: "[64-le]", "[64-be]", "[32-le]", "[32-be]"
[[nodiscard]] consteval auto get_arch_prefix() noexcept {
    if constexpr (sizeof(void*) == 8) {
        return CompileString{TYPELAYOUT_LITTLE_ENDIAN ? "[64-le]" : "[64-be]"};
    } else if constexpr (sizeof(void*) == 4) {
        return CompileString{TYPELAYOUT_LITTLE_ENDIAN ? "[32-le]" : "[32-be]"};
    } else {
        // Exotic architectures
        auto bits = CompileString<32>::from_number(sizeof(void*) * 8);
        return CompileString{"["} + bits + CompileString{TYPELAYOUT_LITTLE_ENDIAN ? "-le]" : "-be]"};
    }
}

// Full layout signature: "[64-le]struct[s:8,a:4]{@0[x]:i32[s:4,a:4],...}"
template <typename T>
[[nodiscard]] consteval auto get_layout_signature() noexcept {
    return get_arch_prefix() + TypeSignature<T>::calculate();
}

// Signature equality check
template <typename T1, typename T2>
[[nodiscard]] consteval bool signatures_match() noexcept {
    return get_layout_signature<T1>() == get_layout_signature<T2>();
}

// Signature as C string (for runtime/logging)
template <typename T>
[[nodiscard]] constexpr const char* get_layout_signature_cstr() noexcept {
    static constexpr auto sig = get_layout_signature<T>();
    return sig.c_str();
}

// 64-bit FNV-1a hash of layout signature
template <typename T>
[[nodiscard]] consteval uint64_t get_layout_hash() noexcept {
    constexpr auto sig = get_layout_signature<T>();
    return fnv1a_hash(sig.c_str(), sig.length());
}

// Hash equality check
template <typename T1, typename T2>
[[nodiscard]] consteval bool hashes_match() noexcept {
    return get_layout_hash<T1>() == get_layout_hash<T2>();
}

} // namespace typelayout
} // namespace boost

// Static assertion: bind type to expected signature
#define TYPELAYOUT_BIND(Type, ExpectedSig) \
    static_assert(::boost::typelayout::get_layout_signature<Type>() == ExpectedSig, \
                  "Layout mismatch for " #Type)

#endif // BOOST_TYPELAYOUT_CORE_SIGNATURE_HPP