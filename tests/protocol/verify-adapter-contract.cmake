# SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

foreach(required
        WIRE_HANDWRITTEN WIRE_GENERATED HIGH_HANDWRITTEN HIGH_GENERATED
        CONTRACT_DIFF REPORT_DIRECTORY)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

file(REMOVE_RECURSE "${REPORT_DIRECTORY}")
file(MAKE_DIRECTORY "${REPORT_DIRECTORY}")

function(run_scenario name executable)
    set(output_directory "${REPORT_DIRECTORY}/${name}")
    execute_process(
        COMMAND "${executable}" --report-dir "${output_directory}"
        RESULT_VARIABLE scenario_result
        OUTPUT_VARIABLE scenario_stdout
        ERROR_VARIABLE scenario_stderr
    )
    if(NOT scenario_result EQUAL 0)
        message(FATAL_ERROR
            "${name} failed (${scenario_result})\n${scenario_stdout}\n${scenario_stderr}")
    endif()
endfunction()

run_scenario(wire-handwritten "${WIRE_HANDWRITTEN}")
run_scenario(wire-generated "${WIRE_GENERATED}")
run_scenario(high-handwritten "${HIGH_HANDWRITTEN}")
run_scenario(high-generated "${HIGH_GENERATED}")

foreach(layer wire high)
    execute_process(
        COMMAND "${CONTRACT_DIFF}"
            "${REPORT_DIRECTORY}/${layer}-handwritten/summary.json"
            "${REPORT_DIRECTORY}/${layer}-generated/summary.json"
        RESULT_VARIABLE diff_result
        OUTPUT_VARIABLE diff_stdout
        ERROR_VARIABLE diff_stderr
    )
    if(NOT diff_result EQUAL 0)
        message(FATAL_ERROR
            "${layer} adapter contract differs\n${diff_stdout}\n${diff_stderr}")
    endif()
endforeach()

message(STATUS "Handwritten and generated wire/high adapter contracts match")
