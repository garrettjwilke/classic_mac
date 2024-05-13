set(CMAKE_C_FLAGS "-Wno-multichar")
set(CMAKE_EXE_LINKER_FLAGS "-Wl,-gc-sections")
cmake_minimum_required(VERSION 3.5)
project(Launcher)
add_application(Launcher
    FILES launcher.c CONSOLE)
