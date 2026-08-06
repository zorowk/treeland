find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(TreelandProtocols REQUIRED)
pkg_search_module(WAYLAND_CLIENT REQUIRED IMPORTED_TARGET wayland-client)
pkg_search_module(WAYLAND_SERVER REQUIRED IMPORTED_TARGET wayland-server)
pkg_get_variable(WAYLAND_SCANNER wayland-scanner wayland_scanner)

# ---- Test protocol fixtures (self-contained, not from TreelandProtocols) ----

set(fixed_test_xml      ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/treeland-test-fixed-v1.xml)
set(fixed_generated_directory ${CMAKE_CURRENT_BINARY_DIR}/generated/treeland-test-fixed-v1)
set(fixed_client_header  ${fixed_generated_directory}/wayland-treeland-test-fixed-v1-client-protocol.h)
set(fixed_client_code    ${fixed_generated_directory}/wayland-treeland-test-fixed-v1-client-protocol.c)
set(fixed_server_header  ${fixed_generated_directory}/wayland-treeland-test-fixed-v1-server-protocol.h)

set(array_test_xml      ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/treeland-test-array-v1.xml)
set(array_generated_directory ${CMAKE_CURRENT_BINARY_DIR}/generated/treeland-test-array-v1)
set(array_client_header  ${array_generated_directory}/wayland-treeland-test-array-v1-client-protocol.h)
set(array_client_code    ${array_generated_directory}/wayland-treeland-test-array-v1-client-protocol.c)
set(array_server_header  ${array_generated_directory}/wayland-treeland-test-array-v1-server-protocol.h)

set(fd_test_xml         ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/treeland-test-fd-v1.xml)
set(fd_generated_directory ${CMAKE_CURRENT_BINARY_DIR}/generated/treeland-test-fd-v1)
set(fd_client_header     ${fd_generated_directory}/wayland-treeland-test-fd-v1-client-protocol.h)
set(fd_client_code       ${fd_generated_directory}/wayland-treeland-test-fd-v1-client-protocol.c)
set(fd_server_header     ${fd_generated_directory}/wayland-treeland-test-fd-v1-server-protocol.h)

set(new_id_test_xml     ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/treeland-test-new-id-v1.xml)
set(new_id_generated_directory ${CMAKE_CURRENT_BINARY_DIR}/generated/treeland-test-new-id-v1)
set(new_id_client_header ${new_id_generated_directory}/wayland-treeland-test-new-id-v1-client-protocol.h)
set(new_id_client_code   ${new_id_generated_directory}/wayland-treeland-test-new-id-v1-client-protocol.c)
set(new_id_server_header ${new_id_generated_directory}/wayland-treeland-test-new-id-v1-server-protocol.h)

set(multi_arg_test_xml   ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/treeland-test-multi-arg-v1.xml)
set(multi_arg_generated_directory ${CMAKE_CURRENT_BINARY_DIR}/generated/treeland-test-multi-arg-v1)
set(multi_arg_client_header ${multi_arg_generated_directory}/wayland-treeland-test-multi-arg-v1-client-protocol.h)
set(multi_arg_client_code   ${multi_arg_generated_directory}/wayland-treeland-test-multi-arg-v1-client-protocol.c)
set(multi_arg_server_header ${multi_arg_generated_directory}/wayland-treeland-test-multi-arg-v1-server-protocol.h)

set(obj_newid_test_xml   ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/treeland-test-object-newid-v1.xml)
set(obj_newid_generated_directory ${CMAKE_CURRENT_BINARY_DIR}/generated/treeland-test-object-newid-v1)
set(obj_newid_client_header ${obj_newid_generated_directory}/wayland-treeland-test-object-newid-v1-client-protocol.h)
set(obj_newid_client_code   ${obj_newid_generated_directory}/wayland-treeland-test-object-newid-v1-client-protocol.c)
set(obj_newid_server_header ${obj_newid_generated_directory}/wayland-treeland-test-object-newid-v1-server-protocol.h)

