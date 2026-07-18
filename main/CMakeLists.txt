file(GLOB_RECURSE SOURCES "*.c")

idf_component_register(
    SRCS 
        ${SOURCES}
    INCLUDE_DIRS 
        "."
    PRIV_REQUIRES
        common
        ui_manager
        ble_manager
        event_manager
        state_manager
        runtime_manager
        app_mgr
        nvs_flash
        tick_manager
        power_manager
        gpio_manager
        notification_manager
        storage_manager
)
