if(NOT DEFINED WIRE_EXECUTABLE
   OR NOT DEFINED JSON_EXECUTABLE
   OR NOT DEFINED CONTRACT_DIFF
   OR NOT DEFINED INPUT
   OR NOT DEFINED EXPECTED
   OR NOT DEFINED REPORT_DIRECTORY)
    message(FATAL_ERROR "JSON contract test arguments are incomplete")
endif()

file(REMOVE_RECURSE "${REPORT_DIRECTORY}")
file(MAKE_DIRECTORY "${REPORT_DIRECTORY}")

execute_process(
    COMMAND "${WIRE_EXECUTABLE}" --report-dir "${REPORT_DIRECTORY}/wire"
    RESULT_VARIABLE wire_result
    OUTPUT_VARIABLE wire_output
    ERROR_VARIABLE wire_error
)
if(NOT wire_result EQUAL 0)
    message(FATAL_ERROR "Hardcoded golden failed\n${wire_output}\n${wire_error}")
endif()

execute_process(
    COMMAND "${JSON_EXECUTABLE}"
        --input "${INPUT}"
        --expected "${EXPECTED}"
        --dump-actual "${REPORT_DIRECTORY}/json.actual.json"
        --report-dir "${REPORT_DIRECTORY}/json"
    RESULT_VARIABLE json_result
    OUTPUT_VARIABLE json_output
    ERROR_VARIABLE json_error
)
if(NOT json_result EQUAL 0)
    message(FATAL_ERROR "JSON scenario failed\n${json_output}\n${json_error}")
endif()

execute_process(
    COMMAND "${CONTRACT_DIFF}"
        "${REPORT_DIRECTORY}/wire/summary.json"
        "${REPORT_DIRECTORY}/json.actual.json"
    RESULT_VARIABLE diff_result
    OUTPUT_VARIABLE diff_output
    ERROR_VARIABLE diff_error
)
if(NOT diff_result EQUAL 0)
    message(FATAL_ERROR "${diff_output}\n${diff_error}")
endif()
