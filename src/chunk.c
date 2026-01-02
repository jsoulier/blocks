#include <SDL3/SDL.h>

#include "block.h"
#include "buffer.h"
#include "chunk.h"
#include "map.h"

static void transform(chunk_t* chunk, int* x, int* y, int* z)
{
    *x -= chunk->x;
    *y -= chunk->y;
    *z -= chunk->z;
}

void chunk_init(chunk_t* chunk, SDL_GPUDevice* device)
{
    chunk->flags = CHUNK_FLAG_GENERATE;
    chunk->x = 0;
    chunk->y = 0;
    chunk->z = 0;
    map_init(&chunk->blocks);
    map_init(&chunk->lights);
    for (int i = 0; i < CHUNK_MESH_TYPE_COUNT; i++)
    {
        gpu_buffer_init(&chunk->vertex_buffers[i], device, SDL_GPU_BUFFERUSAGE_VERTEX);
    }
    gpu_buffer_init(&chunk->light_buffer, device, SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ);
}

void chunk_free(chunk_t* chunk, SDL_GPUDevice* device)
{
    chunk->flags = CHUNK_FLAG_NONE;
    for (int i = 0; i < CHUNK_MESH_TYPE_COUNT; i++)
    {
        gpu_buffer_free(&chunk->vertex_buffers[i], device);
    }
    gpu_buffer_free(&chunk->light_buffer, device);
    map_free(&chunk->blocks);
    map_free(&chunk->lights);
}

void chunk_set_block(chunk_t* chunk, int x, int y, int z, block_t block)
{
    chunk->flags |= CHUNK_FLAG_MESH;
    transform(chunk, &x, &y, &z);
    map_set(&chunk->blocks, x, y, z, block);
}

block_t chunk_get_block(const chunk_t* chunk, int x, int y, int z)
{
    SDL_assert(chunk->flags & CHUNK_FLAG_GENERATE);
    transform(chunk, &x, &y, &z);
    return map_get(&chunk->blocks, x, y, z);
}

void chunk_set_light(chunk_t* chunk, int x, int y, int z, int light)
{
    chunk->flags |= CHUNK_FLAG_MESH;
    transform(chunk, &x, &y, &z);
    map_set(&chunk->blocks, x, y, z, light);
}
