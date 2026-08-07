# Protocol test: treeland-dde-shell-v1
find_package(TreelandProtocols REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(WAYLAND_CLIENT REQUIRED IMPORTED_TARGET wayland-client)

set(PROTOCOL_XML "${TREELAND_PROTOCOLS_DATA_DIR}/treeland-dde-shell-v1.xml")
set(CLIENT_HEADER "${CMAKE_CURRENT_BINARY_DIR}/treeland-dde-shell-client-protocol.h")
set(CLIENT_CODE   "${CMAKE_CURRENT_BINARY_DIR}/treeland-dde-shell-client-protocol.c")

add_custom_command(
    OUTPUT ${CLIENT_HEADER}
    COMMAND wayland-scanner client-header ${PROTOCOL_XML} ${CLIENT_HEADER}
    DEPENDS ${PROTOCOL_XML}
)
add_custom_command(
    OUTPUT ${CLIENT_CODE}
    COMMAND wayland-scanner private-code ${PROTOCOL_XML} ${CLIENT_CODE}
    DEPENDS ${PROTOCOL_XML}
)

add_executable(test_treeland_dde_shell_v1
    main.cpp
    treeland-dde-shell-v1.c
    ${CLIENT_HEADER}
    ${CLIENT_CODE}
)

target_include_directories(test_treeland_dde_shell_v1 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/waylib/src/server
    ${CMAKE_SOURCE_DIR}/waylib/src/server/kernel
    ${CMAKE_SOURCE_DIR}/waylib/src/server/utils
)

target_link_libraries(test_treeland_dde_shell_v1 PRIVATE
    libtreeland
    PkgConfig::WAYLAND_CLIENT
    Qt6::Core
    Qt6::Gui
)

set_target_properties(test_treeland_dde_shell_v1 PROPERTIES C_STANDARD 11)

add_test(NAME test_treeland_dde_shell_v1 COMMAND test_treeland_dde_shell_v1)
set_tests_properties(test_treeland_dde_shell_v1 PROPERTIES
    ENVIRONMENT "WLR_BACKENDS=headless"
    TIMEOUT 30
)
