if(NOT DEFINED SELFTEST_EXECUTABLE OR NOT DEFINED REPORT_DIRECTORY)
    message(FATAL_ERROR "SELFTEST_EXECUTABLE and REPORT_DIRECTORY are required")
endif()

set(summary_file "${REPORT_DIRECTORY}/summary.json")
file(REMOVE "${summary_file}")

execute_process(
    COMMAND "${SELFTEST_EXECUTABLE}" --report-dir "${REPORT_DIRECTORY}"
    RESULT_VARIABLE selftest_result
    OUTPUT_VARIABLE selftest_stdout
    ERROR_VARIABLE selftest_stderr
)

if(selftest_result EQUAL 0)
    message(FATAL_ERROR "Wrong-expected self-test unexpectedly passed")
endif()

if(NOT EXISTS "${summary_file}")
    message(FATAL_ERROR "Wrong-expected self-test did not write summary.json")
endif()

file(READ "${summary_file}" summary)
string(JSON result GET "${summary}" result)
string(JSON failure_category GET "${summary}" failure_category)

if(NOT result STREQUAL "fail")
    message(FATAL_ERROR "Wrong-expected self-test summary result is not fail")
endif()
if(NOT failure_category STREQUAL "checkpoint_event_diff")
    message(FATAL_ERROR
        "Wrong-expected self-test category is '${failure_category}', expected checkpoint_event_diff")
endif()

message(STATUS "Wrong-expected self-test failed with checkpoint_event_diff as expected")
