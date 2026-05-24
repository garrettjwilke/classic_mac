# Host-side mkbook tool — builds .pgdata page-index files for hmls-ebook_reader.
#
# This directory is the standalone build entry point. From here:
#
#   cd projects/hmls-ebook_reader/tools
#   cmake -B build
#   cmake --build build
#
# Run:
#
#   ./build/mkbook path/to/book.txt
#   ./build/mkbook -o path/to/book.pgdata path/to/book.txt
#
# mkbook.c lives here; paginate and geneva12 sources are shared from ../mkbook/.
# The Classic Mac reader app is built separately (./build.sh from the repo root).

cmake_minimum_required(VERSION 3.16)
project(mkbook C)

if(CMAKE_SYSTEM_NAME MATCHES Retro68)
    message(STATUS "mkbook: skipping host tool on Retro68 toolchain")
    return()
endif()

set(MKBOOK_COMMON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../mkbook)

add_executable(mkbook
    mkbook.c
    ${MKBOOK_COMMON_DIR}/paginate.c
    ${MKBOOK_COMMON_DIR}/geneva12.c
)

target_include_directories(mkbook PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${MKBOOK_COMMON_DIR}
)

if(MSVC)
    target_compile_options(mkbook PRIVATE /W4)
else()
    target_compile_options(mkbook PRIVATE -Wall -Wextra -pedantic)
endif()

message(STATUS "mkbook: host tool — output will be ${CMAKE_BINARY_DIR}/mkbook")

install(TARGETS mkbook RUNTIME DESTINATION bin)
