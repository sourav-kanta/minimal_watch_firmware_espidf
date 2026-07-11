FILE(GLOB app_sources *.c)
message(STATUS app_sources : ${app_sources})
target_sources(app PRIVATE ${app_sources})

