cmake_minimum_required(VERSION 3.5)
set(APP_NAME "hmls-minesweeper")
project(${APP_NAME} C)

set(TILE_PNGS
    ${CMAKE_CURRENT_SOURCE_DIR}/tiles/covered.png
    ${CMAKE_CURRENT_SOURCE_DIR}/tiles/flag.png
    ${CMAKE_CURRENT_SOURCE_DIR}/tiles/mine.png
    ${CMAKE_CURRENT_SOURCE_DIR}/tiles/mine_hit.png
    ${CMAKE_CURRENT_SOURCE_DIR}/tiles/empty.png
    ${CMAKE_CURRENT_SOURCE_DIR}/tiles/1.png
    ${CMAKE_CURRENT_SOURCE_DIR}/tiles/2.png
    ${CMAKE_CURRENT_SOURCE_DIR}/tiles/3.png
    ${CMAKE_CURRENT_SOURCE_DIR}/tiles/4.png
    ${CMAKE_CURRENT_SOURCE_DIR}/tiles/5.png
    ${CMAKE_CURRENT_SOURCE_DIR}/tiles/6.png
    ${CMAKE_CURRENT_SOURCE_DIR}/tiles/7.png
    ${CMAKE_CURRENT_SOURCE_DIR}/tiles/8.png
    ${CMAKE_CURRENT_SOURCE_DIR}/tiles/face_normal.png
    ${CMAKE_CURRENT_SOURCE_DIR}/tiles/face_surprised.png
    ${CMAKE_CURRENT_SOURCE_DIR}/tiles/face_dead.png
    ${CMAKE_CURRENT_SOURCE_DIR}/tiles/face_cool.png
)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/tiles_generated.r
    COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_tiles.py
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_tiles.py
        ${TILE_PNGS}
    COMMENT "Generating tile resources from PNGs"
)

add_custom_target(generate_tiles
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tiles_generated.r
)

add_application(${APP_NAME}
    ${APP_NAME}.c
    game.c
    tiles.c
    ${APP_NAME}.r
)

add_dependencies(${APP_NAME} generate_tiles)

set_target_properties(${APP_NAME} PROPERTIES COMPILE_OPTIONS "-m68000;-msoft-float;-ffunction-sections")
if(CMAKE_SYSTEM_NAME MATCHES Retro68)
    set_target_properties(${APP_NAME} PROPERTIES LINK_FLAGS
        "-Wl,-gc-sections -Wl,--mac-strip-macsbug -Wl,--mac-single -Wl,-z,stack-size=65536"
    )
else()
    set_target_properties(${APP_NAME} PROPERTIES LINK_FLAGS "-Wl,-gc-sections")
endif()