# ---- wayland-scanner: wire stubs for each test fixture ----
function(add_fixture_stubs XML_VAR GEN_DIR CLIENT_H CLIENT_C SERVER_H)
    add_custom_command(
        OUTPUT ${${CLIENT_H}} ${${CLIENT_C}} ${${SERVER_H}}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${${GEN_DIR}}
        COMMAND ${WAYLAND_SCANNER} client-header ${${XML_VAR}} ${${CLIENT_H}}
        COMMAND ${WAYLAND_SCANNER} private-code ${${XML_VAR}} ${${CLIENT_C}}
        COMMAND ${WAYLAND_SCANNER} server-header ${${XML_VAR}} ${${SERVER_H}}
        DEPENDS ${${XML_VAR}}
        VERBATIM
    )
endfunction()

add_fixture_stubs(fixed_test_xml fixed_generated_directory fixed_client_header fixed_client_code fixed_server_header)
add_fixture_stubs(array_test_xml array_generated_directory array_client_header array_client_code array_server_header)
add_fixture_stubs(fd_test_xml fd_generated_directory fd_client_header fd_client_code fd_server_header)
add_fixture_stubs(new_id_test_xml new_id_generated_directory new_id_client_header new_id_client_code new_id_server_header)
add_fixture_stubs(multi_arg_test_xml multi_arg_generated_directory multi_arg_client_header multi_arg_client_code multi_arg_server_header)
add_fixture_stubs(obj_newid_test_xml obj_newid_generated_directory obj_newid_client_header obj_newid_client_code obj_newid_server_header)

# ---- gen-test-client: standalone C test client generator ----
add_subdirectory(gen-test-client)

# ---- E2E test helper ----
function(add_gen_client_test TEST_NAME XML_VAR GEN_DIR CLIENT_LIBS SERVER_SRC SERVER_LIBS REQUEST EXPECT)
    set(GEN_SRC ${CMAKE_CURRENT_BINARY_DIR}/gen-test-client-output/${TEST_NAME}-gen.c)
    set(GEN_BIN ${CMAKE_CURRENT_BINARY_DIR}/gen-test-client-output/${TEST_NAME}-gen-client)
    add_custom_command(OUTPUT ${GEN_SRC}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/gen-test-client-output
        COMMAND gen-test-client ${${XML_VAR}} ${GEN_SRC}
        DEPENDS gen-test-client ${${XML_VAR}} VERBATIM)
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
        TL_CLIENT_PATH="${GEN_BIN}" TL_SERVER_PATH="$<TARGET_FILE:${TEST_NAME}-echo-server>"
        TL_REQUEST="${REQUEST}" TL_EXPECT="${EXPECT}")
    target_link_libraries(${TEST_NAME}-e2e PRIVATE Qt6::Core)
    add_dependencies(${TEST_NAME}-e2e ${TEST_NAME}-gen-client ${TEST_NAME}-echo-server)
    add_test(NAME ${TEST_NAME}-e2e COMMAND ${TEST_NAME}-e2e)
    set_tests_properties(${TEST_NAME}-e2e PROPERTIES LABELS "protocol-wire;gen-client" TIMEOUT 15)
endfunction()

add_gen_client_test(test-multi-arg   multi_arg_test_xml   multi_arg_generated_directory   multi_arg_client_code   echoserver.c       multi_arg_client_code   "echo 42 -7 hello"     "id=42")
add_gen_client_test(test-fixed       fixed_test_xml       fixed_generated_directory       fixed_client_code       fixedechoserver.c   fixed_client_code       "echo 1.5"             "value=384")
add_gen_client_test(test-array       array_test_xml       array_generated_directory       array_client_code       arrayechoserver.c   array_client_code       "echo hex:00ff"         "EVENT value")
add_gen_client_test(test-fd          fd_test_xml          fd_generated_directory          fd_client_code          fdechoserver.c      fd_client_code          "echo fd:pipe"          "EVENT value")
add_gen_client_test(test-new-id      new_id_test_xml      new_id_generated_directory      new_id_client_code      newidechoserver.c   new_id_client_code      "create_child"          "EVENT treeland_test_child_v1.done")
add_gen_client_test(test-object-newid obj_newid_test_xml  obj_newid_generated_directory   obj_newid_client_code   objnewidechoserver.c obj_newid_client_code   "create_child"          "EVENT treeland_test_child_v1.done")


