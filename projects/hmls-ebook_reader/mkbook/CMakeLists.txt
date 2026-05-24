cmake_minimum_required(VERSION 3.16)
project(mkbook C)

if(CMAKE_SYSTEM_NAME MATCHES Retro68)
    message(STATUS "mkbook: skipping host tool on Retro68")
    return()
endif()

add_executable(mkbook
    mkbook.c
    paginate.c
    geneva12.c
)

target_include_directories(mkbook PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

if(MSVC)
    target_compile_options(mkbook PRIVATE /W4)
else()
    target_compile_options(mkbook PRIVATE -Wall -Wextra -pedantic)
endif()

install(TARGETS mkbook RUNTIME DESTINATION bin)
