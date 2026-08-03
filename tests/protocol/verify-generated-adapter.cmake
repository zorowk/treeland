# SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

if(NOT DEFINED SCANNER OR NOT DEFINED PROTOCOL_XML OR NOT DEFINED OUTPUT_DIRECTORY)
    message(FATAL_ERROR "SCANNER, PROTOCOL_XML and OUTPUT_DIRECTORY are required")
endif()

file(REMOVE_RECURSE "${OUTPUT_DIRECTORY}")
file(MAKE_DIRECTORY "${OUTPUT_DIRECTORY}/first" "${OUTPUT_DIRECTORY}/second")

set(modes test-client-header test-client-code test-client-metadata)
set(files adapter.h adapter.c metadata.json)
foreach(index RANGE 0 2)
    list(GET modes ${index} mode)
    list(GET files ${index} filename)
    foreach(pass first second)
        execute_process(
            COMMAND "${SCANNER}" "${mode}" "${PROTOCOL_XML}"
            OUTPUT_FILE "${OUTPUT_DIRECTORY}/${pass}/${filename}"
            RESULT_VARIABLE scanner_result
            ERROR_VARIABLE scanner_error
        )
        if(NOT scanner_result EQUAL 0)
            message(FATAL_ERROR "${mode} failed: ${scanner_error}")
        endif()
    endforeach()
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E compare_files
            "${OUTPUT_DIRECTORY}/first/${filename}"
            "${OUTPUT_DIRECTORY}/second/${filename}"
        RESULT_VARIABLE compare_result
    )
    if(NOT compare_result EQUAL 0)
        message(FATAL_ERROR "${mode} output is not reproducible")
    endif()
endforeach()

file(READ "${OUTPUT_DIRECTORY}/first/metadata.json" metadata)
string(JSON protocol GET "${metadata}" protocol)
string(JSON interface_name GET "${metadata}" interfaces 0 name)
string(JSON interface_version GET "${metadata}" interfaces 0 version)
string(JSON request_count LENGTH "${metadata}" interfaces 0 requests)
string(JSON event_count LENGTH "${metadata}" interfaces 0 events)
string(JSON destructor_name GET "${metadata}" interfaces 0 requests 1 name)
string(JSON destructor GET "${metadata}" interfaces 0 requests 1 destructor)
string(JSON request_allow_null GET "${metadata}" interfaces 0 requests 0 arguments 0 allow_null)

if(NOT protocol STREQUAL "treeland_window_management_v1"
        OR NOT interface_name STREQUAL "treeland_window_management_v1"
        OR NOT interface_version EQUAL 1
        OR NOT request_count EQUAL 2
        OR NOT event_count EQUAL 1
        OR NOT destructor_name STREQUAL "destroy"
        OR NOT destructor
        OR request_allow_null)
    message(FATAL_ERROR "Generated metadata does not match the window-management XML contract")
endif()

file(READ "${OUTPUT_DIRECTORY}/first/adapter.c" adapter_code)
string(FIND "${adapter_code}" "treeland_window_management_v1_set_desktop" request_wrapper)
string(FIND "${adapter_code}" "treeland_window_management_v1_add_listener" event_listener)
string(FIND "${adapter_code}" "wl_registry_bind" registry_binder)
string(FIND "${adapter_code}" "wl_proxy_marshal" reimplemented_wire_abi)
if(request_wrapper EQUAL -1 OR event_listener EQUAL -1 OR registry_binder EQUAL -1)
    message(FATAL_ERROR "Generated adapter is missing a required wrapper, listener, or binder")
endif()
if(NOT reimplemented_wire_abi EQUAL -1)
    message(FATAL_ERROR "Generated adapter must call the official C stub, not marshal wire ABI")
endif()

message(STATUS "Generated adapter output and metadata are stable and match the XML contract")
