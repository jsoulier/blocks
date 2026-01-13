#include <SDL3/SDL.h>

#include "block.h"
#include "buffer.h"
#include "chunk.h"
#include "direction.h"
#include "map.h"
#include "noise.h"
#include "voxel.h"

static bool is_local(int x, int y, int z)
{
    SDL_assert(y >= 0);
    SDL_assert(y < CHUNK_HEIGHT);
    return x >= 0 && z >= 0 && x < CHUNK_WIDTH && z < CHUNK_WIDTH;
}

void chunk_world_to_local(const chunk_t* chunk, int* x, int* y, int* z)
{
    *x -= chunk->x;
    *z -= chunk->z;
    SDL_assert(is_local(*x, *y, *z));
}

void chunk_local_to_world(const chunk_t* chunk, int* x, int* y, int* z)
{
    // TODO: SDL_assert(is_local(*x, *y, *z));
    *x += chunk->x;
    *z += chunk->z;
}

void chunk_init(chunk_t* chunk, SDL_GPUDevice* device)
{
    chunk->device = device;
    SDL_SetAtomicInt(&chunk->has_blocks, false);
    SDL_SetAtomicInt(&chunk->set_blocks, true);
    SDL_SetAtomicInt(&chunk->set_voxels, false);
    SDL_SetAtomicInt(&chunk->set_lights, false);
    chunk->x = 0;
    chunk->z = 0;
    // map_init(&chunk->blocks, 32);
    map_init(&chunk->lights, 1);
    for (int i = 0; i < CHUNK_MESH_TYPE_COUNT; i++)
    {
        gpu_buffer_init(&chunk->gpu_voxels[i], device, SDL_GPU_BUFFERUSAGE_VERTEX);
    }
    gpu_buffer_init(&chunk->gpu_lights, device, SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ);
}

void chunk_free(chunk_t* chunk)
{
    gpu_buffer_free(&chunk->gpu_lights);
    for (int i = 0; i < CHUNK_MESH_TYPE_COUNT; i++)
    {
        gpu_buffer_free(&chunk->gpu_voxels[i]);
    }
    // map_free(&chunk->blocks);
    map_free(&chunk->lights);
    chunk->x = 0;
    chunk->z = 0;
    SDL_SetAtomicInt(&chunk->has_blocks, false);
    SDL_SetAtomicInt(&chunk->set_blocks, false);
    SDL_SetAtomicInt(&chunk->set_voxels, false);
    SDL_SetAtomicInt(&chunk->set_lights, false);
    chunk->device = NULL;
}

block_t chunk_set_block(chunk_t* chunk, int x, int y, int z, block_t block)
{
    SDL_SetAtomicInt(&chunk->set_voxels, true);
    SDL_SetAtomicInt(&chunk->has_voxels, false);
    chunk_world_to_local(chunk, &x, &y, &z);
    chunk->blocks[x][y][z] = block;
    // if (block != BLOCK_EMPTY)
    // {
    //     map_set(&chunk->blocks, x, y, z, block);
    // }
    // else
    // {
    //     map_remove(&chunk->blocks, x, y, z);
    // }
    block_t old_block = map_get(&chunk->lights, x, y, z);
    if (!block_is_light(block) && !block_is_light(old_block))
    {
        return old_block;
    }
    SDL_SetAtomicInt(&chunk->set_lights, true);
    SDL_SetAtomicInt(&chunk->has_lights, false);
    if (block_is_light(block))
    {
        map_set(&chunk->lights, x, y, z, block);
    }
    else
    {
        map_remove(&chunk->lights, x, y, z);
    }
    return old_block;
}

block_t chunk_get_block(chunk_t* chunk, int x, int y, int z)
{
    SDL_assert(SDL_GetAtomicInt(&chunk->has_blocks));
    chunk_world_to_local(chunk, &x, &y, &z);
    // return map_get(&chunk->blocks, x, y, z);
    return chunk->blocks[x][y][z];
}

