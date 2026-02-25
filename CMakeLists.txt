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

configure_file(LICENSE.txt ${BINARY_DIR} COPYONLY)
configure_file(README.md ${BINARY_DIR} COPYONLY)
configure_file(textures/atlas.png ${BINARY_DIR} COPYONLY)

add_subdirectory(lib/SDL)
add_executable(blocks WIN32
    lib/sqlite3/sqlite3.c
    lib/stb/stb.c
    src/block.c
    src/buffer.c
    src/camera.c
    src/main.c
    src/map.c
    src/physics.c
    src/player.c
    src/rand.c
    src/save.c
    src/shader.c
    src/voxel.c
    src/world.c
)
set_target_properties(blocks PROPERTIES C_STANDARD 11)
target_include_directories(blocks PUBLIC lib/jsmn)
target_include_directories(blocks PUBLIC lib/sqlite3)
target_include_directories(blocks PUBLIC lib/stb)
target_link_libraries(blocks PRIVATE SDL3::SDL3)

find_program(SHADERCROSS shadercross)
function(add_shader FILE)
    set(DEPENDS ${ARGN})
    set(HLSL ${CMAKE_SOURCE_DIR}/shaders/${FILE})
    set(SPV ${CMAKE_SOURCE_DIR}/shaders/bin/${FILE}.spv)
    set(MSL ${CMAKE_SOURCE_DIR}/shaders/bin/${FILE}.msl)
    set(JSON ${CMAKE_SOURCE_DIR}/shaders/bin/${FILE}.json)
    function(compile OUTPUT)
        add_custom_command(
            OUTPUT ${OUTPUT}
            COMMAND ${SHADERCROSS} ${HLSL} -s hlsl -o ${OUTPUT} -I src
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            DEPENDS ${HLSL} ${DEPENDS}
            COMMENT ${OUTPUT}
        )
        get_filename_component(NAME ${OUTPUT} NAME)
        string(REPLACE . _ NAME ${NAME})
        set(NAME compile_${NAME})
        add_custom_target(${NAME} DEPENDS ${OUTPUT})
        add_dependencies(blocks ${NAME})
    endfunction()
    if(EXISTS ${SHADERCROSS})
        compile(${SPV})
        compile(${MSL})
        compile(${JSON})
    else()
        message("Using prebuilts since SDL_shadercross is missing")
    endif()
    function(package OUTPUT)
        get_filename_component(NAME ${OUTPUT} NAME)
        set(BINARY ${BINARY_DIR}/${NAME})
        add_custom_command(
            OUTPUT ${BINARY}
            COMMAND ${CMAKE_COMMAND} -E copy ${OUTPUT} ${BINARY}
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            DEPENDS ${OUTPUT}
            COMMENT ${BINARY}
        )
        string(REPLACE . _ NAME ${NAME})
        set(NAME package_${NAME})
        add_custom_target(${NAME} DEPENDS ${BINARY})
        add_dependencies(blocks ${NAME})
    endfunction()
    if(APPLE)
        package(${MSL})
    else()
        package(${SPV})
    endif()
    package(${JSON})
endfunction()
add_shader(blur.comp shaders/shader.hlsl src/voxel.inc)
add_shader(composite.comp shaders/shader.hlsl src/voxel.inc)
add_shader(depth.frag shaders/shader.hlsl src/voxel.inc)
add_shader(depth.vert shaders/shader.hlsl src/voxel.inc)
add_shader(opaque.frag shaders/shader.hlsl src/voxel.inc)
add_shader(opaque.vert shaders/shader.hlsl src/voxel.inc)
add_shader(raycast.frag shaders/shader.hlsl src/voxel.inc)
add_shader(raycast.vert shaders/shader.hlsl src/voxel.inc)
add_shader(shadow.frag shaders/shader.hlsl src/voxel.inc)
add_shader(shadow.vert shaders/shader.hlsl src/voxel.inc)
add_shader(sky.frag shaders/shader.hlsl src/voxel.inc)
add_shader(sky.vert shaders/shader.hlsl src/voxel.inc)
add_shader(ssao.comp shaders/shader.hlsl src/voxel.inc)
add_shader(transparent.frag shaders/shader.hlsl src/voxel.inc)
add_shader(transparent.vert shaders/shader.hlsl src/voxel.inc)
add_shader(ui.comp shaders/shader.hlsl src/voxel.inc)
