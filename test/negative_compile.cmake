# Negative compile test driver.
#
# Touches NC_SOURCE so the build system always recompiles it (a stale
# object from an earlier run can never satisfy the build), then builds
# NC_TARGET and echoes the compiler output for CTest's
# PASS_REGULAR_EXPRESSION to match.
#
# Usage: cmake -DBUILD_DIR=<dir> -DNC_TARGET=<name> -DNC_SOURCE=<path>
#              -P negative_compile.cmake
#
# Copyright (c) 2024-2026 TypeLayout Development Team
# Distributed under the Boost Software License, Version 1.0.

file(TOUCH "${NC_SOURCE}")

# Belt and braces: also drop stale objects, so a build that succeeded
# within the same second (timestamp granularity) can never be reused.
file(GLOB_RECURSE nc_stale_objs
    "${BUILD_DIR}/CMakeFiles/${NC_TARGET}.dir/*.o"
    "${BUILD_DIR}/CMakeFiles/${NC_TARGET}.dir/*.obj")
if(nc_stale_objs)
    file(REMOVE ${nc_stale_objs})
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${BUILD_DIR}" --target "${NC_TARGET}"
    OUTPUT_VARIABLE nc_out
    ERROR_VARIABLE nc_err
    RESULT_VARIABLE nc_res)

message("${nc_out}\n${nc_err}")

if(nc_res EQUAL 0)
    message("negative_compile: build of ${NC_TARGET} unexpectedly SUCCEEDED "
            "-- the compile-time gate did not fire")
endif()
