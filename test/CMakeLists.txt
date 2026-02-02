# Boost.TypeLayout - Test CMakeLists.txt
#
# Copyright (c) 2024-2026 TypeLayout Development Team
# Distributed under the Boost Software License, Version 1.0.

cmake_minimum_required(VERSION 3.14)

# Find Boost.Test
find_package(Boost REQUIRED COMPONENTS unit_test_framework)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Compiler flags for P2996 reflection
set(TYPELAYOUT_CXX_FLAGS "-std=c++26 -freflection")

# Helper function to add a test
function(add_typelayout_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} Boost::unit_test_framework)
    target_compile_options(${name} PRIVATE ${TYPELAYOUT_CXX_FLAGS})
    add_test(NAME ${name} COMMAND ${name})
endfunction()

# Compile-time only test (no Boost.Test dependency)
add_executable(test_all_types test_all_types.cpp)
target_compile_options(test_all_types PRIVATE ${TYPELAYOUT_CXX_FLAGS})

# Extended signature tests (no Boost.Test dependency)
add_executable(test_signature_extended test_signature_extended.cpp)
target_compile_options(test_signature_extended PRIVATE ${TYPELAYOUT_CXX_FLAGS})

# Comprehensive signature tests (no Boost.Test dependency)
add_executable(test_signature_comprehensive test_signature_comprehensive.cpp)
target_compile_options(test_signature_comprehensive PRIVATE ${TYPELAYOUT_CXX_FLAGS})

# Anonymous member tests (no Boost.Test dependency)
add_executable(test_anonymous_member test_anonymous_member.cpp)
target_compile_options(test_anonymous_member PRIVATE ${TYPELAYOUT_CXX_FLAGS})

# Serialization signature tests (Layer 2 - no Boost.Test dependency)
add_executable(test_serialization_signature test_serialization_signature.cpp)
target_compile_options(test_serialization_signature PRIVATE ${TYPELAYOUT_CXX_FLAGS})

# Edge case tests (no Boost.Test dependency)
add_executable(test_edge_cases test_edge_cases.cpp)
target_compile_options(test_edge_cases PRIVATE ${TYPELAYOUT_CXX_FLAGS})

# Signature compatibility tests (no Boost.Test dependency)
add_executable(test_signature_compatibility test_signature_compatibility.cpp)
target_compile_options(test_signature_compatibility PRIVATE ${TYPELAYOUT_CXX_FLAGS})

# Unit tests with Boost.Test
add_typelayout_test(test_primitives)
add_typelayout_test(test_structs)
add_typelayout_test(test_inheritance)
add_typelayout_test(test_bitfields)
add_typelayout_test(test_portability)
add_typelayout_test(test_hash)
add_typelayout_test(test_concepts)

# Enable CTest
enable_testing()
