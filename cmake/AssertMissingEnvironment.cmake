if(NOT DEFINED PROGRAM)
    message(FATAL_ERROR "PROGRAM was not provided")
endif()

execute_process(
    COMMAND
        "${CMAKE_COMMAND}" -E env
        --unset=HOSTNAME
        --unset=SERVER_HOST
        "${PROGRAM}"
    RESULT_VARIABLE program_result
    OUTPUT_VARIABLE program_stdout
    ERROR_VARIABLE program_stderr
)

set(program_output "${program_stdout}${program_stderr}")

if(program_result EQUAL 0)
    message(FATAL_ERROR "legacy node unexpectedly accepted missing environment")
endif()

if(NOT program_output MATCHES
   "Environment variables HOSTNAME and SERVER_HOST must be set")
    message(FATAL_ERROR
        "legacy node returned an unexpected diagnostic: ${program_output}")
endif()

message(STATUS "legacy node rejected missing environment as expected")
