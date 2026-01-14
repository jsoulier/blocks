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
    src/noise.c
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

function(add_shader FILE)
    set(DEPENDS ${ARGN})
    set(HLSL ${CMAKE_SOURCE_DIR}/shaders/${FILE})
    set(SPV ${CMAKE_SOURCE_DIR}/shaders/bin/${FILE}.spv)
    set(MSL ${CMAKE_SOURCE_DIR}/shaders/bin/${FILE}.msl)
    set(JSON ${CMAKE_SOURCE_DIR}/shaders/bin/${FILE}.json)
    if(MSVC)
        set(SHADERCROSS lib/SDL_shadercross/msvc/shadercross.exe)
    endif()
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
    if (MSVC)
        compile(${SPV})
        compile(${MSL})
        compile(${JSON})
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
add_shader(blur.comp)
add_shader(chunk.vert shaders/voxel.hlsl src/voxel.inc)
add_shader(composite.comp shaders/voxel.hlsl src/voxel.inc shaders/shadow.hlsl)
add_shader(empty.frag)
add_shader(fullscreen.vert)
add_shader(fxaa.comp)
add_shader(opaque.frag shaders/voxel.hlsl src/voxel.inc shaders/light.hlsl)
add_shader(predepth.vert shaders/voxel.hlsl src/voxel.inc)
add_shader(raycast.frag)
add_shader(raycast.vert shaders/cube.hlsl)
add_shader(shadow.vert shaders/voxel.hlsl src/voxel.inc)
add_shader(sky.frag shaders/sky.hlsl)
add_shader(sky.vert)
add_shader(ssao.comp shaders/voxel.hlsl src/voxel.inc shaders/random.hlsl)
add_shader(transparent.frag shaders/voxel.hlsl src/voxel.inc shaders/light.hlsl shaders/shadow.hlsl)
add_shader(ui.comp)
