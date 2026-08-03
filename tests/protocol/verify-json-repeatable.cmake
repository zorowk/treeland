if(NOT DEFINED JSON_EXECUTABLE
   OR NOT DEFINED INPUT
   OR NOT DEFINED EXPECTED
   OR NOT DEFINED REPORT_DIRECTORY)
    message(FATAL_ERROR "JSON repeatability test arguments are incomplete")
endif()

file(REMOVE_RECURSE "${REPORT_DIRECTORY}")
foreach(run RANGE 1 2)
    execute_process(
        COMMAND "${JSON_EXECUTABLE}"
            --input "${INPUT}"
            --expected "${EXPECTED}"
            --dump-actual "${REPORT_DIRECTORY}/actual-${run}.json"
            --report-dir "${REPORT_DIRECTORY}/run-${run}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
    )
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "JSON repeatability run ${run} failed\n${output}\n${error}")
    endif()
endforeach()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E compare_files
        "${REPORT_DIRECTORY}/actual-1.json"
        "${REPORT_DIRECTORY}/actual-2.json"
    RESULT_VARIABLE compare_result
)
if(NOT compare_result EQUAL 0)
    message(FATAL_ERROR "Normalized actual JSON differs between identical runs")
endif()
