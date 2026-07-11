file(GLOB_RECURSE SOURCES "*.c")

message(STATUS "Compiling files: ${SOURCES}")

idf_component_register(
    SRCS 
        ${SOURCES}
    INCLUDE_DIRS 
        "include"
    PRIV_INCLUDE_DIRS
        "private"
    PRIV_REQUIRES
        common
        event_manager
        esp_timer
        gpio_manager
)

target_compile_options(${COMPONENT_LIB} PRIVATE -fstack-usage)
