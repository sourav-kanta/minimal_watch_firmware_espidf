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
        lvgl
        common_ui
        common_api
        common_app
        watchface_mgr
        gpio_manager
)
