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
        common_ui
        wf_abstract_dark
        wf_analog
        wf_retro
        event_manager
        tick_manager
        storage_manager
        state_manager
)
