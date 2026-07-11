# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.22)

if(DEFINED EXTRA_COMPONENT_DIRS MATCHES "mock_ble")
    message(STATUS "[QEMU BUILD] Injecting global linker wraps for simulation...")
    add_compile_options("-Ddisplay_draw_bitmap=__real_display_draw_bitmap")
    add_link_options(
        "-Wl,--wrap=ble_manager_init"
        "-Wl,--wrap=init_display"
        "-Wl,--defsym,panel_handle=qemu_mock_panel_handle"
        "-Wl,--wrap=display_draw_bitmap"  # <-- Our working wrap method
        "-Wl,--wrap=display_on"
        "-Wl,--wrap=display_off"
    )
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(minimal_watch_firmware)
