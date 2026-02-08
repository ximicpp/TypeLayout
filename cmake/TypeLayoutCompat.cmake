# TypeLayoutCompat.cmake — CMake utilities for cross-platform compatibility checking
#
# Two-phase pipeline:
#   Phase 1: typelayout_add_sig_export()    — export signatures on each platform
#   Phase 2: typelayout_add_compat_check()  — compare signatures at compile time
#   Both:    typelayout_add_compat_pipeline() — one call creates Phase 1 + Phase 2
#
# All functions expect the user to have written their source files using
# the declarative macros:
#   Phase 1 source: uses TYPELAYOUT_EXPORT_TYPES(...)
#   Phase 2 source: uses TYPELAYOUT_CHECK_COMPAT(...) or TYPELAYOUT_ASSERT_COMPAT(...)
#
# Copyright (c) 2024-2026 TypeLayout Development Team
# Distributed under the Boost Software License, Version 1.0.

include_guard(GLOBAL)

# ---------------------------------------------------------------------------
# typelayout_add_sig_export
# ---------------------------------------------------------------------------
# Creates an executable that exports TypeLayout signatures to a .sig.hpp file.
# The user source must use the TYPELAYOUT_EXPORT_TYPES(...) macro which
# generates a complete main().
#
# Usage:
#   typelayout_add_sig_export(
#       TARGET sig_export_myproject
#       SOURCE export_sigs.cpp          # must use TYPELAYOUT_EXPORT_TYPES(...)
#       OUTPUT_DIR ${CMAKE_BINARY_DIR}/sigs
#       INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/include   # optional extra include dirs
#   )
#
# Arguments:
#   TARGET       - Name of the executable target
#   SOURCE       - User .cpp file with TYPELAYOUT_EXPORT_TYPES(...)
#   OUTPUT_DIR   - Where .sig.hpp will be written (default: ${CMAKE_BINARY_DIR}/sigs)
#   INCLUDE_DIRS - Additional include directories for user types (optional)
#
function(typelayout_add_sig_export)
    cmake_parse_arguments(ARG "" "TARGET;SOURCE;OUTPUT_DIR" "INCLUDE_DIRS" ${ARGN})

    if(NOT ARG_TARGET)
        message(FATAL_ERROR "typelayout_add_sig_export: TARGET is required")
    endif()
    if(NOT ARG_SOURCE)
        message(FATAL_ERROR "typelayout_add_sig_export: SOURCE is required")
    endif()
    if(NOT ARG_OUTPUT_DIR)
        set(ARG_OUTPUT_DIR "${CMAKE_BINARY_DIR}/sigs")
    endif()

    add_executable(${ARG_TARGET} ${ARG_SOURCE})

    # TypeLayout headers
    target_include_directories(${ARG_TARGET} PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )

    # User-specified extra include dirs
    if(ARG_INCLUDE_DIRS)
        target_include_directories(${ARG_TARGET} PRIVATE ${ARG_INCLUDE_DIRS})
    endif()

    # P2996 compiler flags (required for Phase 1)
    target_compile_options(${ARG_TARGET} PRIVATE
        -std=c++26 -freflection -freflection-latest -stdlib=libc++
    )
    target_link_options(${ARG_TARGET} PRIVATE -stdlib=libc++)

    # Create output directory
    file(MAKE_DIRECTORY ${ARG_OUTPUT_DIR})

    # Run the exporter after building to produce .sig.hpp
    add_custom_command(
        TARGET ${ARG_TARGET} POST_BUILD
        COMMAND ${ARG_TARGET} ${ARG_OUTPUT_DIR}
        COMMENT "[TypeLayout] Exporting signatures to ${ARG_OUTPUT_DIR}"
        VERBATIM
    )
endfunction()

# ---------------------------------------------------------------------------
# typelayout_add_compat_check
# ---------------------------------------------------------------------------
# Creates a target that compiles a Phase 2 compatibility check program.
# The user source must use TYPELAYOUT_CHECK_COMPAT(...) for a runtime report,
# or TYPELAYOUT_ASSERT_COMPAT(...) for compile-time verification.
#
# Usage:
#   typelayout_add_compat_check(
#       TARGET compat_check_myproject
#       SOURCE check_compat.cpp         # must use TYPELAYOUT_CHECK_COMPAT(...)
#       SIGS_DIR ${CMAKE_SOURCE_DIR}/sigs
#       INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/include   # optional extra include dirs
#   )
#
# Arguments:
#   TARGET       - Name of the executable target
#   SOURCE       - User .cpp file with TYPELAYOUT_CHECK_COMPAT or TYPELAYOUT_ASSERT_COMPAT
#   SIGS_DIR     - Directory containing .sig.hpp files (added to include path)
#   INCLUDE_DIRS - Additional include directories (optional)
#
# Note: Phase 2 does NOT require the P2996 compiler. C++17 is sufficient.
#
function(typelayout_add_compat_check)
    cmake_parse_arguments(ARG "" "TARGET;SOURCE;SIGS_DIR" "INCLUDE_DIRS" ${ARGN})

    if(NOT ARG_TARGET)
        message(FATAL_ERROR "typelayout_add_compat_check: TARGET is required")
    endif()
    if(NOT ARG_SOURCE)
        message(FATAL_ERROR "typelayout_add_compat_check: SOURCE is required")
    endif()

    add_executable(${ARG_TARGET} ${ARG_SOURCE})

    # TypeLayout headers
    target_include_directories(${ARG_TARGET} PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )

    # Signature files directory
    if(ARG_SIGS_DIR)
        target_include_directories(${ARG_TARGET} PRIVATE ${ARG_SIGS_DIR})
    endif()

    # User-specified extra include dirs
    if(ARG_INCLUDE_DIRS)
        target_include_directories(${ARG_TARGET} PRIVATE ${ARG_INCLUDE_DIRS})
    endif()

    # Phase 2 only needs C++17 — no P2996 required
    target_compile_features(${ARG_TARGET} PRIVATE cxx_std_17)
