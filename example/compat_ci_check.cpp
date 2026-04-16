// Green-path compatibility check for the reference pipeline.
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#include "x86_64_linux_clang.sig.hpp"
#include "arm64_macos_clang.sig.hpp"

#include <boost/typelayout/tools/compat_auto.hpp>

TYPELAYOUT_ASSERT_COMPAT(x86_64_linux_clang, arm64_macos_clang)
TYPELAYOUT_CHECK_COMPAT(x86_64_linux_clang, arm64_macos_clang)
