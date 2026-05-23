cmake_minimum_required(VERSION 3.5)
set(APP_NAME "hmls-epub_converter")
project(${APP_NAME} C)

add_application(${APP_NAME}
    hmls-epub_converter.c
    epub_log.c
    mac_fs.c
    xhtml_lite.c
    zip_lite.c
    puff.c
    hmls-epub_converter.r
)

target_include_directories(${APP_NAME} PRIVATE .)

set_target_properties(${APP_NAME} PROPERTIES COMPILE_OPTIONS -ffunction-sections)
if(CMAKE_SYSTEM_NAME MATCHES Retro68)
    # Single-segment: MultiFinder / Mini vMac choke on Retro68 multiseg trampolines at launch.
    set_target_properties(${APP_NAME} PROPERTIES LINK_FLAGS
        "-Wl,-gc-sections -Wl,--mac-strip-macsbug -Wl,--mac-single -Wl,-z,stack-size=65536"
    )
else()
    set_target_properties(${APP_NAME} PROPERTIES LINK_FLAGS "-Wl,-gc-sections")
endif()
