if(NOT DEFINED SELFTEST_EXECUTABLE
   OR NOT DEFINED INPUT
   OR NOT DEFINED EXPECTED
   OR NOT DEFINED REPORT_DIRECTORY
   OR NOT DEFINED EXPECTED_CATEGORY)
    message(FATAL_ERROR "JSON runner self-test arguments are incomplete")
endif()

file(REMOVE_RECURSE "${REPORT_DIRECTORY}")
execute_process(
    COMMAND "${SELFTEST_EXECUTABLE}"
        --input "${INPUT}"
        --expected "${EXPECTED}"
        --report-dir "${REPORT_DIRECTORY}"
        --dump-actual "${REPORT_DIRECTORY}/actual.json"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)

if(result EQUAL 0)
    message(FATAL_ERROR "Self-test unexpectedly passed\n${output}\n${error}")
endif()

set(summary "${REPORT_DIRECTORY}/summary.json")
if(NOT EXISTS "${summary}")
    message(FATAL_ERROR "Self-test did not write summary.json\n${output}\n${error}")
endif()
file(READ "${summary}" contents)
string(JSON category ERROR_VARIABLE json_error GET "${contents}" failure_category)
if(json_error OR NOT category STREQUAL EXPECTED_CATEGORY)
    message(FATAL_ERROR
        "Expected failure category ${EXPECTED_CATEGORY}, got ${category}\n${contents}\n${error}")
endif()

if(DEFINED EXPECTED_CHECKPOINT)
    string(JSON checkpoint ERROR_VARIABLE checkpoint_error GET "${contents}" failure_checkpoint)
    if(checkpoint_error OR NOT checkpoint STREQUAL EXPECTED_CHECKPOINT)
        message(FATAL_ERROR
            "Expected failure checkpoint ${EXPECTED_CHECKPOINT}, got ${checkpoint}\n${contents}")
    endif()
    string(JSON expected_value ERROR_VARIABLE expected_error GET "${contents}" expected)
    string(JSON actual_value ERROR_VARIABLE actual_error GET "${contents}" actual)
    if(expected_error OR actual_error)
        message(FATAL_ERROR "Checkpoint diff lacks expected or actual\n${contents}")
    endif()
endif()
