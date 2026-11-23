cmake_minimum_required(VERSION 3.10)

project(cDB VERSION 1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_library(cDB STATIC
    ./src/all_cDB_Library.c
)

target_include_directories(cDB PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_executable(cdb_tui src/all_cDB_TUI.c)
set_target_properties(cdb_tui PROPERTIES OUTPUT_NAME "cdb")
target_link_libraries(cdb_tui PRIVATE cDB)

add_executable(example1 examples/example1.c)
target_link_libraries(example1 PRIVATE cDB)

add_executable(example2 examples/example2.c)
target_link_libraries(example2 PRIVATE cDB)

add_executable(example3 examples/example3.c)
target_link_libraries(example3 PRIVATE cDB)

add_executable(datagen examples/datagen.c)
target_link_libraries(datagen PRIVATE cDB)