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
        lvgl
    PRIV_REQUIRES
        common
        common_ui
        tick_manager
        wakelock_manager
)
