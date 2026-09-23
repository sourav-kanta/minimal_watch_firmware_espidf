add_executable(test_ble_fifo
    test_ble_fifo.c
)

target_link_libraries(test_ble_fifo
    PRIVATE
        ble_host
        freertos_kernel
        freertos_config
        host_platform
)

add_test(
    NAME ble_fifo
    COMMAND test_ble_fifo
)

add_executable(test_ble_packet_handler
    test_ble_packet_handler.c
)

target_link_libraries(test_ble_packet_handler
    PRIVATE
        ble_host
        freertos_kernel
        freertos_config
        host_platform
)

add_test(
    NAME ble_packet_handler
    COMMAND test_ble_packet_handler
)
