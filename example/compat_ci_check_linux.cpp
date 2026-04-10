// Linux-only compatibility check for live-generated CI artifacts.
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#include "x86_64_linux_gcc.sig.hpp"
#include "x86_64_linux_clang.sig.hpp"

#include "compat_auto.hpp"

TYPELAYOUT_ASSERT_COMPAT(x86_64_linux_gcc, x86_64_linux_clang)
TYPELAYOUT_CHECK_COMPAT(x86_64_linux_gcc, x86_64_linux_clang)
