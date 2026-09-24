cmake_minimum_required(VERSION 3.20)

project(minimal_watch_host_tests LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# Coverage
# ---------------------------------------------------------------------------

option(HOST_COVERAGE "Enable GCC coverage instrumentation" OFF)

if(HOST_COVERAGE)
    add_compile_options(
        --coverage
        -fprofile-update=atomic
    )

    add_link_options(
        --coverage
    )
endif()

include(CTest)

# ---------------------------------------------------------------------------
# FreeRTOS
# ---------------------------------------------------------------------------

set(FREERTOS_PATH
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/FreeRTOS-Kernel
)

set(FREERTOS_HEAP "4" CACHE STRING "" FORCE)
set(FREERTOS_PORT "GCC_POSIX" CACHE STRING "" FORCE)

add_library(freertos_config INTERFACE)

target_include_directories(freertos_config
    SYSTEM INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/config
)

target_compile_definitions(freertos_config
    INTERFACE
        projCOVERAGE_TEST=0
)

add_subdirectory(
    ${FREERTOS_PATH}
    freertos_kernel
)

# ---------------------------------------------------------------------------
# Host platform compatibility
# ---------------------------------------------------------------------------

add_library(host_platform INTERFACE)

target_include_directories(host_platform
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/mocks
)

add_library(host_esp_timer STATIC
    mocks/esp_timer/esp_timer.c
)

target_include_directories(host_esp_timer
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/mocks
)

target_link_libraries(host_esp_timer
    PUBLIC
        freertos_kernel
        freertos_config
        host_platform
)

# ---------------------------------------------------------------------------
# Production components
# ---------------------------------------------------------------------------

add_library(common_host STATIC
    ../../components/common/src/ring_buffer.c
)

target_include_directories(common_host
    PUBLIC
        ../../components/common/include
)

target_link_libraries(common_host
    PUBLIC
        freertos_kernel
        freertos_config
        host_platform
)

add_library(ble_host STATIC
    ../../components/ble_manager/src/ble_fifo.c
    ../../components/ble_manager/src/ble_packet_handler.c
)

target_include_directories(ble_host
    PUBLIC
        ../../components/ble_manager/private
)

target_link_libraries(ble_host
    PRIVATE
        common_host
        freertos_kernel
        freertos_config
        host_platform
)

add_library(event_manager_host STATIC
    ../../components/event_manager/src/event_manager.c
)

target_include_directories(event_manager_host
    PUBLIC
        ../../components/event_manager/include
        ../../components/event_manager/private
        ../../components/common/include
)

target_link_libraries(event_manager_host
    PUBLIC
        freertos_kernel
        freertos_config
        host_platform
)

add_library(tick_manager_host STATIC
    ../../components/tick_manager/src/tick_manager.c
)

target_include_directories(tick_manager_host
    PUBLIC
        ../../components/tick_manager/include
        ../../components/tick_manager/private
)

target_link_libraries(tick_manager_host
    PUBLIC
        event_manager_host
        host_esp_timer
        freertos_kernel
        freertos_config
        host_platform
)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------

enable_testing()

add_subdirectory(tests/host-impl)
add_subdirectory(tests/common)
add_subdirectory(tests/ble)
add_subdirectory(tests/runtime)
add_subdirectory(tests/watch_state)
add_subdirectory(tests/motion_detector)
