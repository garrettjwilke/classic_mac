# To use this example as a standalone project using CMake:
# mkdir build
# cd build
# cmake .. -DCMAKE_TOOLCHAIN_FILE=path/to/Retro68-build/toolchain/m68k-apple-macos/cmake/retro68.toolchain.cmake
# make
cmake_minimum_required(VERSION 3.5)
project(gui_test)
add_application(gui_test
    gui_test.c
    gui_test.r
    )

# Enable -ffunction-sections and -gc-sections to make the app as small as possible
# On 68K, also enable --mac-single to build it as a single-segment app (so that this code path doesn't rot)
set_target_properties(gui_test PROPERTIES COMPILE_OPTIONS -ffunction-sections)
if(CMAKE_SYSTEM_NAME MATCHES Retro68)
    set_target_properties(gui_test PROPERTIES LINK_FLAGS "-Wl,-gc-sections -Wl,--mac-single")

else()
    set_target_properties(gui_test PROPERTIES LINK_FLAGS "-Wl,-gc-sections")
endif()
