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

foreach(target
        test-protocol-window-management-wire
        test-protocol-window-management-wire-wrong-expected)
    add_executable(${target} ${protocol_wire_common_sources})
    target_link_libraries(${target}
        PRIVATE
            protocol-window-management-client-runtime
            module_windowmanagement
            Qt6::Test
    )
    target_compile_definitions(${target}
        PRIVATE
            TL_PROTOCOL_TEST_BUILD_TYPE="${CMAKE_BUILD_TYPE}"
            TL_PROTOCOL_TEST_GIT_COMMIT="${protocol_test_git_commit}"
            TL_PROTOCOL_TEST_DEFAULT_REPORT_DIR="${CMAKE_CURRENT_BINARY_DIR}/test-results/${target}"
    )
endforeach()

target_compile_definitions(test-protocol-window-management-wire-wrong-expected
    PRIVATE TL_PROTOCOL_TEST_WRONG_EXPECTED
)

add_test(
    NAME protocol-wire-window-management
    COMMAND test-protocol-window-management-wire
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
