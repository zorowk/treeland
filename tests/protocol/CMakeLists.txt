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
# ============================================================
# Auto-generated from cases/*.json
# JSON format: {"xml_path":..., "module":{"class":..., "header":..., "dir":...}}
# Standard wayland protocols omit "module" -> NO_MODULE_ATTACH
# ============================================================

function(add_json_protocol_test PROTO_FILE XML_PATH MOD_HEADER MOD_CLASS MOD_DIR)
    get_filename_component(NAME_BASE "${XML_PATH}" NAME_WE)
    string(REPLACE "-" "_" PVAR "${NAME_BASE}")
    set(GEN_D "${CMAKE_CURRENT_BINARY_DIR}/generated/${NAME_BASE}")
    set(CL_H "${GEN_D}/wayland-${NAME_BASE}-client-protocol.h")
    set(CL_C "${GEN_D}/wayland-${NAME_BASE}-client-protocol.c")
    add_custom_command(OUTPUT "${CL_H}" "${CL_C}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${GEN_D}"
        COMMAND ${WAYLAND_SCANNER} client-header "${XML_PATH}" "${CL_H}"
        COMMAND ${WAYLAND_SCANNER} private-code "${XML_PATH}" "${CL_C}"
        DEPENDS "${XML_PATH}" VERBATIM)
    set(GEN_SRC "${CMAKE_CURRENT_BINARY_DIR}/gen-test-client-output/${NAME_BASE}-gen.c")
    set(GEN_BIN "${CMAKE_CURRENT_BINARY_DIR}/gen-test-client-output/${NAME_BASE}-gen-client")
    add_custom_command(OUTPUT "${GEN_SRC}"
        COMMAND gen-test-client "${XML_PATH}" "${GEN_SRC}"
        DEPENDS gen-test-client "${XML_PATH}" VERBATIM)
    add_executable("${PROTO_FILE}-gen-client" "${GEN_SRC}" "${CL_C}" "${XDG_SHELL_CLIENT_C}")
    target_include_directories("${PROTO_FILE}-gen-client" PRIVATE "${GEN_D}" "${XDG_SHELL_INCLUDE_DIR}")
    target_link_libraries("${PROTO_FILE}-gen-client" PRIVATE PkgConfig::WAYLAND_CLIENT)
    set_target_properties("${PROTO_FILE}-gen-client" PROPERTIES
        AUTOMOC OFF AUTOUIC OFF
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/gen-test-client-output")
    add_executable("${PROTO_FILE}-json-test" json-controller.cpp)
    target_compile_definitions("${PROTO_FILE}-json-test" PRIVATE
        TL_CLIENT_PATH="${GEN_BIN}"
        TL_CASE="${CMAKE_CURRENT_SOURCE_DIR}/cases/${PROTO_FILE}.json"
        TL_MODULE_HEADER_STR="${MOD_HEADER}"
        TL_MODULE_CLASS=${MOD_CLASS})
    target_include_directories("${PROTO_FILE}-json-test" PRIVATE "${CMAKE_SOURCE_DIR}/src/modules/${MOD_DIR}")
    target_link_libraries("${PROTO_FILE}-json-test" PRIVATE Qt6::Core libtreeland)
    set_target_properties("${PROTO_FILE}-json-test" PROPERTIES AUTOMOC OFF AUTOUIC OFF)
    add_dependencies("${PROTO_FILE}-json-test" "${PROTO_FILE}-gen-client")
    add_test(NAME "${PROTO_FILE}-json-test" COMMAND "${PROTO_FILE}-json-test")
    set_tests_properties("${PROTO_FILE}-json-test" PROPERTIES LABELS "protocol-high;treeland" TIMEOUT 30)
endfunction()

function(add_wayland_protocol_test PROTO_FILE XML_PATH)
    get_filename_component(NAME_BASE "${XML_PATH}" NAME_WE)
    string(REPLACE "-" "_" PVAR "${NAME_BASE}")
    set(GEN_D "${CMAKE_CURRENT_BINARY_DIR}/generated/${NAME_BASE}")
    set(CL_H "${GEN_D}/wayland-${NAME_BASE}-client-protocol.h")
    set(CL_C "${GEN_D}/wayland-${NAME_BASE}-client-protocol.c")
    add_custom_command(OUTPUT "${CL_H}" "${CL_C}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${GEN_D}"
        COMMAND ${WAYLAND_SCANNER} client-header "${XML_PATH}" "${CL_H}"
        COMMAND ${WAYLAND_SCANNER} private-code "${XML_PATH}" "${CL_C}"
        DEPENDS "${XML_PATH}" VERBATIM)
    set(GEN_SRC "${CMAKE_CURRENT_BINARY_DIR}/gen-test-client-output/${NAME_BASE}-gen.c")
    set(GEN_BIN "${CMAKE_CURRENT_BINARY_DIR}/gen-test-client-output/${NAME_BASE}-gen-client")
    add_custom_command(OUTPUT "${GEN_SRC}"
        COMMAND gen-test-client "${XML_PATH}" "${GEN_SRC}"
        DEPENDS gen-test-client "${XML_PATH}" VERBATIM)
    add_executable("${PROTO_FILE}-gen-client" "${GEN_SRC}" "${CL_C}" "${XDG_SHELL_CLIENT_C}")
    target_include_directories("${PROTO_FILE}-gen-client" PRIVATE "${GEN_D}" "${XDG_SHELL_INCLUDE_DIR}")
    target_link_libraries("${PROTO_FILE}-gen-client" PRIVATE PkgConfig::WAYLAND_CLIENT)
    set_target_properties("${PROTO_FILE}-gen-client" PROPERTIES
        AUTOMOC OFF AUTOUIC OFF
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/gen-test-client-output")
    add_executable("${PROTO_FILE}-json-test" json-controller.cpp)
    target_compile_definitions("${PROTO_FILE}-json-test" PRIVATE
        TL_CLIENT_PATH="${GEN_BIN}"
        TL_CASE="${CMAKE_CURRENT_SOURCE_DIR}/cases/${PROTO_FILE}.json"
        NO_MODULE_ATTACH)
    target_link_libraries("${PROTO_FILE}-json-test" PRIVATE Qt6::Core libtreeland)
    set_target_properties("${PROTO_FILE}-json-test" PROPERTIES AUTOMOC OFF AUTOUIC OFF)
    add_dependencies("${PROTO_FILE}-json-test" "${PROTO_FILE}-gen-client")
    add_test(NAME "${PROTO_FILE}-json-test" COMMAND "${PROTO_FILE}-json-test")
    set_tests_properties("${PROTO_FILE}-json-test" PROPERTIES LABELS "protocol-high;std-wayland" TIMEOUT 30)
endfunction()

# ---- Shared xdg-shell stubs (needed by NEED_SURFACE_FIXTURE) ----
set(XDG_SHELL_XML "/usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml")
set(XDG_SHELL_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/xdg-shell")
set(XDG_SHELL_CLIENT_H "${XDG_SHELL_GEN_DIR}/wayland-xdg-shell-client-protocol.h")
set(XDG_SHELL_CLIENT_C "${XDG_SHELL_GEN_DIR}/wayland-xdg-shell-client-protocol.c")
add_custom_command(
    OUTPUT "${XDG_SHELL_CLIENT_H}" "${XDG_SHELL_CLIENT_C}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${XDG_SHELL_GEN_DIR}"
    COMMAND ${WAYLAND_SCANNER} client-header "${XDG_SHELL_XML}" "${XDG_SHELL_CLIENT_H}"
    COMMAND ${WAYLAND_SCANNER} private-code "${XDG_SHELL_XML}" "${XDG_SHELL_CLIENT_C}"
    DEPENDS "${XDG_SHELL_XML}" VERBATIM
)
set(XDG_SHELL_INCLUDE_DIR "${XDG_SHELL_GEN_DIR}")


# ---- Scan all JSON case files and auto-generate tests ----
file(GLOB JSON_CASES "${CMAKE_CURRENT_SOURCE_DIR}/cases/*.json")
foreach(JSON_PATH ${JSON_CASES})
    get_filename_component(CASE_FILE "${JSON_PATH}" NAME_WE)
    file(READ "${JSON_PATH}" JSON_CONTENT)

    # Must have xml_path field
    string(JSON XML_PATH ERROR_VARIABLE XML_ERR GET "${JSON_CONTENT}" xml_path)
    if(XML_ERR)
        message(WARNING "Skipping ${CASE_FILE}: no xml_path field")
        continue()
    endif()

    # Check for module info (treeland protocol)
    string(JSON MOD_CLASS ERROR_VARIABLE MOD_ERR GET "${JSON_CONTENT}" module class)
    if(MOD_ERR)
        # Standard wayland protocol
        add_wayland_protocol_test("${CASE_FILE}" "${XML_PATH}")
    else()
        string(JSON MOD_HEADER GET "${JSON_CONTENT}" module header)
        string(JSON MOD_DIR GET "${JSON_CONTENT}" module dir)
        add_json_protocol_test("${CASE_FILE}" "${XML_PATH}" "${MOD_HEADER}" "${MOD_CLASS}" "${MOD_DIR}")
    endif()
endforeach()
