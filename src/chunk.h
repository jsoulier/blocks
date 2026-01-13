#pragma once

#include <SDL3/SDL.h>

#include "block.h"
#include "buffer.h"
#include "map.h"

#define CHUNK_WIDTH 30
#define CHUNK_HEIGHT 240

typedef enum chunk_mesh_type
{
    CHUNK_MESH_TYPE_OPAQUE,
    CHUNK_MESH_TYPE_TRANSPARENT,
    CHUNK_MESH_TYPE_COUNT,
}
chunk_mesh_type_t;

typedef struct chunk
{
    SDL_GPUDevice* device;
    SDL_AtomicInt set_blocks;
    SDL_AtomicInt set_voxels;
    SDL_AtomicInt set_lights;
    SDL_AtomicInt has_blocks;
    SDL_AtomicInt has_voxels;
    SDL_AtomicInt has_lights;
    int x;
    int z;
    block_t blocks[CHUNK_WIDTH][CHUNK_HEIGHT][CHUNK_WIDTH];
    map_t lights;
    gpu_buffer_t gpu_voxels[CHUNK_MESH_TYPE_COUNT];
    gpu_buffer_t gpu_lights;
}
chunk_t;

void chunk_world_to_local(const chunk_t* chunk, int* x, int* y, int* z);
void chunk_local_to_world(const chunk_t* chunk, int* x, int* y, int* z);
void chunk_init(chunk_t* chunk, SDL_GPUDevice* device);
void chunk_free(chunk_t* chunk);
block_t chunk_set_block(chunk_t* chunk, int x, int y, int z, block_t block);
block_t chunk_get_block(chunk_t* chunk, int x, int y, int z);
void chunk_set_voxels(chunk_t* chunks[3][3], cpu_buffer_t voxels[CHUNK_MESH_TYPE_COUNT]);
void chunk_set_lights(chunk_t* chunks[3][3], cpu_buffer_t* lights);
void chunk_set_blocks(chunk_t* chunk);
