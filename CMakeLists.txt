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

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(-freflection -freflection-latest -Wall -Wextra)
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
endif()

# Tests
include(CTest)
add_executable(test_two_layer test/test_two_layer.cpp)
target_link_libraries(test_two_layer PRIVATE typelayout)
add_test(NAME test_two_layer COMMAND test_two_layer)
set_tests_properties(test_two_layer PROPERTIES TIMEOUT 120 LABELS "core")

add_executable(test_opaque test/test_opaque.cpp)
target_link_libraries(test_opaque PRIVATE typelayout)
add_test(NAME test_opaque COMMAND test_opaque)
set_tests_properties(test_opaque PROPERTIES TIMEOUT 120 LABELS "core")

add_executable(test_fixed_string test/test_fixed_string.cpp)
target_link_libraries(test_fixed_string PRIVATE typelayout)
add_test(NAME test_fixed_string COMMAND test_fixed_string)
set_tests_properties(test_fixed_string PROPERTIES TIMEOUT 60 LABELS "core")

add_executable(test_sig_export test/test_sig_export.cpp)
target_link_libraries(test_sig_export PRIVATE typelayout)
add_test(NAME test_sig_export COMMAND test_sig_export)
set_tests_properties(test_sig_export PROPERTIES TIMEOUT 120 LABELS "tools")

# Cross-platform compatibility CMake module
include(cmake/TypeLayoutCompat.cmake)

# Examples — Phase 1: Signature export (requires P2996)
add_executable(sig_export example/cross_platform_check.cpp)
target_link_libraries(sig_export PRIVATE typelayout)

# Examples — Phase 2: Compatibility check (C++17 only, no P2996 needed)
add_executable(compat_check example/compat_check.cpp)
target_include_directories(compat_check PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/example
)
target_compile_options(compat_check PRIVATE -std=c++17 -stdlib=libc++)
target_link_options(compat_check PRIVATE -stdlib=libc++)

# Test — compatibility check utilities (C++17 only)
add_executable(test_compat_check test/test_compat_check.cpp)
target_include_directories(test_compat_check PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_options(test_compat_check PRIVATE -std=c++17 -stdlib=libc++)
target_link_options(test_compat_check PRIVATE -stdlib=libc++)
add_test(NAME test_compat_check COMMAND test_compat_check)
set_tests_properties(test_compat_check PROPERTIES TIMEOUT 30 LABELS "tools")
