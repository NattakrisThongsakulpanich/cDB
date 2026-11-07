cmake_minimum_required(VERSION 3.10)

set(CMAKE_C_COMPILER clang)

project(cDB_Library VERSION 1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_library(cDB_Library STATIC
    src/cDB.c
    src/cDB.h
    src/cDB_Table.c
    src/cDB_Table.h
    src/safefOC.c
    src/safefOC.h
)

target_include_directories(cDB_Library PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)