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
set(TYPELAYOUT_TOOLCHAIN_REVISION "${CMAKE_CXX_COMPILER_VERSION}" CACHE STRING
    "Immutable compiler source revision recorded by evidence probes")
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
    example/relocatable_world_demo/world_runtime.cpp
    example/relocatable_world_demo/checkpoint.cpp)
target_link_libraries(relocatable_world_support PUBLIC typelayout)
target_include_directories(relocatable_world_support PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/example/relocatable_world_demo)

add_executable(relocatable_world_demo
    example/relocatable_world_demo/demo.cpp)
target_link_libraries(relocatable_world_demo PRIVATE
    relocatable_world_support)
add_test(NAME relocatable_world_demo COMMAND relocatable_world_demo)
set_tests_properties(relocatable_world_demo PROPERTIES
    LABELS "typelayout;example;relocatable-world"
    PASS_REGULAR_EXPRESSION [=[^Admission\[whole_region_relocation\]: PASS
Agreement\[producer_ok, 4 types\]: MATCH
Relocation: base changed, raw offsets unchanged
Graph: null [+] shared [+] cycle [+] pointer container PASS
Business: party_hp=420, tick=42->43, boss_hp=300->250
Reload: mutation persisted

Negative\[native pointer\]: Admission FAIL, load skipped
Negative\[packed Entity\]: Agreement DIFFER, load skipped
Negative\[corrupt region offset\]: graph REJECT before dereference
$]=])

execute_process(
    COMMAND ${CMAKE_CXX_COMPILER} -dumpmachine
    OUTPUT_VARIABLE TYPELAYOUT_COMPILER_TARGET
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
if(NOT TYPELAYOUT_COMPILER_TARGET)
    set(TYPELAYOUT_COMPILER_TARGET "${CMAKE_SYSTEM_PROCESSOR}-${CMAKE_SYSTEM_NAME}")
endif()

add_executable(relocatable_world_platform_probe
    example/relocatable_world_demo/platform_probe.cpp)
target_link_libraries(relocatable_world_platform_probe PRIVATE
    relocatable_world_support)
target_compile_definitions(relocatable_world_platform_probe PRIVATE
    TYPELAYOUT_TOOLCHAIN_REVISION="${TYPELAYOUT_TOOLCHAIN_REVISION}"
    TYPELAYOUT_COMPILER_TARGET="${TYPELAYOUT_COMPILER_TARGET}")
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(relocatable_world_platform_probe PRIVATE
        -O3 -fstrict-aliasing)
endif()

add_executable(relocatable_world_producer
    example/relocatable_world_demo/producer.cpp)
target_link_libraries(relocatable_world_producer PRIVATE
    relocatable_world_support)

set(TYPELAYOUT_PROBE_NODE "")
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64)$")
    set(TYPELAYOUT_PROBE_ARCH "x86_64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$")
    set(TYPELAYOUT_PROBE_ARCH "arm64")
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(TYPELAYOUT_PROBE_OS "linux")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(TYPELAYOUT_PROBE_OS "macos")
endif()
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(TYPELAYOUT_PROBE_COMPILER "gcc")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(TYPELAYOUT_PROBE_COMPILER "clang")
endif()
if(TYPELAYOUT_PROBE_ARCH AND TYPELAYOUT_PROBE_OS AND
   TYPELAYOUT_PROBE_COMPILER)
    set(TYPELAYOUT_PROBE_NODE
        "${TYPELAYOUT_PROBE_ARCH}_${TYPELAYOUT_PROBE_OS}_${TYPELAYOUT_PROBE_COMPILER}")
endif()
if(TYPELAYOUT_PROBE_NODE)
    find_package(Python3 QUIET COMPONENTS Interpreter)
    include(cmake/RelocatableWorldProbeDefaults.cmake)

    add_test(NAME relocatable_world_platform_probe
        COMMAND relocatable_world_platform_probe
            ${TYPELAYOUT_PROBE_NODE}
            ${CMAKE_CURRENT_BINARY_DIR}/relocatable-world-platform-probe.json
            --runner local-ctest
            --runner-image local-ctest
            --xcode-version ${TYPELAYOUT_PROBE_XCODE_VERSION}
            --xcode-build ${TYPELAYOUT_PROBE_XCODE_BUILD}
            --sdk-version ${TYPELAYOUT_PROBE_SDK_VERSION}
            --sdk-build ${TYPELAYOUT_PROBE_SDK_BUILD}
            --deployment-target ${TYPELAYOUT_PROBE_DEPLOYMENT_TARGET}
            --sdk-locked ${TYPELAYOUT_PROBE_SDK_LOCKED})
    set_tests_properties(relocatable_world_platform_probe PROPERTIES
        LABELS "typelayout;example;relocatable-world;evidence"
        PASS_REGULAR_EXPRESSION
            "TOOLCHAIN PROBE PASS node=${TYPELAYOUT_PROBE_NODE}")

    if(Python3_Interpreter_FOUND)
        add_test(NAME test_relocatable_world_producer_exporter_build
            COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}
                --target relocatable_world_export_ok
                --config $<CONFIG>)
        set_tests_properties(
            test_relocatable_world_producer_exporter_build PROPERTIES
            FIXTURES_SETUP relocatable_world_producer_exporter
            LABELS "typelayout;example;relocatable-world;evidence")

        add_test(NAME test_relocatable_world_producer
            COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tools/relocatable_world_evidence.py
                verify-producer-bundle
                --node ${TYPELAYOUT_PROBE_NODE}
                --directory ${CMAKE_CURRENT_BINARY_DIR}/relocatable-world-producer
                --producer $<TARGET_FILE:relocatable_world_producer>
                --exporter $<TARGET_FILE:relocatable_world_export_ok>)
        set_tests_properties(test_relocatable_world_producer PROPERTIES
            FIXTURES_REQUIRED relocatable_world_producer_exporter
            LABELS "typelayout;example;relocatable-world;evidence"
            PASS_REGULAR_EXPRESSION
                "PRODUCER BUNDLE PASS node=${TYPELAYOUT_PROBE_NODE}")
    endif()
endif()

add_executable(test_relocatable_world test/test_relocatable_world.cpp)
target_link_libraries(test_relocatable_world PRIVATE relocatable_world_support)
add_test(NAME test_relocatable_world COMMAND test_relocatable_world)
set_tests_properties(test_relocatable_world PROPERTIES
    LABELS "typelayout;example;relocatable-world")

add_executable(test_relocatable_checkpoint
    test/test_relocatable_checkpoint.cpp)
target_link_libraries(test_relocatable_checkpoint PRIVATE
    relocatable_world_support)
add_test(NAME test_relocatable_checkpoint COMMAND test_relocatable_checkpoint)
set_tests_properties(test_relocatable_checkpoint PROPERTIES
    LABELS "typelayout;example;relocatable-world")

add_executable(relocatable_world_export_ok EXCLUDE_FROM_ALL
    example/relocatable_world_demo/export_signatures.cpp)
target_link_libraries(relocatable_world_export_ok PRIVATE typelayout)
target_include_directories(relocatable_world_export_ok PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/example/relocatable_world_demo)

add_executable(relocatable_world_export_packed EXCLUDE_FROM_ALL
    example/relocatable_world_demo/export_signatures.cpp)
target_link_libraries(relocatable_world_export_packed PRIVATE typelayout)
target_include_directories(relocatable_world_export_packed PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/example/relocatable_world_demo)
target_compile_definitions(relocatable_world_export_packed PRIVATE
    TYPELAYOUT_RELOCATABLE_WORLD_PACKED_ENTITY=1)

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
