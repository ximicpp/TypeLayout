// serialization_free.hpp -- Zero-copy transmission traits (signature-based).
//
// Provides compile-time and runtime traits for determining whether a
// type can be safely transmitted via raw memcpy (serialization-free)
// without explicit encoding/decoding.
//
// Serialization Free = no serialize, no deserialize, just memcpy.
//
//   Sender:   memcpy(buf, &obj, sizeof(T));   send(buf, sizeof(T));
//   Receiver: recv(buf, sizeof(T));           memcpy(&obj, buf, sizeof(T));
//
// Three conditions must hold:
//   (1) trivially_copyable(T)           -- memcpy does not break object model
//   (2) !has_pointer(T)                 -- no address-space dependencies
//   (3) signature_match(local, remote)  -- both ends have identical layout
//
// Condition (3) uses exact signature string matching (no hashing):
//   - Zero collision risk (false positive = silent data corruption)
//   - Self-documenting (signature string IS the diagnostic info)
//   - Performance irrelevant (comparison only at handshake time)
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#ifndef BOOST_TYPELAYOUT_SERIALIZATION_FREE_HPP
#define BOOST_TYPELAYOUT_SERIALIZATION_FREE_HPP

#include <boost/typelayout/layout_traits.hpp>
#include <type_traits>
#include <string>
#include <string_view>
#include <map>
#include <typeinfo>

namespace boost {
namespace typelayout {

// =========================================================================
// is_local_serialization_free<T>
//
// Compile-time, single-endpoint judgement.
// Determines whether T satisfies the LOCAL preconditions for
// serialization-free transfer:
//   - T is trivially copyable (memcpy is safe for the object model)
//   - T does not contain pointer-like fields (address-space independent)
//
// This does NOT compare remote signatures -- that is a runtime operation.
// If this trait is false, the type can NEVER be serialization-free,
// regardless of the remote endpoint.
// =========================================================================

template <typename T>
struct is_local_serialization_free
    : std::bool_constant<layout_traits<T>::local_serialization_free> {};

template <typename T>
inline constexpr bool is_local_serialization_free_v =
    is_local_serialization_free<T>::value;

// =========================================================================
// serialization_free_assert<T>
//
// Compile-time assertion for use in homogeneous (same-architecture)
// systems where both endpoints are known to be compiled with the same
// toolchain and target.  Produces a clear static_assert failure if T
// does not meet the local preconditions.
// =========================================================================

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

// =========================================================================
// SignatureRegistry
//
// Runtime signature registry for handshake-based serialization-free
// determination.
//
// Usage:
//   1. register_local<T>() for each type the local endpoint can send/recv.
//   2. Exchange signatures with the remote endpoint.
//   3. register_remote() for each type the remote endpoint declared.
//   4. is_serialization_free<T>() to check if memcpy path is safe.
//   5. diagnose<T>() for human-readable diagnostics on mismatch.
//
// All compatibility decisions are based on exact signature string
// matching.  No hashing is involved.
// =========================================================================

class SignatureRegistry {
public:
    // Register a local type's signature.
    // Only locally serialization-free types can be registered.
    template <typename T>
    void register_local() {
        static_assert(is_local_serialization_free_v<T>,
            "Only locally serialization-free types can be registered.");

        auto key = type_key<T>();
        local_signatures_[key] = std::string_view(layout_traits<T>::signature);
    }

    // Record a remote endpoint's signature for a named type.
    void register_remote(std::string_view type_name,
                         std::string_view remote_sig) {
        remote_signatures_[std::string(type_name)] = std::string(remote_sig);
    }

    // Determine if T is serialization-free (local conditions met AND
    // remote signature matches exactly).
    template <typename T>
    [[nodiscard]] bool is_serialization_free() const {
        // Conditions (1) and (2) are enforced at compile time by
        // register_local's static_assert.
        // Condition (3): exact signature string comparison.
        auto key = type_key<T>();
        auto local_it = local_signatures_.find(key);
        auto remote_it = remote_signatures_.find(key);

        if (local_it == local_signatures_.end()) return false;
        if (remote_it == remote_signatures_.end()) return false;

        return local_it->second == remote_it->second;
    }

    // Get diagnostic information when signatures do not match.
    template <typename T>
    [[nodiscard]] std::string diagnose() const {
        auto key = type_key<T>();
        std::string result;
        result += "Type: " + key + "\n";

        auto local_it = local_signatures_.find(key);
        auto remote_it = remote_signatures_.find(key);

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

    // Access the local signatures map (for building SIG_OFFER messages).
    [[nodiscard]] const std::map<std::string, std::string_view>&
    local_signatures() const noexcept {
        return local_signatures_;
    }

    // Access the remote signatures map (for inspection/debugging).
    [[nodiscard]] const std::map<std::string, std::string>&
    remote_signatures() const noexcept {
        return remote_signatures_;
    }

private:
    std::map<std::string, std::string_view> local_signatures_;
    std::map<std::string, std::string> remote_signatures_;

    template <typename T>
    static std::string type_key() {
        return std::string(typeid(T).name());
    }
};

} // namespace typelayout
} // namespace boost

#endif // BOOST_TYPELAYOUT_SERIALIZATION_FREE_HPP