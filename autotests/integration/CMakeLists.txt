add_library(IntegrationTestFramework STATIC)

if (Qt6_VERSION VERSION_GREATER_EQUAL "6.8.0")
    set(private_code_option  "PRIVATE_CODE")
endif()
qt6_generate_wayland_protocol_client_sources(IntegrationTestFramework
    ${private_code_option}
    NO_INCLUDE_CORE_ONLY
    FILES
        ${WaylandProtocols_DATADIR}/unstable/input-method/input-method-unstable-v1.xml
)

pkg_check_modules(WLR_PROTOCOLS wlr-protocols)
if (WLR_PROTOCOLS_FOUND)
    pkg_get_variable(WLR_PROTOCOLS_DATADIR wlr-protocols pkgdatadir)
endif()

qt6_generate_wayland_protocol_client_sources(IntegrationTestFramework
    ${private_code_option}
    FILES
        ${WLR_PROTOCOLS_DATADIR}/stable/presentation-time/presentation-time.xml
        ${WaylandProtocols_DATADIR}/stable/presentation-time/presentation-time.xml
        ${WaylandProtocols_DATADIR}/stable/tablet/tablet-v2.xml
        ${WaylandProtocols_DATADIR}/stable/xdg-shell/xdg-shell.xml
        ${WaylandProtocols_DATADIR}/staging/color-management/color-management-v1.xml
        ${WaylandProtocols_DATADIR}/staging/cursor-shape/cursor-shape-v1.xml
        ${WaylandProtocols_DATADIR}/staging/fifo/fifo-v1.xml
        ${WaylandProtocols_DATADIR}/staging/fractional-scale/fractional-scale-v1.xml
        ${WaylandProtocols_DATADIR}/staging/security-context/security-context-v1.xml
        ${WaylandProtocols_DATADIR}/staging/xdg-activation/xdg-activation-v1.xml
        ${WaylandProtocols_DATADIR}/staging/xdg-dialog/xdg-dialog-v1.xml
        ${WaylandProtocols_DATADIR}/staging/xdg-toplevel-drag/xdg-toplevel-drag-v1.xml
        ${WaylandProtocols_DATADIR}/unstable/idle-inhibit/idle-inhibit-unstable-v1.xml
        ${WaylandProtocols_DATADIR}/unstable/primary-selection/primary-selection-unstable-v1.xml
        ${WaylandProtocols_DATADIR}/unstable/text-input/text-input-unstable-v3.xml
        ${WaylandProtocols_DATADIR}/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml

        ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-dde-shell-v1.xml
        ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-foreign-toplevel-manager-v1.xml
        ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-output-manager-v1.xml
        ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-personalization-manager-v1.xml
        ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-prelaunch-splash-v1.xml
        ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-window-management-v1.xml
)

target_sources(IntegrationTestFramework PRIVATE
    treeland_wayland_test.cpp
    test_helpers.cpp
)

target_link_libraries(KWinIntegrationTestFramework
    PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::WaylandClient
        Qt6::WaylandClientPrivate
        Qt6::Quick
        Qt6::QuickPrivate
        Qt6::QuickControls2
        Qt6::Test
        Qt6::Concurrent
        libtreeland
)