# ============================================================
# Auto-generated: iterate all treeland protocol XMLs
# ============================================================

# Module registry: protocol XML → (header, class, module_dir)
# Only protocols listed here get json-controller tests.
# Others still get gen-test-client generation.
set(TREELAND_MODULE_REGISTRY
    "treeland-window-management-v1|windowmanagementinterfacev1.h|WindowManagementInterfaceV1|window-management"
    "treeland-wallpaper-color-v1|wallpapercolorinterfacev1.h|WallpaperColorInterfaceV1|wallpaper-color"
    "treeland-ddm-v1|ddminterfacev1.h|DDMInterfaceV1|ddm"
    "treeland-screensaver-v1|screensaverinterfacev1.h|ScreensaverInterfaceV1|screensaver"
    "treeland-virtual-output-manager-v1|virtualoutputmanagerinterfacev1.h|VirtualOutputInterfaceV1|virtual-output"
    "treeland-keyboard-state-notify-unstable-v1|keyboardstatenotifymanagerinterfacev1.h|KeyboardStateWatcherV1|keyboard-state-notify"
)

# Find all treeland protocol XMLs
file(GLOB_RECURSE TREELAND_XMLS "${TREELAND_PROTOCOLS_DATA_DIR}/treeland-*.xml")

foreach(XML_PATH ${TREELAND_XMLS})
    # Extract protocol base name from filename
    get_filename_component(PROTO_FILE "${XML_PATH}" NAME_WE)
    # treeland-foo-bar-v1 → treeland_foo_bar_v1_xml
    string(REPLACE "-" "_" PROTO_VAR "${PROTO_FILE}")

    # Set variables for this protocol
    set(${PROTO_VAR}_xml "${XML_PATH}")
    set(${PROTO_VAR}_gen_dir "${CMAKE_CURRENT_BINARY_DIR}/generated/${PROTO_FILE}")
    set(${PROTO_VAR}_client_h "${${PROTO_VAR}_gen_dir}/wayland-${PROTO_FILE}-client-protocol.h")
    set(${PROTO_VAR}_client_c "${${PROTO_VAR}_gen_dir}/wayland-${PROTO_FILE}-client-protocol.c")

    # Generate wayland-scanner stubs
    add_custom_command(
        OUTPUT "${${PROTO_VAR}_client_h}" "${${PROTO_VAR}_client_c}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${${PROTO_VAR}_gen_dir}"
        COMMAND ${WAYLAND_SCANNER} client-header "${XML_PATH}" "${${PROTO_VAR}_client_h}"
        COMMAND ${WAYLAND_SCANNER} private-code "${XML_PATH}" "${${PROTO_VAR}_client_c}"
        DEPENDS "${XML_PATH}"
        VERBATIM
    )

    # Check if this protocol is in the module registry
    set(FOUND_CLASS "")
    foreach(ENTRY ${TREELAND_MODULE_REGISTRY})
        list(GET ENTRY 0 ENTRY_PROTO)
        if("${PROTO_FILE}" STREQUAL "${ENTRY_PROTO}")
            list(GET ENTRY 1 ENTRY_HEADER)
            list(GET ENTRY 2 ENTRY_CLASS)
            list(GET ENTRY 3 ENTRY_DIR)
            set(FOUND_CLASS "${ENTRY_CLASS}")
            set(FOUND_HEADER "${ENTRY_HEADER}")
            set(FOUND_DIR "${ENTRY_DIR}")
            break()
        endif()
    endforeach()

    # Generate gen-test-client client (always)
    set(GEN_CLIENT_SRC "${CMAKE_CURRENT_BINARY_DIR}/gen-test-client-output/${PROTO_FILE}-gen.c")
    add_custom_command(
        OUTPUT "${GEN_CLIENT_SRC}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/gen-test-client-output"
        COMMAND gen-test-client "${XML_PATH}" "${GEN_CLIENT_SRC}"
        DEPENDS gen-test-client "${XML_PATH}"
        VERBATIM
    )

    # If in registry, also build json-controller test
    if(FOUND_CLASS)
        add_json_protocol_test("${PROTO_FILE}"
            ${PROTO_VAR}_xml ${PROTO_VAR}_gen_dir ${PROTO_VAR}_client_c
            "${FOUND_HEADER}" "${FOUND_CLASS}" "${FOUND_DIR}" "${PROTO_FILE}")
    endif()
