#pragma once

#include <SDL3/SDL.h>

#include "block.h"
#include "buffer.h"
#include "map.h"

#define CHUNK_WIDTH 32
#define CHUNK_HEIGHT 256

typedef struct noise noise_t;

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
    gpu_buffer_t voxel_buffers[CHUNK_MESH_TYPE_COUNT];
    gpu_buffer_t light_buffer;
}
chunk_t;

void chunk_init(chunk_t* chunk, SDL_GPUDevice* device);
void chunk_free(chunk_t* chunk);
void chunk_set_block(chunk_t* chunk, int x, int y, int z, block_t block);
block_t chunk_get_block(const chunk_t* chunk, int x, int y, int z);
void chunk_generate(chunk_t* chunk, const noise_t* noise);
void chunk_mesh(chunk_t* chunk, const chunk_t* neighbors[3][3],
    cpu_buffer_t* voxel_buffer, cpu_buffer_t* light_buffer, SDL_GPUCopyPass* pass);