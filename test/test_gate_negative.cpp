// test_gate_negative.cpp -- Negative compile test.
//
// This file MUST NOT compile: SigExporter::add_relocatable must reject a
// type whose opaque member was registered with HasPointer = true (the
// pointer is sealed inside O(Tag|N|A), invisible to the token scan).
// The CTest entry builds this target on demand and passes only when the
// compiler emits the gate's static_assert message.
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#include <boost/typelayout/tools/sig_export.hpp>

#include <cstdint>

using namespace boost::typelayout;

// Sealed opaque that internally holds a native pointer (HasPointer = true).
struct SealedHandle { void* impl; };

namespace boost { namespace typelayout { inline namespace v1 {
TYPELAYOUT_REGISTER_OPAQUE(SealedHandle, "SealedHandle", true)
}}}

struct SealedCarrier { std::uint32_t id; SealedHandle h; };

int main() {
    SigExporter ex("negative");
    ex.add_relocatable<SealedCarrier>("SealedCarrier");  // must be rejected
    return 0;
}
