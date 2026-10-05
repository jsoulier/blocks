cmake_minimum_required(VERSION 3.24)
project(blocks)

if(ANDROID)
    set(BINARY_DIR ${CMAKE_SOURCE_DIR}/android/app/src/main/assets)
else()
    set(BINARY_DIR ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BINARY_DIR})
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${BINARY_DIR})
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${BINARY_DIR})
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${BINARY_DIR})
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE ${BINARY_DIR})
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${BINARY_DIR})
endif()
make_directory(${BINARY_DIR})
configure_file(textures/atlas.png ${BINARY_DIR} COPYONLY)

set(SRC
    lib/sqlite3/sqlite3.c
    lib/stb/stb.c
    src/block.c
    src/buffer.c
    src/camera.c
    src/input.c
    src/main.c
    src/map.c
    src/player.c
    src/rand.c
    src/save.c
    src/shader.c
    src/sky.c
    src/voxel.c
    src/worker.c
    src/world.c
)

if(EMSCRIPTEN)
    add_compile_options(-pthread)
endif()
add_subdirectory(lib/SDL)
if(ANDROID)
    add_library(blocks SHARED ${SRC})
    set_target_properties(blocks PROPERTIES OUTPUT_NAME main)
else()
    add_executable(blocks WIN32 ${SRC})
endif()
set_target_properties(blocks PROPERTIES C_STANDARD 11)
target_compile_definitions(blocks PRIVATE SDL_ASSERT_LEVEL=$<IF:$<CONFIG:Debug>,3,0>)
target_include_directories(blocks PUBLIC lib/jsmn)
target_include_directories(blocks PUBLIC lib/sqlite3)
target_include_directories(blocks PUBLIC lib/stb)
target_link_libraries(blocks PRIVATE SDL3::SDL3)
if(EMSCRIPTEN)
    set_target_properties(blocks PROPERTIES SUFFIX .html)
    target_link_options(blocks PRIVATE -pthread -sPTHREAD_POOL_SIZE=4 -sALLOW_MEMORY_GROWTH=1 -sASYNCIFY=1)
    target_link_options(blocks PRIVATE "SHELL:--preload-file ${BINARY_DIR}/atlas.png@/atlas.png")
    set_property(TARGET blocks APPEND PROPERTY LINK_DEPENDS ${BINARY_DIR}/atlas.png)
endif()

find_program(SHADERCROSS shadercross)
find_program(NAGA naga)
make_directory(${CMAKE_BINARY_DIR}/shaders)
function(add_shader FILE)
    set(DEPENDS ${ARGN})
    set(HLSL ${CMAKE_SOURCE_DIR}/shaders/${FILE})
    set(SPV ${CMAKE_SOURCE_DIR}/shaders/bin/${FILE}.spv)
    set(DXIL ${CMAKE_SOURCE_DIR}/shaders/bin/${FILE}.dxil)
    set(MSL ${CMAKE_SOURCE_DIR}/shaders/bin/${FILE}.msl)
    set(JSON ${CMAKE_SOURCE_DIR}/shaders/bin/${FILE}.json)
    set(WGSL ${CMAKE_SOURCE_DIR}/shaders/bin/${FILE}.wgsl)
    set(TMP_WGSL_SPV ${CMAKE_BINARY_DIR}/shaders/${FILE}.spv)
    function(compile OUTPUT)
        add_custom_command(
            OUTPUT ${OUTPUT}
            COMMAND ${SHADERCROSS} ${HLSL} -s hlsl -o ${OUTPUT} -I shaders
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
        compile(${DXIL})
        compile(${MSL})
        compile(${JSON})
    else()
        message("Using prebuilts since SDL_shadercross is missing")
    endif()
    if(EXISTS ${SHADERCROSS} AND EXISTS ${NAGA})
        add_custom_command(
            OUTPUT ${WGSL}
            COMMAND ${SHADERCROSS} ${HLSL} -s hlsl -d spirv -o ${TMP_WGSL_SPV} -I shaders -DWGSL
            COMMAND ${NAGA} --keep-coordinate-space ${TMP_WGSL_SPV} ${WGSL}
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            DEPENDS ${HLSL} ${DEPENDS}
            COMMENT ${WGSL}
        )
        string(REPLACE . _ NAME compile_${FILE}_wgsl)
        add_custom_target(${NAME} DEPENDS ${WGSL})
        add_dependencies(blocks ${NAME})
    else()
        message("Using prebuilts since naga is missing")
    endif()
    function(package OUTPUT)
        get_filename_component(NAME ${OUTPUT} NAME)
        set(BINARY ${BINARY_DIR}/${NAME})
        if(EMSCRIPTEN)
            target_link_options(blocks PRIVATE "SHELL:--preload-file ${BINARY}@/${NAME}")
            set_property(TARGET blocks APPEND PROPERTY LINK_DEPENDS ${BINARY})
        endif()
        add_custom_command(
            OUTPUT ${BINARY}
            COMMAND ${CMAKE_COMMAND} -E copy ${OUTPUT} ${BINARY}
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            DEPENDS ${OUTPUT}
            COMMENT ${BINARY}
        )
        string(REPLACE . _ NAME ${NAME})
        add_custom_target(package_${NAME} DEPENDS ${BINARY})
        add_dependencies(blocks package_${NAME})
        if(TARGET compile_${NAME})
            add_dependencies(package_${NAME} compile_${NAME})
        endif()
    endfunction()
    package(${SPV})
    if(WIN32)
        package(${DXIL})
    endif()
    if(APPLE)
        package(${MSL})
    endif()
    if(EMSCRIPTEN)
        package(${WGSL})
    endif()
    package(${JSON})
endfunction()
add_shader(hud.frag shaders/shader.hlsl src/hud.inc src/voxel.inc)
add_shader(hud.vert shaders/shader.hlsl src/hud.inc src/voxel.inc)
add_shader(opaque.frag shaders/shader.hlsl src/hud.inc src/voxel.inc)
add_shader(opaque.vert shaders/shader.hlsl src/hud.inc src/voxel.inc)
add_shader(raycast.frag shaders/shader.hlsl src/hud.inc src/voxel.inc)
add_shader(raycast.vert shaders/shader.hlsl src/hud.inc src/voxel.inc)
add_shader(sky.frag shaders/shader.hlsl src/hud.inc src/voxel.inc)
add_shader(sky.vert shaders/shader.hlsl src/hud.inc src/voxel.inc)
add_shader(transparent.frag shaders/shader.hlsl src/hud.inc src/voxel.inc)
add_shader(transparent.vert shaders/shader.hlsl src/hud.inc src/voxel.inc)
