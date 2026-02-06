# Boost.TypeLayout
#
# Copyright (c) 2024-2026 TypeLayout Development Team
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

cmake_minimum_required(VERSION 3.16)

project(boost_typelayout
    VERSION 1.0.0
    DESCRIPTION "Boost.TypeLayout - C++26 Compile-Time Memory Layout Analysis"
    LANGUAGES CXX
)

# Require C++26 for static reflection support
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build options
option(BOOST_TYPELAYOUT_BUILD_TESTS "Build test suite" ON)
option(BOOST_TYPELAYOUT_BUILD_EXAMPLES "Build examples" ON)
option(BOOST_TYPELAYOUT_BUILD_TOOLS "Build tools" ON)

# Constexpr step limit configuration
# Default: 5000000 (supports 100+ struct members)
# Recommended values:
#   0       - Use compiler default (~1M, for small projects <50 members)
#   1500000 - Medium projects (50-80 struct members)
#   3000000 - Large projects (80-100 struct members)
#   5000000 - Very large projects (100+ struct members) [DEFAULT]
#  10000000 - Extreme cases
set(BOOST_TYPELAYOUT_CONSTEXPR_STEPS "5000000" CACHE STRING
    "Constexpr evaluation step limit for large structs (5M supports 100+ members)")

# Compiler-specific settings for Clang P2996
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    # Add reflection flags for Bloomberg Clang P2996 fork
    add_compile_options(
        -freflection
        -freflection-latest
        -Wall
        -Wextra
    )
    
    # Note: -stdlib=libc++ and linker flags are passed via command line
    # from build_and_run.sh to ensure correct library paths
    
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # GCC (when reflection support becomes available)
    add_compile_options(
        -Wall
        -Wextra
    )
    message(WARNING "GCC may not fully support C++26 reflection yet")
else()
    message(WARNING "Compiler ${CMAKE_CXX_COMPILER_ID} may not support C++26 reflection")
endif()

# Header-only library
add_library(typelayout INTERFACE)
add_library(Boost::typelayout ALIAS typelayout)

target_include_directories(typelayout INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Apply constexpr step limit to interface (propagates to all consumers)
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND BOOST_TYPELAYOUT_CONSTEXPR_STEPS GREATER 0)
    target_compile_options(typelayout INTERFACE
        -fconstexpr-steps=${BOOST_TYPELAYOUT_CONSTEXPR_STEPS}
    )
endif()

# Enable CTest for test discovery
include(CTest)

# Test executable (compile-time static_assert tests)
if(BOOST_TYPELAYOUT_BUILD_TESTS)
    add_executable(test_all_types test/test_all_types.cpp)
    target_link_libraries(test_all_types PRIVATE typelayout)
    
    # Extended signature tests
    add_executable(test_signature_extended test/test_signature_extended.cpp)
    target_link_libraries(test_signature_extended PRIVATE typelayout)
    
    # Comprehensive signature tests
    add_executable(test_signature_comprehensive test/test_signature_comprehensive.cpp)
    target_link_libraries(test_signature_comprehensive PRIVATE typelayout)
    
    # Anonymous member tests
    add_executable(test_anonymous_member test/test_anonymous_member.cpp)
    target_link_libraries(test_anonymous_member PRIVATE typelayout)
    
    # Alignment completeness tests
    add_executable(test_alignment test/test_alignment.cpp)
    target_link_libraries(test_alignment PRIVATE typelayout)
    
    # User-defined types tests
    add_executable(test_user_defined_types test/test_user_defined_types.cpp)
    target_link_libraries(test_user_defined_types PRIVATE typelayout)
    
    # Signature modes tests (validates name independence)
    add_executable(test_signature_modes test/test_signature_modes.cpp)
    target_link_libraries(test_signature_modes PRIVATE typelayout)
    
    # Stress tests (large struct support >40 members)
    add_executable(test_stress test/test_stress.cpp)
    target_link_libraries(test_stress PRIVATE typelayout)
    
    # Signature size analysis test (measures signature lengths)
    add_executable(test_signature_size test/test_signature_size.cpp)
    target_link_libraries(test_signature_size PRIVATE typelayout)
    
    # Register tests with CTest
    # For static_assert tests, successful compilation = test passed
    add_test(NAME test_all_types COMMAND test_all_types)
    add_test(NAME test_signature_extended COMMAND test_signature_extended)
    add_test(NAME test_signature_comprehensive COMMAND test_signature_comprehensive)
    add_test(NAME test_anonymous_member COMMAND test_anonymous_member)
    add_test(NAME test_alignment COMMAND test_alignment)
    add_test(NAME test_user_defined_types COMMAND test_user_defined_types)
    add_test(NAME test_signature_modes COMMAND test_signature_modes)
    add_test(NAME test_stress COMMAND test_stress)
    
    # Set test properties
    set_tests_properties(
        test_all_types
        test_signature_extended
        test_signature_comprehensive
        test_anonymous_member
        test_alignment
        test_user_defined_types
        test_signature_modes
        test_stress
        PROPERTIES
            TIMEOUT 120
            LABELS "core"
    )
    
    message(STATUS "  Tests: Enabled")
