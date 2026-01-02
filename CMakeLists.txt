cmake_minimum_required(VERSION 3.24)
project(blocks)

set(BINARY_DIR ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE ${BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${BINARY_DIR})
make_directory(${BINARY_DIR})

add_subdirectory(lib/SDL)

add_executable(blocks WIN32
    lib/sqlite3/sqlite3.c
    lib/stb/stb.c
    src/block.c
    src/buffer.c
    src/camera.c
    src/chunk.c
    src/main.c
    src/map.c
    src/world.c
)

target_link_libraries(blocks PRIVATE
    SDL3::SDL3
)

set_target_properties(blocks PROPERTIES C_STANDARD 11)