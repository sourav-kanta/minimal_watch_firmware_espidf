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
        common_ui
    PRIV_REQUIRES
        common
        event_manager
        tick_manager
        storage_manager
        state_manager
)
