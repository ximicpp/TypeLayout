cmake_minimum_required(VERSION 3.16)

project(boost_typelayout
    VERSION 2.0.0
    DESCRIPTION "Boost.TypeLayout - C++26 Compile-Time Type Layout Signatures"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(BOOST_TYPELAYOUT_CONSTEXPR_STEPS "5000000" CACHE STRING
    "Constexpr evaluation step limit")
option(TYPELAYOUT_BUILD_COMPAT_CI
    "Build the green-path compatibility reference targets"
    ON)
option(TYPELAYOUT_BUILD_COMPAT_CI_LINUX
    "Build the Linux artifact aggregation checker used by the root CI pipeline"
    OFF)
set(TYPELAYOUT_COMPAT_CI_SIGS_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/example/sigs-green"
    CACHE PATH
    "Directory containing .sig.hpp files for the green compatibility pipeline")
set(TYPELAYOUT_COMPAT_CI_EXPORT_DIR
    "${CMAKE_BINARY_DIR}/compat-sigs"
    CACHE PATH
    "Directory where compat_ci_export writes generated .sig.hpp files")
set(TYPELAYOUT_COMPAT_CI_LINUX_SIGS_DIR
    ""
    CACHE PATH
    "Directory containing Linux-generated .sig.hpp files for the aggregate CI checker")

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(-freflection -freflection-latest -Wall -Wextra)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND
       CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "16")
    add_compile_options(-freflection -Wall -Wextra)
endif()

# Header-only library
add_library(typelayout INTERFACE)
target_include_directories(typelayout INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND BOOST_TYPELAYOUT_CONSTEXPR_STEPS GREATER 0)
    target_compile_options(typelayout INTERFACE
        -fconstexpr-steps=${BOOST_TYPELAYOUT_CONSTEXPR_STEPS}
    )
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND BOOST_TYPELAYOUT_CONSTEXPR_STEPS GREATER 0)
    target_compile_options(typelayout INTERFACE
        -fconstexpr-ops-limit=${BOOST_TYPELAYOUT_CONSTEXPR_STEPS}
    )
endif()

# Cross-platform compatibility CMake module
include(cmake/TypeLayoutCompat.cmake)

# Examples — Phase 1: Signature export (requires P2996)
add_executable(sig_export example/cross_platform_check.cpp)
target_link_libraries(sig_export PRIVATE typelayout)

# Examples — Phase 2: Compatibility check
add_executable(compat_check example/compat_check.cpp)
target_link_libraries(compat_check PRIVATE typelayout)
target_include_directories(compat_check PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/example)

enable_testing()
add_test(NAME compat_check_demo_negative COMMAND compat_check)
set_tests_properties(compat_check_demo_negative PROPERTIES
    WILL_FAIL TRUE
    LABELS "typelayout;compat;negative")

if(TYPELAYOUT_BUILD_COMPAT_CI)
    typelayout_add_sig_export(
        TARGET compat_ci_export
        SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/example/compat_ci_export.cpp
        OUTPUT_DIR ${TYPELAYOUT_COMPAT_CI_EXPORT_DIR}
        INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/example
    )

    typelayout_add_compat_check(
        TARGET compat_ci_check
        SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/example/compat_ci_check.cpp
        SIGS_DIR ${TYPELAYOUT_COMPAT_CI_SIGS_DIR}
        INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/example
    )
    add_test(NAME compat_ci_check COMMAND compat_ci_check)
endif()

if(TYPELAYOUT_BUILD_COMPAT_CI_LINUX)
    if(NOT TYPELAYOUT_COMPAT_CI_LINUX_SIGS_DIR)
        message(FATAL_ERROR
            "TYPELAYOUT_BUILD_COMPAT_CI_LINUX requires TYPELAYOUT_COMPAT_CI_LINUX_SIGS_DIR")
    endif()

    typelayout_add_compat_check(
        TARGET compat_ci_check_linux
        SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/example/compat_ci_check_linux.cpp
        SIGS_DIR ${TYPELAYOUT_COMPAT_CI_LINUX_SIGS_DIR}
        INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/example
    )
    add_test(NAME compat_ci_check_linux COMMAND compat_ci_check_linux)
endif()
