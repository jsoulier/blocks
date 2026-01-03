#include <SDL3/SDL.h>

#include <stdint.h>

#include "block.h"
#include "buffer.h"
#include "chunk.h"
#include "map.h"
#include "noise.h"

static void transform(const chunk_t* chunk, int* x, int* y, int* z)
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
        gpu_buffer_init(&chunk->voxel_buffers[i], device, SDL_GPU_BUFFERUSAGE_VERTEX);
    }
    gpu_buffer_init(&chunk->light_buffer, device, SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ);
}

void chunk_free(chunk_t* chunk)
{
    chunk->flags = CHUNK_FLAG_NONE;
    for (int i = 0; i < CHUNK_MESH_TYPE_COUNT; i++)
    {
        gpu_buffer_free(&chunk->voxel_buffers[i]);
    }
    gpu_buffer_free(&chunk->light_buffer);
    map_free(&chunk->blocks);
    map_free(&chunk->lights);
}

void chunk_set_block(chunk_t* chunk, int x, int y, int z, block_t block)
{
    chunk->flags |= CHUNK_FLAG_MESH;
    transform(chunk, &x, &y, &z);
    map_set(&chunk->blocks, x, y, z, block);
    // TODO: check if light (if so, add to lights)
}

block_t chunk_get_block(const chunk_t* chunk, int x, int y, int z)
{
    SDL_assert(chunk->flags & CHUNK_FLAG_GENERATE);
    transform(chunk, &x, &y, &z);
    return map_get(&chunk->blocks, x, y, z);
}

void chunk_generate(chunk_t* chunk, const noise_t* noise)
{
    SDL_assert(chunk->flags & CHUNK_FLAG_GENERATE);
    map_clear(&chunk->blocks);
    map_clear(&chunk->lights);
    noise_generate(noise, chunk);
    chunk->flags &= ~CHUNK_FLAG_GENERATE;
    chunk->flags |= CHUNK_FLAG_MESH;
}

void chunk_mesh(chunk_t* chunk, const chunk_t* neighbors[3][3],
    cpu_buffer_t* voxel_buffer, cpu_buffer_t* light_buffer, SDL_GPUCopyPass* pass)
{
    for (uint32_t i = 0; i < chunk->blocks.capacity; i++)
    {
        if (!map_is_valid(&chunk->blocks, i))
        {
            continue;
        }
        map_row_t row = map_get_row(&chunk->blocks, i);
        SDL_assert(row.value != BLOCK_EMPTY);
    }
}
