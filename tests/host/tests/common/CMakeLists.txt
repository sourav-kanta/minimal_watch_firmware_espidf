add_executable(test_ring_buffer
    test_ring_buffer.c
)

target_link_libraries(test_ring_buffer
    PRIVATE
        common_host
)

add_test(
    NAME common_ring_buffer
    COMMAND test_ring_buffer
)


add_executable(test_ring_buffer_concurrency
    test_ring_buffer_concurrency.c
)

target_link_libraries(test_ring_buffer_concurrency
    PRIVATE
        common_host
)

add_test(
    NAME common_ring_buffer_concurrency
    COMMAND test_ring_buffer_concurrency
)
