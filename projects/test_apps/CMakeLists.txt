#   Copyright 2014 Wolfgang Thaller.
#
#   This file is part of Retro68.
#
#   Retro68 is free software: you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation, either version 3 of the License, or
#   (at your option) any later version.
#
#   Retro68 is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with Retro68.  If not, see <http://www.gnu.org/licenses/>.
cmake_minimum_required(VERSION 3.5)
project(test_apps)
add_application(EmptyTest EmptyTest.c)

add_application(ExceptionTest CONSOLE ExceptionTest.cc)
add_application(InitTest CONSOLE InitTest.cc)

if(CMAKE_SYSTEM_NAME MATCHES Retro68)
enable_language(ASM)
add_application(AsmTest AsmTest.s)
set_target_properties(AsmTest PROPERTIES LINKER_LANGUAGE C)
endif()
