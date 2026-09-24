add_executable(motion_detection
    test_motion_detector.c

    ../../../../components/motion_stage1_engine/src/motion_detection_engine_stage1.c
    ../../../../components/motion_stage1_engine/src/utils.c
)

target_include_directories(motion_detection
    PRIVATE
        ../../../../components/motion_stage1_engine/private
        ../../../../components/motion_stage1_engine/include
)

target_link_libraries(motion_detection
    PRIVATE
        freertos_kernel
        freertos_config
        host_platform
)

target_compile_definitions(motion_detection PRIVATE
    INPUT_FILE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/stage1_motion_test_vectors.txt"
)

add_test(
    NAME motion_detection
    COMMAND motion_detection
)

