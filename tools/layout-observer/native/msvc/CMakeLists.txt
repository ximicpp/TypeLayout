cmake_minimum_required(VERSION 3.20)
project(typelayout_msvc_observer VERSION 0.1 LANGUAGES CXX)
if(NOT MSVC OR NOT WIN32)
    message(FATAL_ERROR "The DIA backend requires Windows and an actual MSVC toolchain.")
endif()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(DIA_SDK_ROOT "${CMAKE_GENERATOR_INSTANCE}/DIA SDK" CACHE PATH "Visual Studio DIA SDK directory")
find_path(DIA_INCLUDE_DIR dia2.h HINTS "${DIA_SDK_ROOT}/include" REQUIRED)
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(DIA_DLL_SUBDIR amd64)
else()
    set(DIA_DLL_SUBDIR "")
endif()
find_file(DIA_RUNTIME msdia140.dll HINTS "${DIA_SDK_ROOT}/bin/${DIA_DLL_SUBDIR}" REQUIRED NO_DEFAULT_PATH)

add_executable(typelayout-msvc main.cpp)
target_include_directories(typelayout-msvc PRIVATE "${DIA_INCLUDE_DIR}")
target_compile_options(typelayout-msvc PRIVATE /W4 /Zi /utf-8)
target_compile_definitions(typelayout-msvc PRIVATE UNICODE _UNICODE)
target_link_libraries(typelayout-msvc PRIVATE ole32 oleaut32 version)
target_link_options(typelayout-msvc PRIVATE /DEBUG:FULL /INCREMENTAL:NO)
add_custom_command(TARGET typelayout-msvc POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${DIA_RUNTIME}" "$<TARGET_FILE_DIR:typelayout-msvc>/msdia140.dll")

add_executable(msvc-layout-fixture fixture.cpp)
target_compile_options(msvc-layout-fixture PRIVATE /W4 /Zi /utf-8)
target_compile_definitions(msvc-layout-fixture PRIVATE TYPELAYOUT_FIXTURE_CONFIGURATION="$<CONFIG>")
target_link_options(msvc-layout-fixture PRIVATE /DEBUG:FULL /OPT:NOREF /INCREMENTAL:NO)

include(CTest)
if(BUILD_TESTING)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    add_test(NAME msvc-layout-oracle COMMAND msvc-layout-fixture)
    add_test(NAME msvc-dia-contract
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_dia.py"
            "$<TARGET_FILE:typelayout-msvc>" "$<TARGET_FILE:msvc-layout-fixture>" "$<TARGET_PDB_FILE:msvc-layout-fixture>"
            "$<TARGET_PDB_FILE:typelayout-msvc>" "$<CONFIG>")
endif()
