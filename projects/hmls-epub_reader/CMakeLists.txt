cmake_minimum_required(VERSION 3.5)
set(APP_NAME "hmls-epub_reader")
project(${APP_NAME} C)

add_application(${APP_NAME}
    ${APP_NAME}.c
    book_index.c
    reader_state.c
    epub_import.c
    epub_util.c
    epub_log.c
    zip_lite.c
    puff.c
    xhtml_lite.c
    mac_fs.c
    tools/geneva12.c
    tools/paginate.c
    ${APP_NAME}.r
)

target_include_directories(${APP_NAME} PRIVATE . tools)

set_target_properties(${APP_NAME} PROPERTIES COMPILE_OPTIONS "-m68000;-msoft-float;-ffunction-sections")
if(CMAKE_SYSTEM_NAME MATCHES Retro68)
    set_target_properties(${APP_NAME} PROPERTIES LINK_FLAGS
        "-Wl,-gc-sections -Wl,--mac-strip-macsbug -Wl,--mac-single -Wl,-z,stack-size=65536"
    )
else()
    set_target_properties(${APP_NAME} PROPERTIES LINK_FLAGS "-Wl,-gc-sections")
endif()
