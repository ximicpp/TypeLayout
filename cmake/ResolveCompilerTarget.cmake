# Resolve the compiler identity recorded by relocatable-world probes.

function(typelayout_probe_compiler_target output compiler requested fallback)
    if("${compiler}" STREQUAL "")
        message(FATAL_ERROR "compiler target probe requires a compiler command")
    endif()

    set(_command ${compiler})
    if(NOT "${requested}" STREQUAL "")
        list(APPEND _command "--target=${requested}")
    endif()
    list(APPEND _command -dumpmachine)
    execute_process(
        COMMAND ${_command}
        RESULT_VARIABLE _result
        OUTPUT_VARIABLE _observed
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_VARIABLE _error)

    if(NOT "${requested}" STREQUAL "")
        if(NOT _result EQUAL 0 OR "${_observed}" STREQUAL "")
            message(FATAL_ERROR
                "cannot verify locked compiler target ${requested}: ${_error}")
        endif()
        if(NOT "${_observed}" STREQUAL "${requested}")
            message(FATAL_ERROR
                "compiler dumpmachine ${_observed} does not match locked compiler target ${requested}")
        endif()
        set(_resolved "${requested}")
    elseif(_result EQUAL 0 AND NOT "${_observed}" STREQUAL "")
        set(_resolved "${_observed}")
    else()
        set(_resolved "${fallback}")
    endif()

    set(${output} "${_resolved}" PARENT_SCOPE)
endfunction()
