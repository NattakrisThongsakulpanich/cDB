cmake_minimum_required(VERSION 3.10)

project(cDB VERSION 1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_library(cDB STATIC
    src/cDB.c
    src/cDB.h
    src/cDB_Table.c
    src/cDB_Table.h
    src/cDB_SpreadSheet.c
)

target_include_directories(cDB PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)