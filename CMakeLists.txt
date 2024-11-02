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
add_executable(${PROJECT_NAME} WIN32
    lib/sqlite3/sqlite3.c
    lib/stb/stb.c
    src/block.c
    src/camera.c
    src/containers.c
    src/database.c
    src/helpers.c
    src/main.c
    src/noise.c
    src/physics.c
    src/voxmesh.c
    src/world.c
)
target_link_libraries(blocks PUBLIC SDL3::SDL3)
target_include_directories(blocks PUBLIC lib/sqlite3)
target_include_directories(blocks PUBLIC lib/stb)
set_target_properties(blocks PROPERTIES C_STANDARD 11)

function(shader FILE)
    string(REPLACE . _ NAME ${FILE})
    set(SOURCE shaders/${FILE})
    set(OUTPUT ${BINARY_DIR}/${FILE})
    add_custom_target(${NAME} ALL
        COMMAND glslc ${SOURCE} -o ${OUTPUT} -I src
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        DEPENDS ${SOURCE} res/config.glsl src/config.h
        BYPRODUCTS ${OUTPUT} COMMENT ${SOURCE})
endfunction()
shader(raycast.frag)
shader(raycast.vert)
shader(sky.frag)
shader(sky.vert)
shader(ui.frag)
shader(ui.vert)
shader(world.frag)
shader(world.vert)

configure_file(textures/atlas.png ${BINARY_DIR} COPYONLY)