else()
    message(STATUS "  Tests: Disabled")
endif()

# Examples
if(BOOST_TYPELAYOUT_BUILD_EXAMPLES)
    # Basic demo
    add_executable(demo example/demo.cpp)
    target_link_libraries(demo PRIVATE typelayout)
    
    # Network protocol verification example
    add_executable(network_protocol_example example/network_protocol.cpp)
    target_link_libraries(network_protocol_example PRIVATE typelayout)
    
    # File format verification example
    add_executable(file_format_example example/file_format.cpp)
    target_link_libraries(file_format_example PRIVATE typelayout)
    
    # Shared Memory Demo
    add_executable(shared_memory_demo example/shared_memory_demo.cpp)
    target_link_libraries(shared_memory_demo PRIVATE typelayout)
    
    # Plugin Interface Verification Demo
    add_executable(plugin_interface_example example/plugin_interface.cpp)
    target_link_libraries(plugin_interface_example PRIVATE typelayout)
    
    message(STATUS "  Examples: Enabled (demo, network_protocol, file_format, shared_memory, plugin_interface)")
else()
    message(STATUS "  Examples: Disabled")
endif()

# Tools
if(BOOST_TYPELAYOUT_BUILD_TOOLS)
    add_executable(typelayout-tool tools/typelayout_tool.cpp)
    target_link_libraries(typelayout-tool PRIVATE typelayout)
    
    message(STATUS "  Tools: Enabled")
else()
    message(STATUS "  Tools: Disabled")
endif()

# Installation rules
include(GNUInstallDirs)

install(TARGETS typelayout
    EXPORT typelayoutTargets
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT typelayoutTargets
    FILE typelayoutTargets.cmake
    NAMESPACE Boost::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/typelayout
)

# Package configuration
include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/typelayoutConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/typelayoutConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/typelayout
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/typelayoutConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/typelayoutConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/typelayoutConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/typelayout
)

# Print configuration summary
message(STATUS "")
message(STATUS "=== Boost.TypeLayout Configuration ===")
message(STATUS "  Project: ${PROJECT_NAME} v${PROJECT_VERSION}")
message(STATUS "  Description: ${PROJECT_DESCRIPTION}")
message(STATUS "  C++ Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  C++ Compiler Path: ${CMAKE_CXX_COMPILER}")
message(STATUS "  C++ Standard: C++${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Install Prefix: ${CMAKE_INSTALL_PREFIX}")
if(BOOST_TYPELAYOUT_CONSTEXPR_STEPS GREATER 0)
    message(STATUS "  Constexpr Steps: ${BOOST_TYPELAYOUT_CONSTEXPR_STEPS}")
else()
    message(STATUS "  Constexpr Steps: compiler default (~1M)")
endif()
message(STATUS "")