static block_t get_block(chunk_t* chunks[3][3], int x, int y, int z, int dx, int dy, int dz)
{
    SDL_assert(dx >= -1 && dx <= 1);
    SDL_assert(dy >= -1 && dy <= 1);
    SDL_assert(dz >= -1 && dz <= 1);
    x += dx;
    y += dy;
    z += dz;
    const chunk_t* chunk = chunks[1][1];
    if (y == CHUNK_HEIGHT)
    {
        return BLOCK_EMPTY;
    }
    else if (y == -1)
    {
        return BLOCK_GRASS;
    }
    if (is_local(x, y, z))
    {
        // return map_get(&chunk->blocks, x, y, z);
        return chunk->blocks[x][y][z];
    }
    chunk_local_to_world(chunk, &x, &y, &z);
    chunk_t* neighbor = chunks[dx + 1][dz + 1];
    SDL_assert(neighbor);
    return chunk_get_block(neighbor, x, y, z);
}

static void upload_voxels(chunk_t* chunk, cpu_buffer_t voxels[CHUNK_MESH_TYPE_COUNT])
{
    bool has_voxels = false;
    for (int i = 0; i < CHUNK_MESH_TYPE_COUNT; i++)
    {
        gpu_buffer_clear(&chunk->gpu_voxels[i]);
        has_voxels |= voxels[i].size > 0;
    }
    if (!has_voxels)
    {
        return;
    }
    SDL_GPUCommandBuffer* command_buffer = SDL_AcquireGPUCommandBuffer(chunk->device);
    if (!command_buffer)
    {
        SDL_Log("Failed to acquire command buffer: %s", SDL_GetError());
        return;
    }
    SDL_GPUCopyPass* copy_pass = SDL_BeginGPUCopyPass(command_buffer);
    if (!copy_pass)
    {
        SDL_Log("Failed to begin copy pass: %s", SDL_GetError());
        SDL_CancelGPUCommandBuffer(command_buffer);
        return;
    }
    for (int i = 0; i < CHUNK_MESH_TYPE_COUNT; i++)
    {
        gpu_buffer_update(&chunk->gpu_voxels[i], copy_pass, &voxels[i]);
    }
    SDL_EndGPUCopyPass(copy_pass);
    SDL_SubmitGPUCommandBuffer(command_buffer);
}

static void upload_lights(chunk_t* chunk, cpu_buffer_t* lights)
{
    gpu_buffer_clear(&chunk->gpu_lights);
    if (!lights->size)
    {
        return;
    }
    SDL_GPUCommandBuffer* command_buffer = SDL_AcquireGPUCommandBuffer(chunk->device);
    if (!command_buffer)
    {
        SDL_Log("Failed to acquire command buffer: %s", SDL_GetError());
        return;
    }
    SDL_GPUCopyPass* copy_pass = SDL_BeginGPUCopyPass(command_buffer);
    if (!copy_pass)
    {
        SDL_Log("Failed to begin copy pass: %s", SDL_GetError());
        SDL_CancelGPUCommandBuffer(command_buffer);
        return;
    }
    gpu_buffer_update(&chunk->gpu_lights, copy_pass, lights);
    SDL_EndGPUCopyPass(copy_pass);
    SDL_SubmitGPUCommandBuffer(command_buffer);
}

