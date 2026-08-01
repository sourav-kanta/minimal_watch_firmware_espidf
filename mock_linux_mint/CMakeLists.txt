idf_component_register(
    SRCS "mock_layer.c"
    INCLUDE_DIRS "."
    REQUIRES esp_lcd encoder_driver ble_manager runtime_manager
)

target_include_directories(
    ${COMPONENT_LIB}
    PRIVATE
    ${CMAKE_SOURCE_DIR}/components/ble_manager/private
)

target_link_libraries(${COMPONENT_LIB} INTERFACE 
    "-Wl,--wrap=ble_manager_init"
    "-Wl,--wrap=init_display"
    "-Wl,--wrap=display_draw_bitmap"  
    "-Wl,--wrap=display_on"
    "-Wl,--wrap=display_off"
    "-Wl,--wrap=led_strip_new_rmt_device"
    "-Wl,--wrap=gpio_sleep_sel_en"
    "-Wl,--wrap=esp_sleep_enable_gpio_wakeup"
    "-Wl,--wrap=encoder_init"
    "-Wl,--wrap=encoder_deinit"
)
