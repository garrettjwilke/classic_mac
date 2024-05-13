cmake_minimum_required(VERSION 3.5)
project(hmls-string_test_cpp)
add_application(hmls-string_test_cpp
    hmls-string_test_cpp.cc
    CONSOLE
   )

set_target_properties(hmls-string_test_cpp PROPERTIES COMPILE_OPTIONS -ffunction-sections)
if(CMAKE_SYSTEM_NAME MATCHES Retro68)
    set_target_properties(hmls-string_test_cpp PROPERTIES LINK_FLAGS "-Wl,-gc-sections -Wl,--mac-strip-macsbug")
else()
    set_target_properties(hmls-string_test_cpp PROPERTIES LINK_FLAGS "-Wl,-gc-sections")
endif()
