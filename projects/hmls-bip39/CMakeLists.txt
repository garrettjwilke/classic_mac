cmake_minimum_required(VERSION 3.2)
project(hmls-bip39 C CXX)

find_library(bip39 PUBLIC)

set(SOURCE_FILES hmls-bip39.cpp)

add_application(${PROJECT_NAME} ${SOURCE_FILES} CONSOLE)
target_link_libraries(${PROJECT_NAME} ${CMAKE_SOURCE_DIR}/libbip39.a)
