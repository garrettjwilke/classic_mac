cmake_minimum_required(VERSION 3.5)
set(APP_NAME "hmls-wordle")
project(${APP_NAME} C)

set(WORDS_GENERATED ${CMAKE_CURRENT_SOURCE_DIR}/words_generated.c)
set(PATTERNS_GENERATED ${CMAKE_CURRENT_SOURCE_DIR}/patterns_generated.c)
set(LETTERS_GENERATED ${CMAKE_CURRENT_SOURCE_DIR}/letters_generated.c)

set(LETTER_PNGS
    ${CMAKE_CURRENT_SOURCE_DIR}/data/letters/a.png
    ${CMAKE_CURRENT_SOURCE_DIR}/data/letters/b.png
    ${CMAKE_CURRENT_SOURCE_DIR}/data/letters/c.png
    ${CMAKE_CURRENT_SOURCE_DIR}/data/letters/d.png
    ${CMAKE_CURRENT_SOURCE_DIR}/data/letters/e.png
    ${CMAKE_CURRENT_SOURCE_DIR}/data/letters/f.png
    ${CMAKE_CURRENT_SOURCE_DIR}/data/letters/g.png
    ${CMAKE_CURRENT_SOURCE_DIR}/data/letters/h.png
    ${CMAKE_CURRENT_SOURCE_DIR}/data/letters/i.png
    ${CMAKE_CURRENT_SOURCE_DIR}/data/letters/j.png
    ${CMAKE_CURRENT_SOURCE_DIR}/data/letters/k.png
    ${CMAKE_CURRENT_SOURCE_DIR}/data/letters/l.png
    ${CMAKE_CURRENT_SOURCE_DIR}/data/letters/m.png
    ${CMAKE_CURRENT_SOURCE_DIR}/data/letters/n.png
    ${CMAKE_CURRENT_SOURCE_DIR}/data/letters/o.png
    ${CMAKE_CURRENT_SOURCE_DIR}/data/letters/p.png
    ${CMAKE_CURRENT_SOURCE_DIR}/data/letters/q.png
    ${CMAKE_CURRENT_SOURCE_DIR}/data/letters/r.png
    ${CMAKE_CURRENT_SOURCE_DIR}/data/letters/s.png
    ${CMAKE_CURRENT_SOURCE_DIR}/data/letters/t.png
    ${CMAKE_CURRENT_SOURCE_DIR}/data/letters/u.png
    ${CMAKE_CURRENT_SOURCE_DIR}/data/letters/v.png
    ${CMAKE_CURRENT_SOURCE_DIR}/data/letters/w.png
    ${CMAKE_CURRENT_SOURCE_DIR}/data/letters/x.png
    ${CMAKE_CURRENT_SOURCE_DIR}/data/letters/y.png
    ${CMAKE_CURRENT_SOURCE_DIR}/data/letters/z.png
)

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

add_custom_command(
    OUTPUT ${LETTERS_GENERATED}
    COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_letters.py
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_letters.py
        ${LETTER_PNGS}
    COMMENT "Generating letter bitmaps from PNGs"
)

add_custom_target(generate_words DEPENDS ${WORDS_GENERATED})
add_custom_target(generate_patterns DEPENDS ${PATTERNS_GENERATED})
add_custom_target(generate_letters DEPENDS ${LETTERS_GENERATED})

add_application(${APP_NAME}
    ${APP_NAME}.c
    game.c
    words.c
    letters.c
    ${WORDS_GENERATED}
    ${PATTERNS_GENERATED}
    ${LETTERS_GENERATED}
    ${APP_NAME}.r
)

add_dependencies(${APP_NAME} generate_words generate_patterns generate_letters)

set_target_properties(${APP_NAME} PROPERTIES COMPILE_OPTIONS "-m68000;-msoft-float;-ffunction-sections")
if(CMAKE_SYSTEM_NAME MATCHES Retro68)
    set_target_properties(${APP_NAME} PROPERTIES LINK_FLAGS
        "-Wl,-gc-sections -Wl,--mac-strip-macsbug -Wl,--mac-single -Wl,-z,stack-size=65536"
    )
else()
    set_target_properties(${APP_NAME} PROPERTIES LINK_FLAGS "-Wl,-gc-sections")
endif()
