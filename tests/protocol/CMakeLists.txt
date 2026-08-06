find_package(Qt6 REQUIRED COMPONENTS Test)
find_package(TreelandProtocols REQUIRED)
pkg_search_module(WAYLAND_CLIENT REQUIRED IMPORTED_TARGET wayland-client)
pkg_search_module(WAYLAND_SERVER REQUIRED IMPORTED_TARGET wayland-server)
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

set(wine_window_management_xml
    ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-wine-window-management-unstable-v1.xml
)
set(xdg_shell_xml
    ${WAYLAND_PROTOCOLS_DATADIR}/stable/xdg-shell/xdg-shell.xml
)
if(NOT WAYLAND_PROTOCOLS_DATADIR)
    pkg_get_variable(WAYLAND_PROTOCOLS_DATADIR wayland-protocols pkgdatadir)
    set(xdg_shell_xml ${WAYLAND_PROTOCOLS_DATADIR}/stable/xdg-shell/xdg-shell.xml)
endif()
set(wine_generated_directory
    ${CMAKE_CURRENT_BINARY_DIR}/generated/treeland-wine-window-management-v1
)
set(wine_client_header
    ${wine_generated_directory}/wayland-treeland-wine-window-management-v1-client-protocol.h
)
set(wine_client_code
    ${wine_generated_directory}/wayland-treeland-wine-window-management-v1-client-protocol.c
)
set(xdg_client_header ${wine_generated_directory}/xdg-shell-client-protocol.h)
set(xdg_client_code ${wine_generated_directory}/xdg-shell-protocol.c)
set(wine_test_adapter_metadata
    ${wine_generated_directory}/tl-test-treeland-wine-window-management-v1-metadata.json
)

add_custom_command(
    OUTPUT
        ${wine_client_header}
        ${wine_client_code}
        ${xdg_client_header}
        ${xdg_client_code}
        ${wine_test_adapter_metadata}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${wine_generated_directory}
    COMMAND ${WAYLAND_SCANNER} client-header ${wine_window_management_xml} ${wine_client_header}
    COMMAND ${WAYLAND_SCANNER} private-code ${wine_window_management_xml} ${wine_client_code}
    COMMAND ${WAYLAND_SCANNER} client-header ${xdg_shell_xml} ${xdg_client_header}
    COMMAND ${WAYLAND_SCANNER} private-code ${xdg_shell_xml} ${xdg_client_code}
    COMMAND qtwaylandscanner_treeland test-client-metadata ${wine_window_management_xml}
        > ${wine_test_adapter_metadata}
    DEPENDS
        ${wine_window_management_xml}
        ${xdg_shell_xml}
        qtwaylandscanner_treeland
    VERBATIM
)

