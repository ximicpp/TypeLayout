foreach(_required BUILD_DIR CONSUMER_HEADER AGREEMENT_HEADER MATRIX_HEADER)
    if(NOT DEFINED ${_required} OR "${${_required}}" STREQUAL "")
        message(FATAL_ERROR "missing required argument ${_required}")
    endif()
endforeach()

foreach(_header
        "${CONSUMER_HEADER}"
        "${AGREEMENT_HEADER}"
        "${MATRIX_HEADER}")
    if(EXISTS "${_header}")
        file(REMOVE "${_header}")
    endif()
endforeach()

set(_build_command
    "${CMAKE_COMMAND}" --build "${BUILD_DIR}"
    --target
        relocatable_world_consumer
        relocatable_world_agreement_check
        relocatable_world_matrix_check
    --parallel 3)
if(DEFINED BUILD_CONFIG AND NOT "${BUILD_CONFIG}" STREQUAL "")
    list(APPEND _build_command --config "${BUILD_CONFIG}")
endif()

execute_process(
    COMMAND ${_build_command}
    RESULT_VARIABLE _build_result
    OUTPUT_VARIABLE _build_stdout
    ERROR_VARIABLE _build_stderr)
if(NOT _build_result EQUAL 0)
    message(FATAL_ERROR
        "generated-header rebuild failed (${_build_result})\n"
        "stdout:\n${_build_stdout}\n"
        "stderr:\n${_build_stderr}")
endif()

foreach(_header
        "${CONSUMER_HEADER}"
        "${AGREEMENT_HEADER}"
        "${MATRIX_HEADER}")
    if(NOT EXISTS "${_header}")
        message(FATAL_ERROR "generated header was not recreated: ${_header}")
    endif()
endforeach()
