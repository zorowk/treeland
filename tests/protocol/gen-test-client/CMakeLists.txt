add_executable(gen-test-client
    main.cpp
)
target_link_libraries(gen-test-client PRIVATE PkgConfig::WAYLAND_CLIENT)
