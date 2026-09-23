add_executable(runtime_watchdog
    test_runtime_watchdog.c

    ../../../../components/runtime_manager/src/runtime_watchdog.c
    ../../../../components/runtime_manager/src/runtime_utils.c
)

target_include_directories(runtime_watchdog
    PRIVATE
        ../../../../components/runtime_manager/include
        ../../../../components/runtime_manager/private
)

target_link_libraries(runtime_watchdog
    PRIVATE
        host_esp_timer
        freertos_kernel
        freertos_config
        host_platform
)

add_test(
    NAME runtime_watchdog
    COMMAND runtime_watchdog
)

add_executable(runtime_worker_pool
    test_runtime_worker_pool.c

    ../../../../components/runtime_manager/src/runtime_worker_pool.c
    ../../../../components/runtime_manager/src/runtime_watchdog.c
    ../../../../components/runtime_manager/src/runtime_utils.c
)

target_include_directories(runtime_worker_pool
    PRIVATE
        ../../../../components/runtime_manager/include
        ../../../../components/runtime_manager/private
)

target_link_libraries(runtime_worker_pool
    PRIVATE
        host_esp_timer
        freertos_kernel
        freertos_config
        host_platform
)

target_compile_options(runtime_worker_pool
    PRIVATE
        -Wno-format
)

add_test(
    NAME runtime_worker_pool
    COMMAND runtime_worker_pool
)

add_executable(runtime_manager
    test_runtime_manager.c

    ../../../../components/runtime_manager/src/runtime_manager.c
    ../../../../components/runtime_manager/src/runtime_worker_pool.c
    ../../../../components/runtime_manager/src/runtime_watchdog.c
    ../../../../components/runtime_manager/src/runtime_utils.c
)

target_include_directories(runtime_manager
    PRIVATE
        ../../../../components/runtime_manager/include
        ../../../../components/runtime_manager/private
)

target_link_libraries(runtime_manager
    PRIVATE
        tick_manager_host
        event_manager_host
        host_esp_timer
        freertos_kernel
        freertos_config
        host_platform
)

target_compile_options(runtime_manager
    PRIVATE
        -Wno-format
)

add_test(
    NAME runtime_manager
    COMMAND runtime_manager
)