endfunction()

# ---------------------------------------------------------------------------
# typelayout_add_compat_pipeline
# ---------------------------------------------------------------------------
# High-level convenience function: one call creates both Phase 1 (export) and
# Phase 2 (check) targets, wires their dependencies, and optionally registers
# a CTest test.
#
# Usage:
#   typelayout_add_compat_pipeline(
#       NAME myproject_compat
#       EXPORT_SOURCE src/export_types.cpp     # TYPELAYOUT_EXPORT_TYPES(...)
#       CHECK_SOURCE  src/check_compat.cpp     # TYPELAYOUT_CHECK_COMPAT(...)
#       SIGS_DIR      ${CMAKE_SOURCE_DIR}/sigs # where .sig.hpp live / get written
#       INCLUDE_DIRS  ${CMAKE_SOURCE_DIR}/include  # optional
#       ADD_TEST                                # optional: register CTest
#   )
#
# This creates:
#   - ${NAME}_export   : Phase 1 executable (requires P2996 Clang)
#   - ${NAME}_check    : Phase 2 executable (C++17)
#   - ${NAME}_check depends on ${NAME}_export (build order)
#   - If ADD_TEST is specified: ctest -R ${NAME}_check
#
# Arguments:
#   NAME           - Base name for generated targets
#   EXPORT_SOURCE  - Phase 1 source file (TYPELAYOUT_EXPORT_TYPES macro)
#   CHECK_SOURCE   - Phase 2 source file (TYPELAYOUT_CHECK_COMPAT or ASSERT macro)
#   SIGS_DIR       - Shared directory for .sig.hpp files
#                    (default: ${CMAKE_BINARY_DIR}/sigs)
#   INCLUDE_DIRS   - Additional include directories (optional, multi-value)
#   ADD_TEST       - If present, register Phase 2 as a CTest test
#
function(typelayout_add_compat_pipeline)
    cmake_parse_arguments(ARG "ADD_TEST" "NAME;EXPORT_SOURCE;CHECK_SOURCE;SIGS_DIR" "INCLUDE_DIRS" ${ARGN})

    if(NOT ARG_NAME)
        message(FATAL_ERROR "typelayout_add_compat_pipeline: NAME is required")
    endif()
    if(NOT ARG_EXPORT_SOURCE)
        message(FATAL_ERROR "typelayout_add_compat_pipeline: EXPORT_SOURCE is required")
    endif()
    if(NOT ARG_CHECK_SOURCE)
        message(FATAL_ERROR "typelayout_add_compat_pipeline: CHECK_SOURCE is required")
    endif()
    if(NOT ARG_SIGS_DIR)
        set(ARG_SIGS_DIR "${CMAKE_BINARY_DIR}/sigs")
    endif()

    set(_export_target "${ARG_NAME}_export")
    set(_check_target  "${ARG_NAME}_check")

    # ---- Phase 1: Export signatures ----
    typelayout_add_sig_export(
        TARGET     ${_export_target}
        SOURCE     ${ARG_EXPORT_SOURCE}
        OUTPUT_DIR ${ARG_SIGS_DIR}
        INCLUDE_DIRS ${ARG_INCLUDE_DIRS}
    )

    # ---- Phase 2: Compatibility check ----
    typelayout_add_compat_check(
        TARGET     ${_check_target}
        SOURCE     ${ARG_CHECK_SOURCE}
        SIGS_DIR   ${ARG_SIGS_DIR}
        INCLUDE_DIRS ${ARG_INCLUDE_DIRS}
    )

    # Phase 2 depends on Phase 1 having run (to produce .sig.hpp files)
    add_dependencies(${_check_target} ${_export_target})

    # ---- Optional CTest registration ----
    if(ARG_ADD_TEST)
        if(NOT CMAKE_TESTING_ENABLED)
            enable_testing()
        endif()

        # Running the Phase 2 executable produces the compatibility report
        # A non-zero exit code (from TYPELAYOUT_CHECK_COMPAT) or
        # compilation failure (from TYPELAYOUT_ASSERT_COMPAT) signals incompatibility
        add_test(
            NAME ${_check_target}
            COMMAND ${_check_target}
        )
        set_tests_properties(${_check_target} PROPERTIES
            LABELS "typelayout;compat"
        )
    endif()

    message(STATUS "[TypeLayout] Compat pipeline '${ARG_NAME}' configured:")
    message(STATUS "  Phase 1 (export): ${_export_target}")
    message(STATUS "  Phase 2 (check):  ${_check_target}")
    message(STATUS "  Sigs directory:   ${ARG_SIGS_DIR}")
    if(ARG_ADD_TEST)
        message(STATUS "  CTest:            ${_check_target}")
    endif()
endfunction()