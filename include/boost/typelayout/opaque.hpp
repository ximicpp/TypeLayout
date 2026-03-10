// Opaque type registration macro.
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#ifndef BOOST_TYPELAYOUT_OPAQUE_HPP
#define BOOST_TYPELAYOUT_OPAQUE_HPP

#include <boost/typelayout/fixed_string.hpp>

// TYPELAYOUT_REGISTER_OPAQUE(Type, Tag, HasPointer)
//   Tag  -- globally unique string identifier
//   HasPointer -- true if the type contains pointers
//   Type must be trivially_copyable (enforced by static_assert).
//   Signature format: O(Tag|size|alignment)
#define TYPELAYOUT_REGISTER_OPAQUE(Type, Tag, HasPointer)                      \
    static_assert(std::is_trivially_copyable_v<Type>,                           \
        "TYPELAYOUT_REGISTER_OPAQUE: opaque type must be trivially copyable");  \
    template <>                                                                 \
    struct TypeSignature<Type> {                                                \
        static constexpr bool is_opaque = true;                                \
        static constexpr bool pointer_free = !(HasPointer);                    \
        static consteval auto calculate() noexcept {                           \
            return ::boost::typelayout::FixedString{"O("} +                    \
                   ::boost::typelayout::FixedString{Tag} +                     \
                   ::boost::typelayout::FixedString{"|"} +                     \
                   ::boost::typelayout::to_fixed_string<sizeof(Type)>() +      \
                   ::boost::typelayout::FixedString{"|"} +                     \
                   ::boost::typelayout::to_fixed_string<alignof(Type)>() +     \
                   ::boost::typelayout::FixedString{")"};                      \
        }                                                                      \
    };

#endif // BOOST_TYPELAYOUT_OPAQUE_HPP
