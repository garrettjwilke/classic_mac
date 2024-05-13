cmake_minimum_required(VERSION 3.5)
project(hmls-string_test)
add_application(hmls-string_test
    hmls-string_test.c
    CONSOLE
   )

set_target_properties(hmls-string_test PROPERTIES COMPILE_OPTIONS -ffunction-sections)
if(CMAKE_SYSTEM_NAME MATCHES Retro68)
    set_target_properties(hmls-string_test PROPERTIES LINK_FLAGS "-Wl,-gc-sections -Wl,--mac-strip-macsbug")
else()
    set_target_properties(hmls-string_test PROPERTIES LINK_FLAGS "-Wl,-gc-sections")
endif()
