add_executable(test_freertos
    test_freertos.c
)

target_link_libraries(test_freertos
    PRIVATE
        freertos_kernel
)

add_test(
    NAME freertos_basic
    COMMAND test_freertos
)

add_executable(test_freertos_mutex
    test_freertos_mutex.c
)

target_link_libraries(test_freertos_mutex
    PRIVATE
        freertos_kernel
)

add_test(
    NAME freertos_mutex
    COMMAND test_freertos_mutex
)

add_executable(test_esp_timer
    test_esp_timer.c
)

target_link_libraries(test_esp_timer
    PRIVATE
        host_esp_timer
)

add_test(
    NAME host_esp_timer
    COMMAND test_esp_timer
)

add_executable(test_esp_timer_concurrency
    test_esp_timer_concurrency.c
)

target_link_libraries(test_esp_timer_concurrency
    PRIVATE
        host_esp_timer
)

add_test(
    NAME host_esp_timer_concurrency
    COMMAND test_esp_timer_concurrency
)