endforeach()


function(add_wayland_protocol_test NAME PROTO_FILE CLIENT_H CLIENT_C)
    set(GEN_SRC "${CMAKE_CURRENT_BINARY_DIR}/gen-test-client-output/${PROTO_FILE}-gen.c")
    set(GEN_BIN "${CMAKE_CURRENT_BINARY_DIR}/gen-test-client-output/${PROTO_FILE}-gen-client")
    
    # Already have wayland-scanner stubs from the loop
    # Already have gen-test-client generation from the loop
    
    add_executable("${NAME}-gen-client" "${GEN_SRC}" "${CLIENT_C}")
    target_include_directories("${NAME}-gen-client" PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated/${PROTO_FILE}")
    target_link_libraries("${NAME}-gen-client" PRIVATE PkgConfig::WAYLAND_CLIENT)
    set_target_properties("${NAME}-gen-client" PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/gen-test-client-output")
    
    add_executable("${NAME}-json-test" json-controller.cpp)
    target_compile_definitions("${NAME}-json-test" PRIVATE
        TL_CLIENT_PATH="${GEN_BIN}"
        TL_CASE="${CMAKE_CURRENT_SOURCE_DIR}/cases/${PROTO_FILE}.json"
        TL_MODULE_HEADER_STR=""
        TL_MODULE_CLASS=int
        NO_MODULE_ATTACH)
    target_link_libraries("${NAME}-json-test" PRIVATE Qt6::Core libtreeland)
    add_dependencies("${NAME}-json-test" "${NAME}-gen-client")
    add_test(NAME "${NAME}-json-test" COMMAND "${NAME}-json-test")
    set_tests_properties("${NAME}-json-test" PROPERTIES LABELS "protocol-high;std-wayland" TIMEOUT 30)
endfunction()

# Also scan standard wayland protocols
file(GLOB_RECURSE WAYLAND_XMLS "/usr/share/wayland-protocols/*.xml")
foreach(XML_PATH ${WAYLAND_XMLS})
    get_filename_component(PROTO_FILE "${XML_PATH}" NAME_WE)
    string(REPLACE "-" "_" PROTO_VAR "${PROTO_FILE}")
    set(${PROTO_VAR}_client_h "${CMAKE_CURRENT_BINARY_DIR}/generated/${PROTO_FILE}/wayland-${PROTO_FILE}-client-protocol.h")
    set(${PROTO_VAR}_client_c "${CMAKE_CURRENT_BINARY_DIR}/generated/${PROTO_FILE}/wayland-${PROTO_FILE}-client-protocol.c")
    add_custom_command(
        OUTPUT "${${PROTO_VAR}_client_h}" "${${PROTO_VAR}_client_c}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/generated/${PROTO_FILE}"
        COMMAND ${WAYLAND_SCANNER} client-header "${XML_PATH}" "${${PROTO_VAR}_client_h}"
        COMMAND ${WAYLAND_SCANNER} private-code "${XML_PATH}" "${${PROTO_VAR}_client_c}"
        DEPENDS "${XML_PATH}" VERBATIM
    )
    
    # Generate gen-test-client for standard protocols too
    set(GEN_CLIENT_SRC "${CMAKE_CURRENT_BINARY_DIR}/gen-test-client-output/${PROTO_FILE}-gen.c")
    add_custom_command(
        OUTPUT "${GEN_CLIENT_SRC}"
        COMMAND gen-test-client "${XML_PATH}" "${GEN_CLIENT_SRC}"
        DEPENDS gen-test-client "${XML_PATH}" VERBATIM
    )
    
    # Add test for protocols we have JSON cases for
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cases/${PROTO_FILE}.json")
        add_wayland_protocol_test("std-${PROTO_FILE}" "${PROTO_FILE}" "${${PROTO_VAR}_client_h}" "${${PROTO_VAR}_client_c}")
    endif()
endforeach()
