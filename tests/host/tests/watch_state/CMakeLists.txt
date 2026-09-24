add_executable(alarm_manager
    test_alarm_manager.c
    ../../../../components/state_manager/src/alarm_manager.c
)

target_include_directories(alarm_manager
    PRIVATE
        ../../../../components/state_manager/include
        ../../../../components/state_manager/private/
)

target_link_libraries(alarm_manager
    PRIVATE
        host_esp_timer
        freertos_kernel
        freertos_config
        host_platform
        common_host
        event_manager_host
)

add_test(
    NAME alarm_manager
    COMMAND alarm_manager
)
