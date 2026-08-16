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
        common_app
    PRIV_REQUIRES
        common
        common_ui
        lvgl
        runtime_manager
        event_manager
        weather_app
        settings_app
        alarm_app
        stopwatch_app
        brickbreaker_game
)
