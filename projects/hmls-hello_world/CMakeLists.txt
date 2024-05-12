add_application(hmls-hello_world
    hmls-hello_world.c
    CONSOLE
   )

set_target_properties(hmls-hello_world PROPERTIES COMPILE_OPTIONS -ffunction-sections)
if(CMAKE_SYSTEM_NAME MATCHES Retro68)
    set_target_properties(hmls-hello_world PROPERTIES LINK_FLAGS "-Wl,-gc-sections -Wl,--mac-strip-macsbug")
else()
    set_target_properties(hmls-hello_world PROPERTIES LINK_FLAGS "-Wl,-gc-sections")
endif()
