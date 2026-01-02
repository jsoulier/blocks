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
    src/camera.c
    src/chunk.c
    src/database.c
    src/helpers.c
    src/main.c
    src/noise.c
    src/pipeline.c
    src/raycast.c
    src/voxel.c
    src/world.c
)
target_link_libraries(blocks PUBLIC SDL3::SDL3)
target_include_directories(blocks PUBLIC lib/sqlite3)
target_include_directories(blocks PUBLIC lib/stb)
set_target_properties(blocks PROPERTIES C_STANDARD 11)

find_program(GLSLC glslc)
find_program(SHADERCROSS shadercross)
function(shader FILE)
    set(GLSL ${CMAKE_SOURCE_DIR}/shaders/${FILE})
    set(SPV ${CMAKE_SOURCE_DIR}/shaders/bin/${FILE}.spv)
    set(DXIL ${CMAKE_SOURCE_DIR}/shaders/bin/${FILE}.dxil)
    set(MSL ${CMAKE_SOURCE_DIR}/shaders/bin/${FILE}.msl)
    set(JSON ${CMAKE_SOURCE_DIR}/shaders/bin/${FILE}.json)
    function(compile PROGRAM SOURCE OUTPUT)
        add_custom_command(
            OUTPUT ${OUTPUT}
            COMMAND ${PROGRAM} ${SOURCE} -o ${OUTPUT} -I src
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            DEPENDS ${SOURCE} shaders/helpers.glsl src/config.h
            COMMENT "${SOURCE} -> ${OUTPUT}"
        )
        get_filename_component(NAME ${OUTPUT} NAME)
        string(REPLACE . _ NAME ${NAME})
        set(NAME compile_${NAME})
        add_custom_target(${NAME} DEPENDS ${OUTPUT})
        add_dependencies(blocks ${NAME})
    endfunction()
    if(EXISTS ${GLSLC} AND EXISTS ${SHADERCROSS})
        compile(${GLSLC} ${GLSL} ${SPV})
        compile(${SHADERCROSS} ${SPV} ${MSL})
        compile(${SHADERCROSS} ${SPV} ${JSON})
    else()
        message("Skipping build since dependencies are missing")
    endif()
    function(package OUTPUT)
        get_filename_component(NAME ${OUTPUT} NAME)
        set(BINARY ${BINARY_DIR}/${NAME})
        add_custom_command(
            OUTPUT ${BINARY}
            COMMAND ${CMAKE_COMMAND} -E copy ${OUTPUT} ${BINARY}
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            DEPENDS ${OUTPUT}
            COMMENT "${OUTPUT} -> ${BINARY}"
        )
        string(REPLACE . _ NAME ${NAME})
        set(NAME package_${NAME})
        add_custom_target(${NAME} DEPENDS ${BINARY})
        add_dependencies(blocks ${NAME})
    endfunction()
    if(NOT APPLE)
        package(${SPV})
    else()
        package(${MSL})
    endif()
endfunction()
shader(composite.frag)
shader(fullscreen.vert)
shader(opaque.frag)
shader(opaque.vert)
shader(random.frag)
shader(raycast.frag)
shader(raycast.vert)
shader(shadow.frag)
shader(shadow.vert)
shader(sky.frag)
shader(sky.vert)
shader(ssao.frag)
shader(transparent.frag)
shader(transparent.vert)
shader(ui.frag)

configure_file(LICENSE.txt ${BINARY_DIR} COPYONLY)
configure_file(README.md ${BINARY_DIR} COPYONLY)
configure_file(textures/atlas.png ${BINARY_DIR} COPYONLY)