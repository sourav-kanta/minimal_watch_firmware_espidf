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
        wf_abstract_dark
        wf_analog
        wf_retro
        common_api
        event_manager
        tick_manager
        storage_manager
)