add_library(protocol-wine-window-management-client-runtime STATIC
    wineclientworker.cpp
    wineclientworker.h
    ${wine_client_header}
    ${wine_client_code}
    ${xdg_client_header}
    ${xdg_client_code}
)
target_include_directories(protocol-wine-window-management-client-runtime
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${wine_generated_directory}
)
target_link_libraries(protocol-wine-window-management-client-runtime
    PUBLIC Qt6::Core PkgConfig::WAYLAND_CLIENT
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

set(fixed_test_xml ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/treeland-test-fixed-v1.xml)
set(fixed_generated_directory ${CMAKE_CURRENT_BINARY_DIR}/generated/treeland-test-fixed-v1)
set(fixed_client_header
    ${fixed_generated_directory}/wayland-treeland-test-fixed-v1-client-protocol.h)
set(fixed_client_code
    ${fixed_generated_directory}/wayland-treeland-test-fixed-v1-client-protocol.c)
set(fixed_server_header
    ${fixed_generated_directory}/wayland-treeland-test-fixed-v1-server-protocol.h)
set(fixed_adapter_header ${fixed_generated_directory}/tl-test-treeland-test-fixed-v1.h)
set(fixed_adapter_code ${fixed_generated_directory}/tl-test-treeland-test-fixed-v1.c)
set(fixed_adapter_metadata
    ${fixed_generated_directory}/tl-test-treeland-test-fixed-v1-metadata.json)

add_custom_command(
    OUTPUT
        ${fixed_client_header}
        ${fixed_client_code}
        ${fixed_server_header}
        ${fixed_adapter_header}
        ${fixed_adapter_code}
        ${fixed_adapter_metadata}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${fixed_generated_directory}
    COMMAND ${WAYLAND_SCANNER} client-header ${fixed_test_xml} ${fixed_client_header}
    COMMAND ${WAYLAND_SCANNER} private-code ${fixed_test_xml} ${fixed_client_code}
    COMMAND ${WAYLAND_SCANNER} server-header ${fixed_test_xml} ${fixed_server_header}
    COMMAND qtwaylandscanner_treeland test-client-header ${fixed_test_xml}
        > ${fixed_adapter_header}
    COMMAND qtwaylandscanner_treeland test-client-code ${fixed_test_xml}
        > ${fixed_adapter_code}
    COMMAND qtwaylandscanner_treeland test-client-metadata ${fixed_test_xml}
        > ${fixed_adapter_metadata}
    DEPENDS ${fixed_test_xml} qtwaylandscanner_treeland
    VERBATIM
)

add_executable(protocol-fixed-adapter-selftest
    fixedadapterselftest.cpp
    protocolfixedvalue.cpp
    protocolfixedvalue.h
    ${fixed_client_header}
    ${fixed_client_code}
    ${fixed_server_header}
    ${fixed_adapter_header}
    ${fixed_adapter_code}
)
target_include_directories(protocol-fixed-adapter-selftest PRIVATE ${fixed_generated_directory})
target_compile_definitions(protocol-fixed-adapter-selftest
    PRIVATE TL_FIXED_TEST_METADATA="${fixed_adapter_metadata}")
target_link_libraries(protocol-fixed-adapter-selftest
    PRIVATE Qt6::Test PkgConfig::WAYLAND_CLIENT PkgConfig::WAYLAND_SERVER)

add_test(NAME protocol-fixed-adapter-selftest COMMAND protocol-fixed-adapter-selftest)
set_tests_properties(protocol-fixed-adapter-selftest PROPERTIES
    LABELS "protocol-wire;generated-adapter;mvp-d4a-accepted"
    TIMEOUT 10
)

set(array_test_xml ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/treeland-test-array-v1.xml)
set(array_generated_directory ${CMAKE_CURRENT_BINARY_DIR}/generated/treeland-test-array-v1)
set(array_client_header
    ${array_generated_directory}/wayland-treeland-test-array-v1-client-protocol.h)
set(array_client_code
    ${array_generated_directory}/wayland-treeland-test-array-v1-client-protocol.c)
set(array_server_header
    ${array_generated_directory}/wayland-treeland-test-array-v1-server-protocol.h)
set(array_adapter_header ${array_generated_directory}/tl-test-treeland-test-array-v1.h)
set(array_adapter_code ${array_generated_directory}/tl-test-treeland-test-array-v1.c)
set(array_adapter_metadata
    ${array_generated_directory}/tl-test-treeland-test-array-v1-metadata.json)

add_custom_command(
    OUTPUT
        ${array_client_header}
        ${array_client_code}
        ${array_server_header}
        ${array_adapter_header}
        ${array_adapter_code}
        ${array_adapter_metadata}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${array_generated_directory}
    COMMAND ${WAYLAND_SCANNER} client-header ${array_test_xml} ${array_client_header}
    COMMAND ${WAYLAND_SCANNER} private-code ${array_test_xml} ${array_client_code}
    COMMAND ${WAYLAND_SCANNER} server-header ${array_test_xml} ${array_server_header}
    COMMAND qtwaylandscanner_treeland test-client-header ${array_test_xml}
        > ${array_adapter_header}
    COMMAND qtwaylandscanner_treeland test-client-code ${array_test_xml}
        > ${array_adapter_code}
    COMMAND qtwaylandscanner_treeland test-client-metadata ${array_test_xml}
        > ${array_adapter_metadata}
    DEPENDS ${array_test_xml} qtwaylandscanner_treeland
    VERBATIM
)

add_executable(protocol-array-adapter-selftest
    arrayadapterselftest.cpp
    protocolarrayvalue.cpp
    protocolarrayvalue.h
    ${array_client_header}
    ${array_client_code}
    ${array_server_header}
    ${array_adapter_header}
    ${array_adapter_code}
)
target_include_directories(protocol-array-adapter-selftest PRIVATE ${array_generated_directory})
target_compile_definitions(protocol-array-adapter-selftest
    PRIVATE TL_ARRAY_TEST_METADATA="${array_adapter_metadata}")
target_link_libraries(protocol-array-adapter-selftest
    PRIVATE Qt6::Test PkgConfig::WAYLAND_CLIENT PkgConfig::WAYLAND_SERVER)

add_test(NAME protocol-array-adapter-selftest COMMAND protocol-array-adapter-selftest)
set_tests_properties(protocol-array-adapter-selftest PROPERTIES
    LABELS "protocol-wire;generated-adapter;mvp-d4b-accepted"
    TIMEOUT 10
)

set(fd_test_xml ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/treeland-test-fd-v1.xml)
set(fd_generated_directory ${CMAKE_CURRENT_BINARY_DIR}/generated/treeland-test-fd-v1)
set(fd_client_header
    ${fd_generated_directory}/wayland-treeland-test-fd-v1-client-protocol.h)
set(fd_client_code
    ${fd_generated_directory}/wayland-treeland-test-fd-v1-client-protocol.c)
set(fd_server_header
    ${fd_generated_directory}/wayland-treeland-test-fd-v1-server-protocol.h)
set(fd_adapter_header ${fd_generated_directory}/tl-test-treeland-test-fd-v1.h)
set(fd_adapter_code ${fd_generated_directory}/tl-test-treeland-test-fd-v1.c)
set(fd_adapter_metadata
    ${fd_generated_directory}/tl-test-treeland-test-fd-v1-metadata.json)

add_custom_command(
    OUTPUT
        ${fd_client_header}
        ${fd_client_code}
        ${fd_server_header}
        ${fd_adapter_header}
        ${fd_adapter_code}
        ${fd_adapter_metadata}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${fd_generated_directory}
    COMMAND ${WAYLAND_SCANNER} client-header ${fd_test_xml} ${fd_client_header}
    COMMAND ${WAYLAND_SCANNER} private-code ${fd_test_xml} ${fd_client_code}
    COMMAND ${WAYLAND_SCANNER} server-header ${fd_test_xml} ${fd_server_header}
    COMMAND qtwaylandscanner_treeland test-client-header ${fd_test_xml}
        > ${fd_adapter_header}
    COMMAND qtwaylandscanner_treeland test-client-code ${fd_test_xml}
        > ${fd_adapter_code}
    COMMAND qtwaylandscanner_treeland test-client-metadata ${fd_test_xml}
        > ${fd_adapter_metadata}
    DEPENDS ${fd_test_xml} qtwaylandscanner_treeland
    VERBATIM
)

add_executable(protocol-fd-adapter-selftest
    fdadapterselftest.cpp
    protocolfdvalue.cpp
    protocolfdvalue.h
    ${fd_client_header}
    ${fd_client_code}
    ${fd_server_header}
    ${fd_adapter_header}
    ${fd_adapter_code}
)
target_include_directories(protocol-fd-adapter-selftest PRIVATE ${fd_generated_directory})
target_compile_definitions(protocol-fd-adapter-selftest
    PRIVATE TL_FD_TEST_METADATA="${fd_adapter_metadata}")
target_link_libraries(protocol-fd-adapter-selftest
    PRIVATE Qt6::Test PkgConfig::WAYLAND_CLIENT PkgConfig::WAYLAND_SERVER)

add_test(NAME protocol-fd-adapter-selftest COMMAND protocol-fd-adapter-selftest)
set_tests_properties(protocol-fd-adapter-selftest PROPERTIES
    LABELS "protocol-wire;generated-adapter;mvp-d4c"
    TIMEOUT 10
)

set(new_id_test_xml ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/treeland-test-new-id-v1.xml)
set(new_id_generated_directory ${CMAKE_CURRENT_BINARY_DIR}/generated/treeland-test-new-id-v1)
set(new_id_client_header
    ${new_id_generated_directory}/wayland-treeland-test-new-id-v1-client-protocol.h)
set(new_id_client_code
    ${new_id_generated_directory}/wayland-treeland-test-new-id-v1-client-protocol.c)
set(new_id_server_header
    ${new_id_generated_directory}/wayland-treeland-test-new-id-v1-server-protocol.h)
set(new_id_adapter_header ${new_id_generated_directory}/tl-test-treeland-test-new-id-v1.h)
set(new_id_adapter_code ${new_id_generated_directory}/tl-test-treeland-test-new-id-v1.c)
set(new_id_adapter_metadata
    ${new_id_generated_directory}/tl-test-treeland-test-new-id-v1-metadata.json)

add_custom_command(
    OUTPUT
        ${new_id_client_header}
        ${new_id_client_code}
        ${new_id_server_header}
        ${new_id_adapter_header}
        ${new_id_adapter_code}
        ${new_id_adapter_metadata}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${new_id_generated_directory}
    COMMAND ${WAYLAND_SCANNER} client-header ${new_id_test_xml} ${new_id_client_header}
    COMMAND ${WAYLAND_SCANNER} private-code ${new_id_test_xml} ${new_id_client_code}
    COMMAND ${WAYLAND_SCANNER} server-header ${new_id_test_xml} ${new_id_server_header}
    COMMAND qtwaylandscanner_treeland test-client-header ${new_id_test_xml}
        > ${new_id_adapter_header}
    COMMAND qtwaylandscanner_treeland test-client-code ${new_id_test_xml}
        > ${new_id_adapter_code}
    COMMAND qtwaylandscanner_treeland test-client-metadata ${new_id_test_xml}
        > ${new_id_adapter_metadata}
    DEPENDS ${new_id_test_xml} qtwaylandscanner_treeland
    VERBATIM
)

add_executable(protocol-new-id-adapter-selftest
    newidadapterselftest.cpp
    ${new_id_client_header}
    ${new_id_client_code}
    ${new_id_server_header}
    ${new_id_adapter_header}
    ${new_id_adapter_code}
)
target_include_directories(protocol-new-id-adapter-selftest PRIVATE ${new_id_generated_directory})
target_compile_definitions(protocol-new-id-adapter-selftest
    PRIVATE TL_NEW_ID_TEST_METADATA="${new_id_adapter_metadata}")
target_link_libraries(protocol-new-id-adapter-selftest
    PRIVATE Qt6::Test PkgConfig::WAYLAND_CLIENT PkgConfig::WAYLAND_SERVER)

add_test(NAME protocol-new-id-adapter-selftest COMMAND protocol-new-id-adapter-selftest)
set_tests_properties(protocol-new-id-adapter-selftest PROPERTIES
    LABELS "protocol-wire;generated-adapter;mvp-d4d"
    TIMEOUT 10
)

set(multi_arg_test_xml ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/treeland-test-multi-arg-v1.xml)
set(multi_arg_generated_directory ${CMAKE_CURRENT_BINARY_DIR}/generated/treeland-test-multi-arg-v1)
set(multi_arg_client_header
    ${multi_arg_generated_directory}/wayland-treeland-test-multi-arg-v1-client-protocol.h)
set(multi_arg_client_code
    ${multi_arg_generated_directory}/wayland-treeland-test-multi-arg-v1-client-protocol.c)
set(multi_arg_server_header
    ${multi_arg_generated_directory}/wayland-treeland-test-multi-arg-v1-server-protocol.h)
set(multi_arg_adapter_header ${multi_arg_generated_directory}/tl-test-treeland-test-multi-arg-v1.h)
set(multi_arg_adapter_code ${multi_arg_generated_directory}/tl-test-treeland-test-multi-arg-v1.c)
set(multi_arg_adapter_metadata
    ${multi_arg_generated_directory}/tl-test-treeland-test-multi-arg-v1-metadata.json)

add_custom_command(
    OUTPUT
        ${multi_arg_client_header}
        ${multi_arg_client_code}
        ${multi_arg_server_header}
        ${multi_arg_adapter_header}
        ${multi_arg_adapter_code}
        ${multi_arg_adapter_metadata}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${multi_arg_generated_directory}
    COMMAND ${WAYLAND_SCANNER} client-header ${multi_arg_test_xml} ${multi_arg_client_header}
    COMMAND ${WAYLAND_SCANNER} private-code ${multi_arg_test_xml} ${multi_arg_client_code}
    COMMAND ${WAYLAND_SCANNER} server-header ${multi_arg_test_xml} ${multi_arg_server_header}
    COMMAND qtwaylandscanner_treeland test-client-header ${multi_arg_test_xml}
        > ${multi_arg_adapter_header}
    COMMAND qtwaylandscanner_treeland test-client-code ${multi_arg_test_xml}
        > ${multi_arg_adapter_code}
    COMMAND qtwaylandscanner_treeland test-client-metadata ${multi_arg_test_xml}
        > ${multi_arg_adapter_metadata}
    DEPENDS ${multi_arg_test_xml} qtwaylandscanner_treeland
    VERBATIM
)

add_executable(protocol-multi-arg-adapter-selftest
    multiargadapterselftest.cpp
    ${multi_arg_client_header}
    ${multi_arg_client_code}
    ${multi_arg_server_header}
    ${multi_arg_adapter_header}
    ${multi_arg_adapter_code}
)
target_include_directories(protocol-multi-arg-adapter-selftest PRIVATE ${multi_arg_generated_directory})
target_compile_definitions(protocol-multi-arg-adapter-selftest
    PRIVATE TL_MULTI_ARG_TEST_METADATA="${multi_arg_adapter_metadata}")
target_link_libraries(protocol-multi-arg-adapter-selftest
    PRIVATE Qt6::Test PkgConfig::WAYLAND_CLIENT PkgConfig::WAYLAND_SERVER)

add_test(NAME protocol-multi-arg-adapter-selftest COMMAND protocol-multi-arg-adapter-selftest)
set_tests_properties(protocol-multi-arg-adapter-selftest PROPERTIES
    LABELS "protocol-wire;generated-adapter;mvp-d4e"
    TIMEOUT 10
)
add_test(
    NAME protocol-json-runner-multi-arg-echo
    COMMAND treeland-protocol-test-runner
        --input ${CMAKE_CURRENT_SOURCE_DIR}/cases/multi-arg-echo.input.json
        --expected ${CMAKE_CURRENT_SOURCE_DIR}/cases/multi-arg-echo.expected.json
        --metadata ${multi_arg_adapter_metadata}
        --protocol-xml ${multi_arg_test_xml}
        --dump-actual ${CMAKE_CURRENT_BINARY_DIR}/test-results/d4g/multi-arg-echo.actual.json
        --report-dir ${CMAKE_CURRENT_BINARY_DIR}/test-results/d4g/multi-arg-echo
)
set_tests_properties(protocol-json-runner-multi-arg-echo PROPERTIES
    LABELS "protocol-wire;json-runner;mvp-d4g"
    TIMEOUT 10
)

add_executable(protocol-global-version-selftest
    globalversionselftest.cpp
    ${multi_arg_client_header}
    ${multi_arg_client_code}
    ${multi_arg_server_header}
    ${multi_arg_adapter_header}
    ${multi_arg_adapter_code}
)
target_include_directories(protocol-global-version-selftest PRIVATE ${multi_arg_generated_directory})
target_link_libraries(protocol-global-version-selftest
    PRIVATE Qt6::Test PkgConfig::WAYLAND_CLIENT PkgConfig::WAYLAND_SERVER)

add_test(NAME protocol-global-version-selftest COMMAND protocol-global-version-selftest)
set_tests_properties(protocol-global-version-selftest PROPERTIES
    LABELS "protocol-wire;generated-adapter;mvp-d5"
    TIMEOUT 10
)

set(obj_newid_test_xml ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/treeland-test-object-newid-v1.xml)
set(obj_newid_generated_directory ${CMAKE_CURRENT_BINARY_DIR}/generated/treeland-test-object-newid-v1)
set(obj_newid_client_header
    ${obj_newid_generated_directory}/wayland-treeland-test-object-newid-v1-client-protocol.h)
set(obj_newid_client_code
    ${obj_newid_generated_directory}/wayland-treeland-test-object-newid-v1-client-protocol.c)
set(obj_newid_server_header
    ${obj_newid_generated_directory}/wayland-treeland-test-object-newid-v1-server-protocol.h)
set(obj_newid_adapter_header ${obj_newid_generated_directory}/tl-test-treeland-test-object-newid-v1.h)
set(obj_newid_adapter_code ${obj_newid_generated_directory}/tl-test-treeland-test-object-newid-v1.c)
set(obj_newid_adapter_metadata
    ${obj_newid_generated_directory}/tl-test-treeland-test-object-newid-v1-metadata.json)
add_custom_command(
    OUTPUT
        ${obj_newid_client_header}
        ${obj_newid_client_code}
        ${obj_newid_server_header}
        ${obj_newid_adapter_header}
        ${obj_newid_adapter_code}
        ${obj_newid_adapter_metadata}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${obj_newid_generated_directory}
    COMMAND ${WAYLAND_SCANNER} client-header ${obj_newid_test_xml} ${obj_newid_client_header}
    COMMAND ${WAYLAND_SCANNER} private-code ${obj_newid_test_xml} ${obj_newid_client_code}
    COMMAND ${WAYLAND_SCANNER} server-header ${obj_newid_test_xml} ${obj_newid_server_header}
    COMMAND qtwaylandscanner_treeland test-client-header ${obj_newid_test_xml}
        > ${obj_newid_adapter_header}
    COMMAND qtwaylandscanner_treeland test-client-code ${obj_newid_test_xml}
        > ${obj_newid_adapter_code}
    COMMAND qtwaylandscanner_treeland test-client-metadata ${obj_newid_test_xml}
        > ${obj_newid_adapter_metadata}
    DEPENDS ${obj_newid_test_xml} qtwaylandscanner_treeland
    VERBATIM
)
add_executable(protocol-object-newid-adapter-selftest
    objectnewidadapterselftest.cpp
    ${obj_newid_client_header}
    ${obj_newid_client_code}
    ${obj_newid_server_header}
    ${obj_newid_adapter_header}
    ${obj_newid_adapter_code}
)
target_include_directories(protocol-object-newid-adapter-selftest PRIVATE ${obj_newid_generated_directory})
target_compile_definitions(protocol-object-newid-adapter-selftest
    PRIVATE TL_OBJECT_NEWID_TEST_METADATA="${obj_newid_adapter_metadata}")
target_link_libraries(protocol-object-newid-adapter-selftest
    PRIVATE Qt6::Test PkgConfig::WAYLAND_CLIENT PkgConfig::WAYLAND_SERVER)
add_test(NAME protocol-object-newid-adapter-selftest COMMAND protocol-object-newid-adapter-selftest)
set_tests_properties(protocol-object-newid-adapter-selftest PROPERTIES
    LABELS "protocol-wire;generated-adapter;mvp-d4f"
    TIMEOUT 10
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
add_executable(treeland-protocol-test-runner
    protocoljsoncase.cpp
    protocoljsoncase.h
    protocoljsonmain.cpp
    protocoljsonscenario.cpp
    protocoljsonscenario.h
    wineprotocoljsonscenario.cpp
    wineprotocoljsonscenario.h
    winemulticlientscenario.cpp
    winemulticlientscenario.h
    genericprotocoljsonscenario.cpp
    ${multi_arg_client_code}
    ${multi_arg_adapter_code}
    ${CMAKE_BINARY_DIR}/src/modules/window-management/qwayland-server-treeland-window-management-v1.cpp
)
target_include_directories(treeland-protocol-test-runner PRIVATE ${multi_arg_generated_directory})
target_link_libraries(treeland-protocol-test-runner
    PRIVATE
        protocol-window-management-generated-client-runtime
        protocol-wine-window-management-client-runtime
        libtreeland
        Qt6::Test
)
target_compile_definitions(treeland-protocol-test-runner
    PRIVATE
        TL_PROTOCOL_TEST_METADATA="${test_adapter_metadata}"
        TL_PROTOCOL_TEST_XML="${window_management_xml}"
        TL_WINE_PROTOCOL_TEST_METADATA="${wine_test_adapter_metadata}"
        TL_WINE_PROTOCOL_TEST_XML="${wine_window_management_xml}"
        TL_PROTOCOL_TEST_DEFAULT_REPORT_DIR="${CMAKE_CURRENT_BINARY_DIR}/test-results/json-runner"
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
set(protocol_json_case_directory ${CMAKE_CURRENT_SOURCE_DIR}/cases)
set(protocol_json_good_input
    ${protocol_json_case_directory}/window-management-show.input.json
)
set(protocol_json_good_expected
    ${protocol_json_case_directory}/window-management-show.expected.json
)
function(add_wine_position_case name input expected)
    add_test(
        NAME protocol-wine-window-management-${name}
        COMMAND treeland-protocol-test-runner
            --input ${protocol_json_case_directory}/${input}
            --expected ${protocol_json_case_directory}/${expected}
            --dump-actual
                ${CMAKE_CURRENT_BINARY_DIR}/test-results/poc-3/${name}.actual.json
            --report-dir ${CMAKE_CURRENT_BINARY_DIR}/test-results/poc-3/${name}
    )
    set_tests_properties(protocol-wine-window-management-${name} PROPERTIES
        LABELS "protocol-high;json-runner;poc-3-accepted"
        TIMEOUT 30
    )
endfunction()
add_wine_position_case(
    set-position
    wine-set-position.input.json
    wine-set-position.expected.json
)
add_wine_position_case(
    set-position-outside-output
    wine-set-position-outside-output.input.json
    wine-set-position-outside-output.expected.json
)
add_wine_position_case(
    disconnect-before-completion
    wine-disconnect-before-completion.input.json
    wine-disconnect-before-completion.expected.json
)
add_wine_position_case(
    protocol-error-invalid-sibling
    wine-protocol-error-invalid-sibling.input.json
    wine-protocol-error-invalid-sibling.expected.json
)
set_tests_properties(
    protocol-wine-window-management-protocol-error-invalid-sibling
    PROPERTIES LABELS "protocol-high;json-runner;protocol-error;mvp-d1-accepted"
)
function(add_wine_lifecycle_case name)
    add_test(
        NAME protocol-wine-window-management-lifecycle-${name}
        COMMAND treeland-protocol-test-runner
            --input ${protocol_json_case_directory}/wine-lifecycle-${name}.input.json
            --expected ${protocol_json_case_directory}/wine-lifecycle-${name}.expected.json
            --dump-actual
                ${CMAKE_CURRENT_BINARY_DIR}/test-results/mvp-d2/${name}.actual.json
            --report-dir ${CMAKE_CURRENT_BINARY_DIR}/test-results/mvp-d2/${name}
    )
    set_tests_properties(protocol-wine-window-management-lifecycle-${name} PROPERTIES
        LABELS "protocol-high;json-runner;abrupt-disconnect;mvp-d2-accepted"
        TIMEOUT 30
    )
endfunction()
add_wine_lifecycle_case(protocol-destroy)
add_wine_lifecycle_case(proxy-only-destroy)
add_wine_lifecycle_case(graceful-disconnect)
add_wine_lifecycle_case(abrupt-disconnect)
add_wine_lifecycle_case(server-shutdown)
function(add_wine_multi_client_case name)
    add_test(
        NAME protocol-wine-window-management-multi-client-${name}
        COMMAND treeland-protocol-test-runner
            --input ${protocol_json_case_directory}/wine-multi-client-${name}.input.json
            --expected ${protocol_json_case_directory}/wine-multi-client-${name}.expected.json
            --dump-actual
                ${CMAKE_CURRENT_BINARY_DIR}/test-results/mvp-d3/${name}.actual.json
            --report-dir ${CMAKE_CURRENT_BINARY_DIR}/test-results/mvp-d3/${name}
    )
    set_tests_properties(protocol-wine-window-management-multi-client-${name} PROPERTIES
        LABELS "protocol-high;json-runner;multi-client;mvp-d3-accepted"
        TIMEOUT 40
    )
endfunction()
add_wine_multi_client_case(disconnect-isolation)
add_wine_multi_client_case(protocol-error-isolation)
add_test(
    NAME protocol-wire-window-management-json
    COMMAND treeland-protocol-test-runner
        --input ${protocol_json_good_input}
        --expected ${protocol_json_good_expected}
        --dump-actual ${CMAKE_CURRENT_BINARY_DIR}/test-results/json-runner/actual.json
        --report-dir ${CMAKE_CURRENT_BINARY_DIR}/test-results/json-runner
)
set_tests_properties(protocol-wire-window-management-json PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    LABELS "protocol-wire;json-runner"
    TIMEOUT 10
)
function(add_protocol_json_selftest name input expected category)
    add_test(
        NAME protocol-json-runner-${name}-selftest
        COMMAND ${CMAKE_COMMAND}
            -DSELFTEST_EXECUTABLE=$<TARGET_FILE:treeland-protocol-test-runner>
            -DINPUT=${input}
            -DEXPECTED=${expected}
            -DREPORT_DIRECTORY=${CMAKE_CURRENT_BINARY_DIR}/test-results/json-${name}-selftest
            -DEXPECTED_CATEGORY=${category}
            ${ARGN}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/verify-json-runner-selftest.cmake
    )
    set_tests_properties(protocol-json-runner-${name}-selftest PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        LABELS "protocol-wire;json-runner;runner-selftest"
        TIMEOUT 10
    )
endfunction()
add_protocol_json_selftest(
    invalid-schema
    ${protocol_json_case_directory}/selftest-invalid-schema.input.json
    ${protocol_json_good_expected}
    schema_error
)
add_protocol_json_selftest(
    unknown-request
    ${protocol_json_case_directory}/selftest-unknown-request.input.json
    ${protocol_json_good_expected}
    metadata_validation_error
)
add_protocol_json_selftest(
    wrong-argument-type
    ${protocol_json_case_directory}/selftest-wrong-argument-type.input.json
    ${protocol_json_good_expected}
    adapter_validation_error
)
add_protocol_json_selftest(
    metadata-mismatch
    ${protocol_json_case_directory}/selftest-metadata-mismatch.input.json
    ${protocol_json_good_expected}
    metadata_validation_error
)
add_protocol_json_selftest(
    wrong-expected-event
    ${protocol_json_good_input}
    ${protocol_json_case_directory}/selftest-wrong-event.expected.json
    checkpoint_event_diff
    -DEXPECTED_CHECKPOINT=request-state
)
add_protocol_json_selftest(
    wrong-server-state
    ${protocol_json_good_input}
    ${protocol_json_case_directory}/selftest-wrong-server-state.expected.json
    checkpoint_probe_diff
    -DEXPECTED_CHECKPOINT=request-state
)
add_protocol_json_selftest(
    case-id-mismatch
    ${protocol_json_good_input}
    ${protocol_json_case_directory}/selftest-case-id-mismatch.expected.json
    schema_error
)
add_protocol_json_selftest(
    malformed-expected
    ${protocol_json_good_input}
    ${protocol_json_case_directory}/selftest-malformed.expected.json
    schema_error
)
add_protocol_json_selftest(
    wine-wrong-position
    ${protocol_json_case_directory}/wine-set-position.input.json
    ${protocol_json_case_directory}/selftest-wine-wrong-position.expected.json
    checkpoint_probe_diff
    -DEXPECTED_CHECKPOINT=position-applied
)
add_protocol_json_selftest(
    protocol-error-strict-validation
    ${protocol_json_case_directory}/selftest-wine-protocol-error-strict.input.json
    ${protocol_json_case_directory}/wine-protocol-error-invalid-sibling.expected.json
    adapter_validation_error
)
add_protocol_json_selftest(
    protocol-error-wrong-code
    ${protocol_json_case_directory}/wine-protocol-error-invalid-sibling.input.json
    ${protocol_json_case_directory}/selftest-wine-protocol-error-wrong-code.expected.json
    checkpoint_protocol_error_diff
    -DEXPECTED_CHECKPOINT=invalid-sibling-error
)
add_protocol_json_selftest(
    protocol-error-wrong-object
    ${protocol_json_case_directory}/wine-protocol-error-invalid-sibling.input.json
    ${protocol_json_case_directory}/selftest-wine-protocol-error-wrong-object.expected.json
    checkpoint_protocol_error_diff
    -DEXPECTED_CHECKPOINT=invalid-sibling-error
)
add_protocol_json_selftest(
    lifecycle-resource-not-restored
    ${protocol_json_case_directory}/wine-lifecycle-abrupt-disconnect.input.json
    ${protocol_json_case_directory}/selftest-wine-lifecycle-resource-not-restored.expected.json
    checkpoint_lifecycle_diff
    -DEXPECTED_CHECKPOINT=disconnected
)
add_protocol_json_selftest(
    multi-client-wrong-attribution
    ${protocol_json_case_directory}/wine-multi-client-disconnect-isolation.input.json
    ${protocol_json_case_directory}/selftest-wine-multi-client-wrong-attribution.expected.json
    checkpoint_client_isolation_diff
    -DEXPECTED_CHECKPOINT=client-1-operated
)
set_tests_properties(
    protocol-json-runner-protocol-error-strict-validation-selftest
    protocol-json-runner-protocol-error-wrong-code-selftest
    protocol-json-runner-protocol-error-wrong-object-selftest
    PROPERTIES LABELS "protocol-high;json-runner;runner-selftest;protocol-error"
)
set_tests_properties(
    protocol-json-runner-lifecycle-resource-not-restored-selftest
    PROPERTIES LABELS "protocol-high;json-runner;runner-selftest;abrupt-disconnect;mvp-d2-accepted"
)
set_tests_properties(
    protocol-json-runner-multi-client-wrong-attribution-selftest
    PROPERTIES LABELS "protocol-high;json-runner;runner-selftest;multi-client;mvp-d3-accepted"
        TIMEOUT 40
)
add_executable(protocol-json-contract-diff
    protocoljsoncontractdiff.cpp
)
target_link_libraries(protocol-json-contract-diff PRIVATE Qt6::Core)
add_test(
    NAME protocol-window-management-json-contract
    COMMAND ${CMAKE_COMMAND}
        -DWIRE_EXECUTABLE=$<TARGET_FILE:test-protocol-window-management-wire-generated>
        -DJSON_EXECUTABLE=$<TARGET_FILE:treeland-protocol-test-runner>
        -DCONTRACT_DIFF=$<TARGET_FILE:protocol-json-contract-diff>
        -DINPUT=${protocol_json_good_input}
        -DEXPECTED=${protocol_json_good_expected}
        -DREPORT_DIRECTORY=${CMAKE_CURRENT_BINARY_DIR}/test-results/json-contract
        -P ${CMAKE_CURRENT_SOURCE_DIR}/verify-json-contract.cmake
)
set_tests_properties(protocol-window-management-json-contract PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    LABELS "protocol-wire;generated-adapter;json-runner;contract-diff"
    TIMEOUT 20
)
add_test(
    NAME protocol-window-management-json-repeatable
    COMMAND ${CMAKE_COMMAND}
        -DJSON_EXECUTABLE=$<TARGET_FILE:treeland-protocol-test-runner>
        -DINPUT=${protocol_json_good_input}
        -DEXPECTED=${protocol_json_good_expected}
        -DREPORT_DIRECTORY=${CMAKE_CURRENT_BINARY_DIR}/test-results/json-repeatable
        -P ${CMAKE_CURRENT_SOURCE_DIR}/verify-json-repeatable.cmake
)
set_tests_properties(protocol-window-management-json-repeatable PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    LABELS "protocol-wire;json-runner;contract-diff"
    TIMEOUT 20
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
add_subdirectory(gen-test-client)

# === gen-test-client: generated standalone clients ===

# Helper: define a gen-client E2E test
# Args: TEST_NAME XML_VAR GEN_DIR CLIENT_DEPS SERVER_SRC SERVER_DEPS REQUEST EXPECT
function(add_gen_client_test TEST_NAME XML_VAR GEN_DIR CLIENT_LIBS SERVER_SRC SERVER_LIBS REQUEST EXPECT)
    set(GEN_SRC ${CMAKE_CURRENT_BINARY_DIR}/gen-test-client-output/${TEST_NAME}-gen.c)
    set(GEN_BIN ${CMAKE_CURRENT_BINARY_DIR}/gen-test-client-output/${TEST_NAME}-gen-client)
    add_custom_command(
        OUTPUT ${GEN_SRC}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/gen-test-client-output
        COMMAND gen-test-client ${${XML_VAR}} ${GEN_SRC}
        DEPENDS gen-test-client ${${XML_VAR}}
        VERBATIM
    )
    add_executable(${TEST_NAME}-gen-client ${GEN_SRC} ${${CLIENT_LIBS}})
    target_include_directories(${TEST_NAME}-gen-client PRIVATE ${${GEN_DIR}})
    target_link_libraries(${TEST_NAME}-gen-client PRIVATE PkgConfig::WAYLAND_CLIENT)
    set_target_properties(${TEST_NAME}-gen-client PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/gen-test-client-output)
    add_executable(${TEST_NAME}-echo-server ${SERVER_SRC} ${${SERVER_LIBS}})
    target_include_directories(${TEST_NAME}-echo-server PRIVATE ${${GEN_DIR}})
    target_link_libraries(${TEST_NAME}-echo-server PRIVATE PkgConfig::WAYLAND_SERVER)
    add_executable(${TEST_NAME}-e2e gencliente2etest.cpp)
    target_compile_definitions(${TEST_NAME}-e2e PRIVATE
        TL_CLIENT_PATH="${GEN_BIN}"
        TL_SERVER_PATH="$<TARGET_FILE:${TEST_NAME}-echo-server>"
        TL_REQUEST="${REQUEST}"
        TL_EXPECT="${EXPECT}")
    target_link_libraries(${TEST_NAME}-e2e PRIVATE Qt6::Core)
    add_dependencies(${TEST_NAME}-e2e ${TEST_NAME}-gen-client ${TEST_NAME}-echo-server)
    add_test(NAME ${TEST_NAME}-e2e COMMAND ${TEST_NAME}-e2e)
    set_tests_properties(${TEST_NAME}-e2e PROPERTIES LABELS "protocol-wire;gen-client" TIMEOUT 15)
endfunction()

add_gen_client_test(test-multi-arg multi_arg_test_xml multi_arg_generated_directory multi_arg_client_code
    echoserver.c multi_arg_client_code "echo 42 -7 hello" "id=42")
add_gen_client_test(test-fixed fixed_test_xml fixed_generated_directory fixed_client_code
    fixedechoserver.c fixed_client_code "echo 1.5" "value=384")
add_gen_client_test(test-array array_test_xml array_generated_directory array_client_code
    arrayechoserver.c array_client_code "echo hex:00ff" "EVENT value")
add_gen_client_test(test-fd fd_test_xml fd_generated_directory fd_client_code
    fdechoserver.c fd_client_code "echo fd:pipe" "EVENT value")
