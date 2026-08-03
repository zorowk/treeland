find_package(Qt6 REQUIRED COMPONENTS Test)
find_package(TreelandProtocols REQUIRED)
pkg_search_module(WAYLAND_CLIENT REQUIRED IMPORTED_TARGET wayland-client)
pkg_get_variable(WAYLAND_SCANNER wayland-scanner wayland_scanner)

set(window_management_xml
    ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-window-management-v1.xml
)
set(generated_directory
    ${CMAKE_CURRENT_BINARY_DIR}/generated/treeland-window-management-v1
)
set(client_header
    ${generated_directory}/wayland-treeland-window-management-v1-client-protocol.h
)
set(client_code
    ${generated_directory}/wayland-treeland-window-management-v1-client-protocol.c
)
set(test_adapter_header
    ${generated_directory}/tl-test-treeland-window-management-v1.h
)
set(test_adapter_code
    ${generated_directory}/tl-test-treeland-window-management-v1.c
)
set(test_adapter_metadata
    ${generated_directory}/tl-test-treeland-window-management-v1-metadata.json
)

add_custom_command(
    OUTPUT
        ${client_header}
        ${client_code}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${generated_directory}
    COMMAND ${WAYLAND_SCANNER} client-header ${window_management_xml} ${client_header}
    COMMAND ${WAYLAND_SCANNER} private-code ${window_management_xml} ${client_code}
    DEPENDS ${window_management_xml}
    VERBATIM
)

add_custom_command(
    OUTPUT
        ${test_adapter_header}
        ${test_adapter_code}
        ${test_adapter_metadata}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${generated_directory}
    COMMAND qtwaylandscanner_treeland test-client-header ${window_management_xml}
        > ${test_adapter_header}
    COMMAND qtwaylandscanner_treeland test-client-code ${window_management_xml}
        > ${test_adapter_code}
    COMMAND qtwaylandscanner_treeland test-client-metadata ${window_management_xml}
        > ${test_adapter_metadata}
    DEPENDS
        ${window_management_xml}
        qtwaylandscanner_treeland
    VERBATIM
)

add_custom_target(generate-window-management-test-adapter
    DEPENDS
        ${test_adapter_header}
        ${test_adapter_code}
        ${test_adapter_metadata}
)

add_test(
    NAME protocol-window-management-generated-adapter-output
    COMMAND ${CMAKE_COMMAND}
        -DSCANNER=$<TARGET_FILE:qtwaylandscanner_treeland>
        -DPROTOCOL_XML=${window_management_xml}
        -DOUTPUT_DIRECTORY=${CMAKE_CURRENT_BINARY_DIR}/scanner-selftest
        -P ${CMAKE_CURRENT_SOURCE_DIR}/verify-generated-adapter.cmake
)
set_tests_properties(protocol-window-management-generated-adapter-output PROPERTIES
    LABELS "protocol-scanner;generated-adapter"
    TIMEOUT 10
)

add_library(protocol-window-management-client-runtime STATIC
    clientruntime.cpp
    clientruntime.h
    windowmanagementadapter.c
    windowmanagementadapter.h
    ${client_code}
    ${client_header}
)
target_include_directories(protocol-window-management-client-runtime
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${generated_directory}
)
target_link_libraries(protocol-window-management-client-runtime
    PUBLIC
        Qt6::Core
        PkgConfig::WAYLAND_CLIENT
)

function(add_generated_client_runtime target)
    add_library(${target} STATIC
        clientruntime.cpp
        clientruntime.h
        ${test_adapter_code}
        ${test_adapter_header}
        ${client_code}
        ${client_header}
    )
    target_compile_definitions(${target}
        PRIVATE
            TL_WINDOW_MANAGEMENT_ADAPTER_HEADER="tl-test-treeland-window-management-v1.h"
            ${ARGN}
    )
    target_include_directories(${target}
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${generated_directory}
    )
    target_link_libraries(${target}
        PUBLIC
            Qt6::Core
            PkgConfig::WAYLAND_CLIENT
    )
endfunction()

add_generated_client_runtime(protocol-window-management-generated-client-runtime)
add_generated_client_runtime(
    protocol-window-management-generated-missing-listener-client-runtime
    TL_TEST_ADAPTER_DISABLE_LISTENER
)

