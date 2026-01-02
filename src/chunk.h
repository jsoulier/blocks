#pragma once

#include <SDL3/SDL.h>

#include "block.h"
#include "buffer.h"
#include "map.h"

typedef enum chunk_mesh_type
{
    CHUNK_MESH_TYPE_DEFAULT,
    CHUNK_MESH_TYPE_TRANSPARENT,
    CHUNK_MESH_TYPE_COUNT,
}
chunk_mesh_type_t;

typedef enum chunk_flag
{
    CHUNK_FLAG_NONE = 0,
    CHUNK_FLAG_GENERATE = 1,
    CHUNK_FLAG_MESH = 2,
}
chunk_flag_t;

typedef struct chunk
{
    chunk_flag_t flags;
    int x;
    int y;
    int z;
    map_t blocks;
    map_t lights;
    gpu_buffer_t vertex_buffers[CHUNK_MESH_TYPE_COUNT];
    gpu_buffer_t light_buffer;
}
chunk_t;

void chunk_init(chunk_t* chunk, SDL_GPUDevice* device);
void chunk_free(chunk_t* chunk, SDL_GPUDevice* device);
void chunk_set_block(chunk_t* chunk, int x, int y, int z, block_t block);
block_t chunk_get_block(const chunk_t* chunk, int x, int y, int z);
void chunk_set_light(chunk_t* chunk, int x, int y, int z, int light);