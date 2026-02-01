// Boost.TypeLayout
//
// Signature Generator Tool
//
// This tool generates type layout signatures for the current platform.
// It is designed to be compiled with user configuration included via -D macro.
//
// Usage:
//   clang++ -std=c++26 -freflection \
//     -DTYPELAYOUT_USER_CONFIG=\"path/to/config.hpp\" \
//     -I /path/to/typelayout/include \
//     siggen.cpp -o siggen
//   ./siggen > signatures.txt
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

// Include user configuration
#ifndef TYPELAYOUT_USER_CONFIG
    // Default to conventional name if not specified
    #if __has_include("typelayout.config.hpp")
        #include "typelayout.config.hpp"
    #else
        #error "No user configuration found. Define TYPELAYOUT_USER_CONFIG or create typelayout.config.hpp"
    #endif
#else
    #include TYPELAYOUT_USER_CONFIG
#endif

// Note: User must define types using TYPELAYOUT_TYPES() macro in their config file.
// The macro creates namespace typelayout_user_config with RegisteredTypes type alias.

// Generate main function
TYPELAYOUT_SIGGEN_MAIN()
