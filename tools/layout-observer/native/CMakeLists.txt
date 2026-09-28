cmake_minimum_required(VERSION 3.20)
project(typelayout_native_observer VERSION 0.1 LANGUAGES CXX)

# Separate project: no .NET dependency and no change to the header library's targets.
if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    message(FATAL_ERROR "The initial observation backend requires the validated Bloomberg Clang P2996 toolchain; ordinary Clang/GCC/MSVC support is not implied.")
endif()
if(NOT CMAKE_BUILD_TYPE MATCHES "^(Debug|Release|RelWithDebInfo|MinSizeRel)$")
    message(FATAL_ERROR "Configure with -DCMAKE_BUILD_TYPE=Debug or Release (or RelWithDebInfo/MinSizeRel).")
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(OBSERVER_NATIVE_FLAGS -std=c++26 -freflection -freflection-latest -stdlib=libc++ -Wall -Wextra -Wpedantic)
include(CheckCXXSourceCompiles)
string(JOIN " " CMAKE_REQUIRED_FLAGS ${OBSERVER_NATIVE_FLAGS})
check_cxx_source_compiles([=[
    #include <meta>
    #include <cstddef>
    struct Probe { [[no_unique_address]] struct Empty {} empty; unsigned bits : 3; int value; };
    constexpr auto member = std::meta::nonstatic_data_members_of(^^Probe, std::meta::access_context::unchecked())[0];
    constexpr auto bits = std::meta::nonstatic_data_members_of(^^Probe, std::meta::access_context::unchecked())[1];
    constexpr auto value = std::meta::nonstatic_data_members_of(^^Probe, std::meta::access_context::unchecked())[2];
    static_assert(std::meta::has_attribute(member, ^^[[no_unique_address]]));
    static_assert(std::meta::bit_size_of(bits) == 3);
    static_assert(std::meta::offset_of(value).bytes == offsetof(Probe, value));
    int main() { return 0; }
]=] TYPELAYOUT_OBSERVER_REFLECTION_CAPABLE)
unset(CMAKE_REQUIRED_FLAGS)
if(NOT TYPELAYOUT_OBSERVER_REFLECTION_CAPABLE)
    message(FATAL_ERROR "Observation capability probe failed: P2996 field offsets, bit widths, and no_unique_address attribute reflection are required. See CMakeFiles/CMakeError.log; no substitute compiler was selected.")
endif()
execute_process(COMMAND git rev-parse HEAD WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    OUTPUT_VARIABLE OBSERVER_REVISION OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(NOT OBSERVER_REVISION)
    set(OBSERVER_REVISION "unknown")
endif()
execute_process(COMMAND git status --porcelain WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    OUTPUT_VARIABLE OBSERVER_STATUS OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
set(OBSERVER_DIRTY 0)
if(OBSERVER_STATUS)
    set(OBSERVER_DIRTY 1)
endif()
string(TOUPPER "${CMAKE_BUILD_TYPE}" OBSERVER_CONFIGURATION_UPPER)
string(JOIN " " OBSERVER_NATIVE_FLAGS_TEXT ${OBSERVER_NATIVE_FLAGS})
set(OBSERVER_COMPILE_FLAGS "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_${OBSERVER_CONFIGURATION_UPPER}} ${OBSERVER_NATIVE_FLAGS_TEXT}")
configure_file(build_metadata.hpp.in build_metadata.hpp @ONLY)

add_executable(typelayout-native main.cpp)
target_include_directories(typelayout-native PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_options(typelayout-native PRIVATE ${OBSERVER_NATIVE_FLAGS})
target_link_options(typelayout-native PRIVATE -stdlib=libc++)

set(TYPELAYOUT_OBSERVER_REGISTRATION_HEADER "" CACHE FILEPATH "Consumer header defining register_layouts(observer::Collector&)")
if(TYPELAYOUT_OBSERVER_REGISTRATION_HEADER)
    get_filename_component(OBSERVER_REGISTRATION_HEADER "${TYPELAYOUT_OBSERVER_REGISTRATION_HEADER}" ABSOLUTE)
    if(NOT EXISTS "${OBSERVER_REGISTRATION_HEADER}" OR IS_DIRECTORY "${OBSERVER_REGISTRATION_HEADER}")
        message(FATAL_ERROR "Consumer registration header does not exist: ${OBSERVER_REGISTRATION_HEADER}")
    endif()
    target_compile_definitions(typelayout-native PRIVATE TYPELAYOUT_OBSERVER_REGISTRATION_HEADER="${OBSERVER_REGISTRATION_HEADER}")
endif()

include(CTest)
if(BUILD_TESTING AND NOT TYPELAYOUT_OBSERVER_REGISTRATION_HEADER)
    add_executable(native-layout-oracle test_oracle.cpp)
    target_compile_options(native-layout-oracle PRIVATE ${OBSERVER_NATIVE_FLAGS})
    target_link_options(native-layout-oracle PRIVATE -stdlib=libc++)
    add_test(NAME native-layout-oracle COMMAND native-layout-oracle)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    add_test(NAME native-observer-contract
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_snapshot.py"
            "$<TARGET_FILE:typelayout-native>" "$<TARGET_FILE:native-layout-oracle>" "${CMAKE_BUILD_TYPE}")
endif()
