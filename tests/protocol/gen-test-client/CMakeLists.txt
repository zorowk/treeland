add_executable(gen-test-client
    main.cpp
)
target_link_libraries(gen-test-client PRIVATE PkgConfig::WAYLAND_CLIENT)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/template.c.in ${CMAKE_CURRENT_BINARY_DIR}/template.c.in COPYONLY)
