set(APP_NAME "hmls-maths")
project(${APP_NAME})
add_application(${APP_NAME}
    ${APP_NAME}.c
    CONSOLE
    )

set_target_properties(${APP_NAME} PROPERTIES COMPILE_OPTIONS -ffunction-sections)
if(CMAKE_SYSTEM_NAME MATCHES Retro68)
    set_target_properties(${APP_NAME} PROPERTIES LINK_FLAGS "-Wl,-gc-sections -Wl,--mac-strip-macsbug")
else()
    set_target_properties(${APP_NAME} PROPERTIES LINK_FLAGS "-Wl,-gc-sections")
endif()
