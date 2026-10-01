add_executable(tick_manager
    test_tick_manager.c
)

target_link_libraries(tick_manager
    PRIVATE
        tick_manager_host
        event_manager_host
        host_esp_timer
        freertos_kernel
        freertos_config
        host_platform
)

target_compile_options(tick_manager
    PRIVATE
        -Wno-format
)

add_test(
    NAME tick_manager
    COMMAND tick_manager
)
