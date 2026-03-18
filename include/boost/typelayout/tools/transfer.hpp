// transfer.hpp -- Transfer safety: is_transfer_safe<T>(sig).
//
// Cross-endpoint verification:
//   (a) is_byte_copy_safe_v<T>          -- safe for byte-level copy
//   (b) signature_match(local, remote)  -- identical layout on both ends
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#ifndef BOOST_TYPELAYOUT_TOOLS_TRANSFER_HPP
#define BOOST_TYPELAYOUT_TOOLS_TRANSFER_HPP

#include <boost/typelayout/admission.hpp>
#include <boost/typelayout/signature.hpp>

#include <string_view>

namespace boost {
namespace typelayout {
inline namespace v1 {

// is_transfer_safe<T>(remote_sig) -- byte-copy safe + signature match.
// Condition (a) compile-time, condition (b) runtime.
template <typename T>
[[nodiscard]] inline bool is_transfer_safe(std::string_view remote_sig) noexcept {
    if constexpr (!is_byte_copy_safe_v<T>) {
        return false;
    } else {
        constexpr auto local_sig = get_layout_signature<T>();
        return std::string_view(local_sig) == remote_sig;
    }
}

} // inline namespace v1
} // namespace typelayout
} // namespace boost

#endif // BOOST_TYPELAYOUT_TOOLS_TRANSFER_HPP