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

# Tests
include(CTest)

function(typelayout_add_test name label)
    add_executable(${name} test/${name}.cpp)
    target_link_libraries(${name} PRIVATE typelayout)
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES TIMEOUT 120 LABELS "${label}")
endfunction()

typelayout_add_test(test_opaque core)
typelayout_add_test(test_fixed_string core)
typelayout_add_test(test_layout_traits core)
typelayout_add_test(test_classify tools)
typelayout_add_test(test_sig_export tools)

# Cross-platform compatibility CMake module
include(cmake/TypeLayoutCompat.cmake)

# Examples — Phase 1: Signature export (requires P2996)
add_executable(sig_export example/cross_platform_check.cpp)
target_link_libraries(sig_export PRIVATE typelayout)

# Examples — Phase 2: Compatibility check
add_executable(compat_check example/compat_check.cpp)
target_link_libraries(compat_check PRIVATE typelayout)
target_include_directories(compat_check PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/example)

typelayout_add_test(test_empty_member_probe core)
typelayout_add_test(test_transfer tools)
typelayout_add_test(test_byte_copy_safe core)
typelayout_add_test(test_padding_precision core)
typelayout_add_test(test_rt_padding tools)
typelayout_add_test(test_compat_check tools)
typelayout_add_test(test_advanced_types core)