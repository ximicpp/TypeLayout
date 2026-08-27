foreach(_required
        PYTHON
        EVIDENCE_TOOL
        CONSUMER
        EVIDENCE_DIR
        RESULTS_DIR
        PROFILE
        NODE)
    if(NOT DEFINED ${_required} OR "${${_required}}" STREQUAL "")
        message(FATAL_ERROR "missing required argument ${_required}")
    endif()
endforeach()

file(MAKE_DIRECTORY "${RESULTS_DIR}")
set(_fallback "${RESULTS_DIR}/${NODE}.results.json")
execute_process(
    COMMAND "${PYTHON}" "${EVIDENCE_TOOL}"
        fallback-results
        --profile "${PROFILE}"
        --consumer "${NODE}"
        --reason "consumer provenance is intentionally absent"
        --output "${_fallback}"
    RESULT_VARIABLE _fallback_result
    ERROR_VARIABLE _fallback_stderr)
if(NOT _fallback_result EQUAL 0)
    message(FATAL_ERROR "cannot create fallback result: ${_fallback_stderr}")
endif()
file(SHA256 "${_fallback}" _before)

execute_process(
    COMMAND "${CONSUMER}"
        "${PROFILE}" "${NODE}" "${EVIDENCE_DIR}" "${_fallback}"
    RESULT_VARIABLE _consumer_result
    OUTPUT_VARIABLE _consumer_stdout
    ERROR_VARIABLE _consumer_stderr)
if(_consumer_result EQUAL 0)
    message(FATAL_ERROR
        "fixture consumer unexpectedly replaced fallback\n${_consumer_stdout}")
endif()

file(SHA256 "${_fallback}" _after)
if(NOT "${_before}" STREQUAL "${_after}")
    message(FATAL_ERROR "consumer failure changed the fallback result")
endif()

execute_process(
    COMMAND "${PYTHON}" "${EVIDENCE_TOOL}"
        validate-results "${_fallback}"
    RESULT_VARIABLE _validation_result
    ERROR_VARIABLE _validation_stderr)
if(NOT _validation_result EQUAL 0)
    message(FATAL_ERROR "preserved fallback is invalid: ${_validation_stderr}")
endif()
