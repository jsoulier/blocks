cmake_minimum_required(VERSION 3.24)
project(blocks)

set(BINARY_DIR ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE ${BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${BINARY_DIR})

add_subdirectory(${CMAKE_SOURCE_DIR}/lib/SDL)
add_executable(${PROJECT_NAME} WIN32
    ${CMAKE_SOURCE_DIR}/lib/sqlite3/sqlite3.c
    ${CMAKE_SOURCE_DIR}/src/block.c
    ${CMAKE_SOURCE_DIR}/src/camera.c
    ${CMAKE_SOURCE_DIR}/src/containers.c
    ${CMAKE_SOURCE_DIR}/src/database.c
    ${CMAKE_SOURCE_DIR}/src/helpers.c
    ${CMAKE_SOURCE_DIR}/src/main.c
    ${CMAKE_SOURCE_DIR}/src/noise.c
    ${CMAKE_SOURCE_DIR}/src/voxmesh.c
    ${CMAKE_SOURCE_DIR}/src/world.c
)
target_link_libraries(${PROJECT_NAME} PUBLIC SDL3::SDL3)
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_SOURCE_DIR}/lib/sqlite3)
set_target_properties(${PROJECT_NAME} PROPERTIES C_STANDARD 11)

function(shader FILE)
    string(REPLACE . _ NAME ${FILE})
    set(SOURCE ${CMAKE_SOURCE_DIR}/res/${FILE})
    set(OUTPUT ${BINARY_DIR}/${FILE})
    add_custom_target(${NAME} ALL
        COMMAND glslc ${SOURCE} -o ${OUTPUT}
        DEPENDS ${SOURCE} BYPRODUCTS ${OUTPUT} COMMENT ${SOURCE})
endfunction()
shader(sky.frag)
shader(sky.vert)
shader(ui.frag)
shader(ui.vert)
shader(world.frag)
shader(world.vert)

configure_file(${CMAKE_SOURCE_DIR}/res/atlas.bmp ${BINARY_DIR} COPYONLY)