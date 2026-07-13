cmake_minimum_required(VERSION 3.5)
set(APP_NAME "hmls-wordle")
project(${APP_NAME} C)

set(WORDS_GENERATED ${CMAKE_CURRENT_SOURCE_DIR}/words_generated.c)
set(PATTERNS_GENERATED ${CMAKE_CURRENT_SOURCE_DIR}/patterns_generated.c)

add_custom_command(
    OUTPUT ${WORDS_GENERATED}
    COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_words.py
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_words.py
        ${CMAKE_CURRENT_SOURCE_DIR}/data/targetWords.json
        ${CMAKE_CURRENT_SOURCE_DIR}/data/dictionary.json
    COMMENT "Generating word tables from JSON"
)

add_custom_command(
    OUTPUT ${PATTERNS_GENERATED}
    COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_patterns.py
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_patterns.py
        ${CMAKE_CURRENT_SOURCE_DIR}/data/missrows.png
        ${CMAKE_CURRENT_SOURCE_DIR}/data/wrongplacerows.png
        ${CMAKE_CURRENT_SOURCE_DIR}/data/correctrows.png
        ${CMAKE_CURRENT_SOURCE_DIR}/data/empty.png
    COMMENT "Generating tile patterns from PNGs"
)

add_custom_target(generate_words DEPENDS ${WORDS_GENERATED})
add_custom_target(generate_patterns DEPENDS ${PATTERNS_GENERATED})

add_application(${APP_NAME}
    ${APP_NAME}.c
    game.c
    words.c
    ${WORDS_GENERATED}
    ${PATTERNS_GENERATED}
    ${APP_NAME}.r
)

add_dependencies(${APP_NAME} generate_words generate_patterns)

set_target_properties(${APP_NAME} PROPERTIES COMPILE_OPTIONS "-m68000;-msoft-float;-ffunction-sections")
if(CMAKE_SYSTEM_NAME MATCHES Retro68)
    set_target_properties(${APP_NAME} PROPERTIES LINK_FLAGS
        "-Wl,-gc-sections -Wl,--mac-strip-macsbug -Wl,--mac-single -Wl,-z,stack-size=65536"
    )
else()
    set_target_properties(${APP_NAME} PROPERTIES LINK_FLAGS "-Wl,-gc-sections")
endif()
