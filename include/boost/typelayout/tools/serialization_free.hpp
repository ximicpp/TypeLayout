// serialization_free.hpp -- Zero-copy transmission traits.
//
// is_local_serialization_free (strict C++ POD safety):
//   (1) trivially_copyable(T)           -- memcpy preserves object model
//   (2) !has_pointer(T)                 -- no address-space dependencies
//
// is_transfer_safe (cross-endpoint verification):
//   (a) is_byte_copy_safe_v<T>          -- safe for byte-level copy
//       (accepts trivially_copyable types AND registered relocatable
//        opaque types such as XVector, XString, etc.)
//   (b) signature_match(local, remote)  -- identical layout on both ends
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#ifndef BOOST_TYPELAYOUT_TOOLS_SERIALIZATION_FREE_HPP
#define BOOST_TYPELAYOUT_TOOLS_SERIALIZATION_FREE_HPP

#include <boost/typelayout/layout_traits.hpp>
#include <boost/typelayout/admission.hpp>
#include <boost/typelayout/signature.hpp>
#include <type_traits>
#include <string>
#include <string_view>
#include <map>
#include <typeinfo>

namespace boost {
namespace typelayout {
inline namespace v1 {

// is_local_serialization_free<T> -- conditions (1) + (2) at compile time.
template <typename T>
struct is_local_serialization_free
    : std::bool_constant<
          std::is_trivially_copyable_v<T> &&
          !layout_traits<T>::has_pointer> {};

template <typename T>
inline constexpr bool is_local_serialization_free_v =
    is_local_serialization_free<T>::value;

// serialization_free_assert<T> -- static_assert for conditions (1) + (2).
template <typename T>
struct serialization_free_assert {
    static_assert(std::is_trivially_copyable_v<T>,
        "Serialization-free requires trivially_copyable. "
        "Type has non-trivial copy/move/destructor.");

    static_assert(!layout_traits<T>::has_pointer,
        "Serialization-free requires no pointers. "
        "Type contains pointer or reference members "
        "that are address-space dependent.");

    static constexpr bool value = true;
};

// is_transfer_safe<T>(remote_sig) -- byte-copy safe + signature match.
// Condition (a) compile-time, condition (b) runtime.
// Accepts trivially_copyable types and registered relocatable opaque types.
template <typename T>
[[nodiscard]] inline bool is_transfer_safe(std::string_view remote_sig) noexcept {
    // Condition (a): compile-time local check.
    // If T is not byte-copy safe, it can never be transfer-safe.
    if constexpr (!is_byte_copy_safe_v<T>) {
        return false;
    } else {
        constexpr auto local_sig = get_layout_signature<T>();
        return std::string_view(local_sig) == remote_sig;
    }
}

// SignatureRegistry -- runtime registry for handshake-based checks.
// register_local<T>(), exchange, register_remote(), is_serialization_free().

class SignatureRegistry {
public:
    // Register a local type with an explicit binary-stable key.
    // Accepts trivially_copyable types and registered relocatable opaque types.
    template <typename T>
    void register_local(std::string_view key) {
        static_assert(is_byte_copy_safe_v<T>,
            "Only byte-copy safe types can be registered. "
            "Type must be either locally serialization-free (trivially_copyable + no pointer) "
            "or a registered relocatable opaque type with safe elements.");

        local_signatures_[std::string(key)] =
            std::string(std::string_view(layout_traits<T>::signature));
    }

    // Register using typeid(T).name() as key.
    //
    // WARNING: typeid(T).name() is NOT binary-stable across compilers
    // (Clang, GCC, and MSVC use different mangling schemes).  Do NOT use
    // this overload for cross-compiler or cross-platform scenarios.
    // Prefer register_local<T>(key) with an explicit, stable key string.
    //
    // Accepts trivially_copyable types and registered relocatable opaque types.
    template <typename T>
    void register_local() {
        static_assert(is_byte_copy_safe_v<T>,
            "Only byte-copy safe types can be registered. "
            "Type must be either locally serialization-free (trivially_copyable + no pointer) "
            "or a registered relocatable opaque type with safe elements.");

        auto key = default_type_key<T>();
        local_signatures_[key] =
            std::string(std::string_view(layout_traits<T>::signature));
    }

    void register_remote(std::string_view type_name,
                         std::string_view remote_sig) {
        remote_signatures_[std::string(type_name)] = std::string(remote_sig);
    }

    // is_serialization_free -- historical name. Checks whether a type is
    // safe for cross-endpoint zero-copy transfer (byte-copy safe + sig match).
    // Despite the name, this now also accepts registered relocatable opaque types.
    [[nodiscard]] bool is_serialization_free(std::string_view key) const {
        auto local_it = local_signatures_.find(std::string(key));
        auto remote_it = remote_signatures_.find(std::string(key));

        if (local_it == local_signatures_.end()) return false;
        if (remote_it == remote_signatures_.end()) return false;

        return local_it->second == remote_it->second;
    }

    template <typename T>
    [[nodiscard]] bool is_serialization_free() const {
        return is_serialization_free(default_type_key<T>());
    }

    [[nodiscard]] std::string diagnose(std::string_view key) const {
        std::string k(key);
        std::string result;
        result += "Type: " + k + "\n";

        auto local_it = local_signatures_.find(k);
        auto remote_it = remote_signatures_.find(k);

        if (local_it != local_signatures_.end())
            result += "  local:  " + std::string(local_it->second) + "\n";
        else
            result += "  local:  (not registered)\n";

        if (remote_it != remote_signatures_.end())
            result += "  remote: " + remote_it->second + "\n";
        else
            result += "  remote: (not registered)\n";

        return result;
    }

    template <typename T>
    [[nodiscard]] std::string diagnose() const {
        return diagnose(default_type_key<T>());
    }

    [[nodiscard]] const std::map<std::string, std::string>&
    local_signatures() const noexcept {
        return local_signatures_;
    }

    [[nodiscard]] const std::map<std::string, std::string>&
    remote_signatures() const noexcept {
        return remote_signatures_;
    }

private:
    std::map<std::string, std::string> local_signatures_;
    std::map<std::string, std::string> remote_signatures_;

    template <typename T>
    static std::string default_type_key() {
        return std::string(typeid(T).name());
    }
};

} // inline namespace v1
} // namespace typelayout
} // namespace boost

#endif // BOOST_TYPELAYOUT_TOOLS_SERIALIZATION_FREE_HPP