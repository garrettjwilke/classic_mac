#    Copyright 2015 Wolfgang Thaller.
#
#    This file is part of Retro68.
#
#    Retro68 is free software: you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation, either version 3 of the License, or
#    (at your option) any later version.
#
#    Retro68 is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with Retro68.  If not, see <http://www.gnu.org/licenses/>.

# To use this example as a standalone project using CMake:
# mkdir build
# cd build
# cmake .. -DCMAKE_TOOLCHAIN_FILE=path/to/Retro68-build/toolchain/cmake/retro68.toolchain.cmake
# make

# This sample program contains a custom window definition procedure (window_types) defined in window_types.c.
# It is used in two different ways:
# 1. The 80s way: compiled as a separate 'window_types' code resource.
# 2. The 90s way: compiled as part of the application.
cmake_minimum_required(VERSION 3.5)
project(window_types)
# First, let's build a separate code resource:
add_executable(window_types window_types.c)


if(TARGET retrocrt)
    # Hack: if we are building as part of the Retro68 source tree,
    #       make sure the run-time library is already compiled
    # (not needed for standalone projects)
    add_dependencies(window_types retrocrt)
endif(TARGET retrocrt)


if(CMAKE_SYSTEM_NAME MATCHES Retro68)
    set_target_properties(window_types PROPERTIES
            # tell window_types.c that it is being compiled as a code resource
        COMPILE_DEFINITIONS "COMPILING_AS_CODE_RESOURCE"
        
        COMPILE_OPTIONS -ffunction-sections # make things smaller

            # set a linker flag that says we want a flat piece
            # of code in a data file, specify entry point,
            # and add -Wl,-gc-sections to make things smaller.
        LINK_FLAGS "-Wl,--mac-flat -Wl,-eMYWINDOWDEFPROC -Wl,-gc-sections")
    set(window_types_R window_types.r)
    set(window_types_RESOURCE window_types)
elseif(CMAKE_SYSTEM_NAME MATCHES RetroPPC)
    set_target_properties(window_types PROPERTIES
        COMPILE_OPTIONS -ffunction-sections # make things smaller
        # tell window_types.c that it is being compiled as a code resource
        COMPILE_DEFINITIONS "COMPILING_AS_CODE_RESOURCE"
        LINK_FLAGS " -Wl,-eMyWindowindow_typesProc -Wl,-gc-sections"
        )
    set(window_types_R window_typesppc.r)

    add_custom_command(
        OUTPUT window_types.pef
        COMMAND ${MAKE_PEF} window_types -o "window_types.pef"
        DEPENDS window_types)
    set(window_types_RESOURCE window_types.pef)
endif()

if (REZ_INCLUDE_PATH)
    set(REZ_INCLUDE_FLAG -I${REZ_INCLUDE_PATH})
endif()

    # wrap the compiled window_types into a resource
add_custom_command(
    OUTPUT window_types.rsrc.bin
    COMMAND ${REZ} ${REZ_INCLUDE_FLAG}
                    ${CMAKE_CURRENT_SOURCE_DIR}/${window_types_R}
                    -o window_types.rsrc.bin
                    
    DEPENDS ${window_types_RESOURCE} ${window_types_R})
    
# Now build the application
add_application(window_typesShell
    window_typesshell.c
    window_typesshell.r

    window_types.c  # the window_types as a plain source file in the application

        # the separately compiled window_types resource
    ${CMAKE_CURRENT_BINARY_DIR}/window_types.rsrc.bin   
    )

# Again, add some options to make things smaller.
set_target_properties(window_typesShell PROPERTIES COMPILE_OPTIONS -ffunction-sections)
set_target_properties(window_typesShell PROPERTIES LINK_FLAGS "-Wl,-gc-sections")
