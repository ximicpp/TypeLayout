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

# Examples — Compatibility check
add_executable(compat_check example/compat_check.cpp)
target_link_libraries(compat_check PRIVATE typelayout)

enable_testing()

# Core engine regression tests (signature grammar + admission predicate)
add_executable(test_core test/test_core.cpp)
target_link_libraries(test_core PRIVATE typelayout)
add_test(NAME test_core COMMAND test_core)
set_tests_properties(test_core PROPERTIES
    LABELS "typelayout;core")

add_executable(test_relocatable_region
    test/test_relocatable_region.cpp)
target_link_libraries(test_relocatable_region PRIVATE typelayout)
target_include_directories(test_relocatable_region PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/example/relocatable_world_demo)
add_test(NAME test_relocatable_region COMMAND test_relocatable_region)
set_tests_properties(test_relocatable_region PROPERTIES
    LABELS "typelayout;example;relocatable-world")

add_library(relocatable_world_support STATIC
    example/relocatable_world_demo/world_runtime.cpp)
target_link_libraries(relocatable_world_support PUBLIC typelayout)
target_include_directories(relocatable_world_support PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/example/relocatable_world_demo)

add_executable(test_relocatable_world test/test_relocatable_world.cpp)
target_link_libraries(test_relocatable_world PRIVATE relocatable_world_support)
add_test(NAME test_relocatable_world COMMAND test_relocatable_world)
set_tests_properties(test_relocatable_world PROPERTIES
    LABELS "typelayout;example;relocatable-world")

set(TYPELAYOUT_XOFFSET_HEADER
    "${CMAKE_CURRENT_SOURCE_DIR}/vendor/XOffsetDatastructure/xoffsetdatastructure.hpp")

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    if(EXISTS "${TYPELAYOUT_XOFFSET_HEADER}")
        add_executable(xoffset_world_demo
            example/xoffset_world_demo/demo.cpp)
        target_link_libraries(xoffset_world_demo PRIVATE typelayout)
        target_include_directories(xoffset_world_demo PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/example/xoffset_world_demo
            ${CMAKE_CURRENT_SOURCE_DIR}/vendor/XOffsetDatastructure)
        add_test(NAME xoffset_world_demo COMMAND xoffset_world_demo)
        set_tests_properties(xoffset_world_demo PROPERTIES
            LABELS "typelayout;example;xoffset")

        add_executable(xoffset_world_export_ok EXCLUDE_FROM_ALL
            example/xoffset_world_demo/export_signatures.cpp)
        target_link_libraries(xoffset_world_export_ok PRIVATE typelayout)
        target_include_directories(xoffset_world_export_ok PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/example/xoffset_world_demo
            ${CMAKE_CURRENT_SOURCE_DIR}/vendor/XOffsetDatastructure)

        add_executable(xoffset_world_export_packed EXCLUDE_FROM_ALL
            example/xoffset_world_demo/export_signatures.cpp)
        target_link_libraries(xoffset_world_export_packed PRIVATE typelayout)
        target_include_directories(xoffset_world_export_packed PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/example/xoffset_world_demo
            ${CMAKE_CURRENT_SOURCE_DIR}/vendor/XOffsetDatastructure)
        target_compile_definitions(xoffset_world_export_packed PRIVATE
            TYPELAYOUT_XOFFSET_PACKED_ENTITY=1)
    else()
        message(STATUS
            "Skipping xoffset_world_demo: initialize vendor/XOffsetDatastructure")
    endif()
endif()

# Negative compile test: add_relocatable must reject a type whose opaque
# member hides a pointer. The target is excluded from the normal build;
# the test builds it on demand and passes only when the compiler emits
# the gate's static_assert message.
add_executable(test_gate_negative test/test_gate_negative.cpp)
target_link_libraries(test_gate_negative PRIVATE typelayout)
set_target_properties(test_gate_negative PROPERTIES
    EXCLUDE_FROM_ALL TRUE
    EXCLUDE_FROM_DEFAULT_BUILD TRUE)
add_test(NAME test_gate_negative
    COMMAND ${CMAKE_COMMAND}
        -DBUILD_DIR=${CMAKE_BINARY_DIR}
        -DNC_TARGET=test_gate_negative
        -DNC_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/test/test_gate_negative.cpp
        -P ${CMAKE_CURRENT_SOURCE_DIR}/test/negative_compile.cmake)
set_tests_properties(test_gate_negative PROPERTIES
    PASS_REGULAR_EXPRESSION "add_relocatable<T>: type must be byte-copy safe"
    RUN_SERIAL TRUE
    LABELS "typelayout;core;negative")

add_test(NAME compat_check_demo_negative COMMAND compat_check)
set_tests_properties(compat_check_demo_negative PROPERTIES
    WILL_FAIL TRUE
    LABELS "typelayout;compat;negative")

if(TYPELAYOUT_BUILD_COMPAT_CI)
    typelayout_add_sig_export(
        TARGET compat_ci_export
        SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/example/compat_ci_export.cpp
        OUTPUT_DIR ${TYPELAYOUT_COMPAT_CI_EXPORT_DIR}
    )

    typelayout_add_compat_check(
        TARGET compat_ci_check
        SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/example/compat_ci_check.cpp
        SIGS_DIR ${TYPELAYOUT_COMPAT_CI_SIGS_DIR}
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
    )
    add_test(NAME compat_ci_check_linux COMMAND compat_ci_check_linux)
endif()
