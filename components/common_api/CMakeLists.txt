file(GLOB_RECURSE SOURCES "*.c")

message(STATUS "Compiling files: ${SOURCES}")

idf_component_register(
    SRCS 
        ${SOURCES}
    INCLUDE_DIRS 
        "include"
    PRIV_INCLUDE_DIRS
        "private"
    REQUIRES
        common
        common_app
    PRIV_REQUIRES
        state_manager
        event_manager
        gpio_manager
        watchface_mgr
        lvgl_bridge
)
