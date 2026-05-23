cmake_minimum_required(VERSION 3.5)
set(APP_NAME "hmls-epub_converter")
project(${APP_NAME})

add_application(${APP_NAME}
    hmls-epub_converter.c
    xhtml_lite.c
    zip_lite.c
    puff.c
    hmls-epub_converter.r
    CONSOLE
)

target_include_directories(${APP_NAME} PRIVATE .)

set_target_properties(${APP_NAME} PROPERTIES COMPILE_OPTIONS -ffunction-sections)
if(CMAKE_SYSTEM_NAME MATCHES Retro68)
    set_target_properties(${APP_NAME} PROPERTIES LINK_FLAGS "-Wl,-gc-sections -Wl,--mac-strip-macsbug")
else()
    set_target_properties(${APP_NAME} PROPERTIES LINK_FLAGS "-Wl,-gc-sections")
endif()
