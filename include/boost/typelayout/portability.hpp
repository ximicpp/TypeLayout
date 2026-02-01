// Boost.TypeLayout
//
// Portability Utilities (Legacy compatibility header)
//
// DEPRECATED: This header has been replaced.
// - For bit-field detection: use <boost/typelayout/util/serialization_check.hpp>
// - For serialization checking: use <boost/typelayout/typelayout_util.hpp>
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_TYPELAYOUT_PORTABILITY_HPP
#define BOOST_TYPELAYOUT_PORTABILITY_HPP

#ifdef _MSC_VER
#pragma message("Warning: <boost/typelayout/portability.hpp> is deprecated. Use <boost/typelayout/typelayout_util.hpp> instead.")
#else
#warning "This header is deprecated. Use <boost/typelayout/typelayout_util.hpp> instead."
#endif

// Include the new utility modules
#include <boost/typelayout/util/serialization_check.hpp>
#include <boost/typelayout/util/concepts.hpp>

namespace boost {
namespace typelayout {

    // has_bitfields<T>() is now provided by util/serialization_check.hpp
    // is_serializable_v<T> is now provided by util/serialization_check.hpp
    // Serializable<T> concept is now provided by util/concepts.hpp

} // namespace typelayout
} // namespace boost

#endif // BOOST_TYPELAYOUT_PORTABILITY_HPP