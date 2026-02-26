// Compile-time safety classifier for cross-platform type layout portability.
//
// classify_safety<T>() generates the layout signature of T via the existing
// signature engine, then scans it for risk markers (ptr, bits, wchar, f80,
// vptr, union, etc.).  This ensures that the safety classification is
// grounded in the SAME data that cross-platform comparison uses -- the
// actual encoded layout, not a parallel type-tree walk.
//
// The signature already encodes offsets, sizes, alignment, and type markers,
// so scanning it gives us both "type safety" AND "layout faithfulness" in
// one pass.
//
// Requires P2996 (Bloomberg Clang) because get_layout_signature<T>() needs
// reflection to generate the signature.
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#ifndef BOOST_TYPELAYOUT_TOOLS_CLASSIFY_SAFETY_HPP
#define BOOST_TYPELAYOUT_TOOLS_CLASSIFY_SAFETY_HPP

#include <boost/typelayout/signature.hpp>
#include <boost/typelayout/tools/compat_check.hpp>

namespace boost {
namespace typelayout {
namespace compat {

// =========================================================================
// classify_safety<T>  --  signature-based compile-time classifier
// =========================================================================
// Generates get_layout_signature<T>() and scans it for risk/warning markers.
//
// This is the consteval counterpart of classify_safety(std::string_view)
// in compat_check.hpp.  Same SafetyLevel enum, same marker detection logic,
// but the signature is produced and scanned entirely at compile time.
//
// Classification rules (matching the runtime version in compat_check.hpp):
//
//   Risk markers:
//     "bits<"    -- bit-fields (layout not portable across compilers)
//     "wchar["   -- wchar_t (2 bytes on Windows, 4 bytes on Linux)
//     "f80["     -- long double (80-bit on x86, 64-bit on ARM/MSVC)
//
//   Warning markers:
//     "ptr["     -- pointer
//     "fnptr["   -- function pointer
//     "memptr["  -- member pointer
//     "ref["     -- lvalue reference
//     "rref["    -- rvalue reference
//     ",vptr]"   -- polymorphic type (contains vtable pointer)
//     "union["   -- union (overlapping members)
//
//   Safe:
//     No risk or warning markers found in the signature.

/// Compile-time safety classification of type T, based on its layout signature.
///
/// @tparam T  The type to classify.
/// @return SafetyLevel::Safe, Warning, or Risk.
///
/// Example:
///   static_assert(classify_safety<int32_t>() == SafetyLevel::Safe);
///   static_assert(classify_safety<long double>() == SafetyLevel::Risk);
template<typename T>
[[nodiscard]] consteval SafetyLevel classify_safety() {
    constexpr auto sig = get_layout_signature<T>();

    // --- Risk markers (highest severity, check first) ---
    if constexpr (sig.contains(FixedString{"bits<"}))  return SafetyLevel::Risk;
    if constexpr (sig.contains(FixedString{"wchar["})) return SafetyLevel::Risk;
    if constexpr (sig.contains(FixedString{"f80["}))   return SafetyLevel::Risk;

    // --- Warning markers ---
    if constexpr (sig.contains(FixedString{"ptr["}) ||
                  sig.contains(FixedString{"fnptr["}) ||
                  sig.contains(FixedString{"memptr["}) ||
                  sig.contains(FixedString{"ref["}) ||
                  sig.contains(FixedString{"rref["}))  return SafetyLevel::Warning;
    if constexpr (sig.contains(FixedString{",vptr]"})) return SafetyLevel::Warning;
    if constexpr (sig.contains(FixedString{"union["})) return SafetyLevel::Warning;

    // --- No markers found ---
    return SafetyLevel::Safe;
}

/// Convenience: is the type's layout safe for zero-copy cross-platform transfer?
template<typename T>
[[nodiscard]] consteval bool is_layout_safe() {
    return classify_safety<T>() == SafetyLevel::Safe;
}

} // namespace compat
} // namespace typelayout
} // namespace boost

#endif // BOOST_TYPELAYOUT_TOOLS_CLASSIFY_SAFETY_HPP