execute_process(
    COMMAND git rev-parse --short=12 HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE protocol_test_git_commit
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

set(protocol_wire_common_sources
    main.cpp
    wirescenario.cpp
    wirescenario.h
    ${CMAKE_BINARY_DIR}/src/modules/window-management/qwayland-server-treeland-window-management-v1.cpp
)

function(add_protocol_wire_test target runtime)
    add_executable(${target} ${protocol_wire_common_sources})
    target_link_libraries(${target}
        PRIVATE
            ${runtime}
            module_windowmanagement
            Qt6::Test
    )
    target_compile_definitions(${target}
        PRIVATE
            TL_PROTOCOL_TEST_BUILD_TYPE="${CMAKE_BUILD_TYPE}"
            TL_PROTOCOL_TEST_GIT_COMMIT="${protocol_test_git_commit}"
            TL_PROTOCOL_TEST_DEFAULT_REPORT_DIR="${CMAKE_CURRENT_BINARY_DIR}/test-results/${target}"
    )
endfunction()

add_protocol_wire_test(
    test-protocol-window-management-wire
    protocol-window-management-client-runtime
)
add_protocol_wire_test(
    test-protocol-window-management-wire-wrong-expected
    protocol-window-management-client-runtime
)
add_protocol_wire_test(
    test-protocol-window-management-wire-generated
    protocol-window-management-generated-client-runtime
)
add_protocol_wire_test(
    test-protocol-window-management-wire-generated-missing-listener
    protocol-window-management-generated-missing-listener-client-runtime
)

target_compile_definitions(test-protocol-window-management-wire-wrong-expected
    PRIVATE TL_PROTOCOL_TEST_WRONG_EXPECTED
)

add_test(
    NAME protocol-wire-window-management
    COMMAND test-protocol-window-management-wire
)

add_test(
    NAME protocol-wire-window-management-generated
    COMMAND test-protocol-window-management-wire-generated
)
set_tests_properties(protocol-wire-window-management-generated PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    LABELS "protocol-wire;generated-adapter"
    TIMEOUT 10
)

add_test(
    NAME protocol-wire-window-management-generated-missing-listener-selftest
    COMMAND ${CMAKE_COMMAND}
        -DSELFTEST_EXECUTABLE=$<TARGET_FILE:test-protocol-window-management-wire-generated-missing-listener>
        -DREPORT_DIRECTORY=${CMAKE_CURRENT_BINARY_DIR}/test-results/generated-missing-listener-selftest
        -P ${CMAKE_CURRENT_SOURCE_DIR}/verify-missing-listener-selftest.cmake
)
set_tests_properties(protocol-wire-window-management-generated-missing-listener-selftest PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    LABELS "protocol-wire;generated-adapter;runner-selftest"
    TIMEOUT 10
)
set_tests_properties(protocol-wire-window-management PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    LABELS "protocol-wire"
    TIMEOUT 10
)

add_test(
    NAME protocol-wire-window-management-wrong-expected-selftest
    COMMAND ${CMAKE_COMMAND}
        -DSELFTEST_EXECUTABLE=$<TARGET_FILE:test-protocol-window-management-wire-wrong-expected>
        -DREPORT_DIRECTORY=${CMAKE_CURRENT_BINARY_DIR}/test-results/wrong-expected-selftest
        -P ${CMAKE_CURRENT_SOURCE_DIR}/verify-selftest.cmake
)
set_tests_properties(protocol-wire-window-management-wrong-expected-selftest PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    LABELS "protocol-wire;runner-selftest"
    TIMEOUT 10
)

set(protocol_high_common_sources
    helpermain.cpp
    helperscenario.cpp
    helperscenario.h
)

function(add_protocol_high_test target runtime)
    add_executable(${target} ${protocol_high_common_sources})
    target_link_libraries(${target}
        PRIVATE
            ${runtime}
            libtreeland
            Qt6::Test
    )
    target_compile_definitions(${target}
        PRIVATE
            TL_PROTOCOL_TEST_DEFAULT_REPORT_DIR="${CMAKE_CURRENT_BINARY_DIR}/test-results/${target}"
    )
endfunction()

add_protocol_high_test(
    test-protocol-window-management-helper
    protocol-window-management-client-runtime
)
add_protocol_high_test(
    test-protocol-window-management-helper-wrong-expected
    protocol-window-management-client-runtime
)
add_protocol_high_test(
    test-protocol-window-management-helper-generated
    protocol-window-management-generated-client-runtime
)

target_compile_definitions(test-protocol-window-management-helper-wrong-expected
    PRIVATE TL_PROTOCOL_TEST_WRONG_HELPER_EXPECTED
)

add_test(
    NAME protocol-high-window-management-helper
    COMMAND test-protocol-window-management-helper
)
set_tests_properties(protocol-high-window-management-helper PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    LABELS "protocol-high"
    TIMEOUT 30
)

add_test(
    NAME protocol-high-window-management-helper-generated
    COMMAND test-protocol-window-management-helper-generated
)
set_tests_properties(protocol-high-window-management-helper-generated PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    LABELS "protocol-high;generated-adapter"
    TIMEOUT 30
)

add_test(
    NAME protocol-high-window-management-helper-wrong-expected-selftest
    COMMAND ${CMAKE_COMMAND}
        -DSELFTEST_EXECUTABLE=$<TARGET_FILE:test-protocol-window-management-helper-wrong-expected>
        -DREPORT_DIRECTORY=${CMAKE_CURRENT_BINARY_DIR}/test-results/helper-wrong-expected-selftest
        -P ${CMAKE_CURRENT_SOURCE_DIR}/verify-helper-selftest.cmake
)
set_tests_properties(protocol-high-window-management-helper-wrong-expected-selftest PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    LABELS "protocol-high;runner-selftest"
    TIMEOUT 30
)

add_executable(protocol-adapter-contract-diff
    protocoladaptercontractdiff.cpp
)
target_link_libraries(protocol-adapter-contract-diff PRIVATE Qt6::Core)

add_test(
    NAME protocol-window-management-adapter-contract
    COMMAND ${CMAKE_COMMAND}
        -DWIRE_HANDWRITTEN=$<TARGET_FILE:test-protocol-window-management-wire>
        -DWIRE_GENERATED=$<TARGET_FILE:test-protocol-window-management-wire-generated>
        -DHIGH_HANDWRITTEN=$<TARGET_FILE:test-protocol-window-management-helper>
        -DHIGH_GENERATED=$<TARGET_FILE:test-protocol-window-management-helper-generated>
        -DCONTRACT_DIFF=$<TARGET_FILE:protocol-adapter-contract-diff>
        -DREPORT_DIRECTORY=${CMAKE_CURRENT_BINARY_DIR}/test-results/adapter-contract
        -P ${CMAKE_CURRENT_SOURCE_DIR}/verify-adapter-contract.cmake
)
set_tests_properties(protocol-window-management-adapter-contract PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    LABELS "protocol-wire;protocol-high;generated-adapter;contract-diff"
    TIMEOUT 30
)