void chunk_set_voxels(chunk_t* chunks[3][3], cpu_buffer_t voxels[CHUNK_MESH_TYPE_COUNT])
{
    chunk_t* chunk = chunks[1][1];
    SDL_assert(SDL_GetAtomicInt(&chunk->has_blocks));
    SDL_assert(!SDL_GetAtomicInt(&chunk->set_blocks));
    SDL_assert(!SDL_GetAtomicInt(&chunk->has_voxels));
    SDL_assert(!SDL_GetAtomicInt(&chunk->set_voxels));
    for (Uint32 x = 0; x < CHUNK_WIDTH; x++)
    for (Uint32 y = 0; y < CHUNK_HEIGHT; y++)
    for (Uint32 z = 0; z < CHUNK_WIDTH; z++)
    {
        block_t block = chunk->blocks[x][y][z];
        if (block == BLOCK_EMPTY)
        {
            continue;
        }
        if (block_is_sprite(block))
        {
            for (int j = 0; j < 4; j++)
            for (int k = 0; k < 4; k++)
            {
                voxel_t voxel = voxel_pack_sprite(block, x, y, z, j, k);
                cpu_buffer_append(&voxels[CHUNK_MESH_TYPE_OPAQUE], &voxel);
            }
            continue;
        }
        for (int j = 0; j < 6; j++)
        {
            int dx = DIRECTIONS[j][0];
            int dy = DIRECTIONS[j][1];
            int dz = DIRECTIONS[j][2];
            block_t neighbor = get_block(chunks, x, y, z, dx, dy, dz);
            bool is_opaque = block_is_opaque(block);
            bool is_forced = neighbor == BLOCK_EMPTY || block_is_sprite(neighbor);
            bool is_opaque_next_to_transparent = is_opaque && !block_is_opaque(neighbor);
            if (!is_forced && !is_opaque_next_to_transparent)
            {
                continue;
            }
            for (int k = 0; k < 4; k++)
            {
                voxel_t voxel = voxel_pack_cube(block, x, y, z, j, k);
                if (is_opaque)
                {
                    cpu_buffer_append(&voxels[CHUNK_MESH_TYPE_OPAQUE], &voxel);
                }
                else
                {
                    cpu_buffer_append(&voxels[CHUNK_MESH_TYPE_TRANSPARENT], &voxel);
                }
            }
        }
    }
    upload_voxels(chunk, voxels);
    SDL_SetAtomicInt(&chunk->has_voxels, true);
}

void chunk_set_lights(chunk_t* chunks[3][3], cpu_buffer_t* lights)
{
    chunk_t* chunk = chunks[1][1];
    SDL_assert(SDL_GetAtomicInt(&chunk->has_blocks));
    SDL_assert(!SDL_GetAtomicInt(&chunk->set_blocks));
    SDL_assert(!SDL_GetAtomicInt(&chunk->set_voxels));
    SDL_assert(!SDL_GetAtomicInt(&chunk->has_lights));
    SDL_assert(!SDL_GetAtomicInt(&chunk->set_lights));
    for (int i = -1; i <= 1; i++)
    for (int j = -1; j <= 1; j++)
    {
        const chunk_t* neighbor = chunks[i + 1][j + 1];
        SDL_assert(neighbor);
        for (Uint32 i = 0; i < neighbor->lights.capacity; i++)
        {
            if (!map_is_row_valid(&neighbor->lights, i))
            {
                continue;
            }
            map_row_t row = map_get_row(&neighbor->lights, i);
            SDL_assert(row.value != BLOCK_EMPTY);
            SDL_assert(block_is_light(row.value));
            light_t light = block_get_light(row.value);
            light.x = neighbor->x + row.x;
            light.y = row.y;
            light.z = neighbor->z + row.z;
            cpu_buffer_append(lights, &light);
        }
    }
    upload_lights(chunk, lights);
    SDL_SetAtomicInt(&chunk->has_lights, true);
}

void chunk_set_blocks(chunk_t* chunk)
{
    SDL_assert(!SDL_GetAtomicInt(&chunk->has_blocks));
    SDL_assert(!SDL_GetAtomicInt(&chunk->set_blocks));
    SDL_memset(chunk->blocks, 0, sizeof(chunk->blocks));
    // map_clear(&chunk->blocks);
    map_clear(&chunk->lights);
    noise_generate(chunk, chunk->x, chunk->z);
    SDL_SetAtomicInt(&chunk->has_blocks, true);
    if (SDL_GetAtomicInt(&chunk->set_voxels))
    {
        SDL_SetAtomicInt(&chunk->set_lights, true);
    }
}
