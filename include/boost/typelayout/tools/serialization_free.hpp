// serialization_free.hpp -- Zero-copy transmission traits (signature-based).
//
// [Tool layer] This header is part of the Tools layer of TypeLayout.
// It consumes layout_traits (Core) to provide serialization-free
// determination -- a specific usage strategy.
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

#ifndef BOOST_TYPELAYOUT_TOOLS_SERIALIZATION_FREE_HPP
#define BOOST_TYPELAYOUT_TOOLS_SERIALIZATION_FREE_HPP

#include <boost/typelayout/layout_traits.hpp>
#include <boost/typelayout/signature.hpp>
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
    : std::bool_constant<
          std::is_trivially_copyable_v<T> &&
          !layout_traits<T>::has_pointer> {};

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
// is_transfer_safe<T>(remote_sig)
//
// Runtime function that checks all three serialization-free conditions
// in a single call:
//   (1) trivially_copyable(T)           -- memcpy does not break object model
//   (2) !has_pointer(T)                 -- no address-space dependencies
//   (3) signature_match(local, remote)  -- both endpoints have identical layout
//
// Conditions (1) and (2) are checked at compile time via is_local_serialization_free_v<T>.
// Condition (3) is checked at runtime because the remote signature is only
// known at connection/handshake time.
//
// Returns true only if all three conditions hold.  This is a sufficient
// condition for safe raw-memcpy transfer between the local endpoint and
// the remote endpoint that provided `remote_sig`.
//
// Parameters:
//   remote_sig -- the layout signature string received from the remote endpoint
//                 (e.g. via get_layout_signature<T>().c_str() exported by peer)
//
// Example:
//   // Handshake: receive remote signature string
//   std::string remote_sig = receive_handshake_sig();
//
//   if (!is_transfer_safe<PacketHeader>(remote_sig)) {
//       // Layout mismatch or type is not serialization-free locally.
//       // Fall back to explicit serialization.
//   } else {
//       // Safe to memcpy directly.
//       memcpy(buf, &pkt, sizeof(PacketHeader));
//   }
// =========================================================================

template <typename T>
[[nodiscard]] inline bool is_transfer_safe(std::string_view remote_sig) noexcept {
    // Conditions (1) + (2): compile-time local check.
    // If T fails these, it can never be transfer-safe regardless of the remote.
    if constexpr (!is_local_serialization_free_v<T>) {
        return false;
    } else {
        // Condition (3): runtime signature comparison.
        // get_layout_signature<T>() is consteval — evaluated at compile time.
        // FixedString has an implicit conversion to std::string_view.
        constexpr auto local_sig = get_layout_signature<T>();
        return std::string_view(local_sig) == remote_sig;
    }
}

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
    // Register a local type's signature with an explicit key.
    //
    // `key` is a user-chosen, binary-stable string that identifies the
    // type.  Both endpoints must agree on the same key for the same
    // logical type.  Typical choices:
    //   - A qualified C++ name:   "mylib::PacketHeader"
    //   - A protocol tag:         "msg.PacketHeader/v1"
    //   - A hash or UUID:         "a3f8..."
    //
    // Using an explicit key avoids reliance on typeid(T).name(),
    // whose output is compiler-specific (e.g. GCC mangles, MSVC does
    // not) and therefore not binary-stable across toolchains.
    template <typename T>
    void register_local(std::string_view key) {
        static_assert(is_local_serialization_free_v<T>,
            "Only locally serialization-free types can be registered.");

        // layout_traits<T>::signature is a static constexpr data member with
        // static storage duration -- string_view is permanently valid.
        // Do not change signature to a local or non-static variable without
        // switching local_signatures_ values to std::string.
        local_signatures_[std::string(key)] =
            std::string_view(layout_traits<T>::signature);
    }

    // Register a local type using typeid(T).name() as a convenience key.
    //
    // This is the legacy overload.  It is suitable for single-compiler
    // systems but NOT binary-stable across different compilers.
    // Prefer the explicit-key overload for cross-compiler scenarios.
    template <typename T>
    void register_local() {
        static_assert(is_local_serialization_free_v<T>,
            "Only locally serialization-free types can be registered.");

        auto key = default_type_key<T>();
        // layout_traits<T>::signature is static constexpr -- string_view is safe.
        local_signatures_[key] = std::string_view(layout_traits<T>::signature);
    }

    // Record a remote endpoint's signature for a named type.
    void register_remote(std::string_view type_name,
                         std::string_view remote_sig) {
        remote_signatures_[std::string(type_name)] = std::string(remote_sig);
    }

    // Determine if a type is serialization-free by explicit key.
    [[nodiscard]] bool is_serialization_free(std::string_view key) const {
        auto local_it = local_signatures_.find(std::string(key));
        auto remote_it = remote_signatures_.find(std::string(key));

        if (local_it == local_signatures_.end()) return false;
        if (remote_it == remote_signatures_.end()) return false;

        return local_it->second == remote_it->second;
    }

    // Determine if T is serialization-free using the default typeid key.
    // Legacy convenience overload -- see register_local() note above.
    template <typename T>
    [[nodiscard]] bool is_serialization_free() const {
        return is_serialization_free(default_type_key<T>());
    }

    // Get diagnostic information by explicit key.
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

    // Get diagnostic information using the default typeid key.
    template <typename T>
    [[nodiscard]] std::string diagnose() const {
        return diagnose(default_type_key<T>());
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

    // Default key derived from typeid.  Not binary-stable across compilers,
    // but convenient for single-toolchain deployments.
    template <typename T>
    static std::string default_type_key() {
        return std::string(typeid(T).name());
    }
};

} // namespace typelayout
} // namespace boost

#endif // BOOST_TYPELAYOUT_TOOLS_SERIALIZATION_FREE_HPP