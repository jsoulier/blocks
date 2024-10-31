#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdint.h>
#include "block.h"
#include "helpers.h"
#include "voxmesh.h"
#include "world.h"

static_assert(VOXEL_X_OFFSET + VOXEL_X_BITS <= 32, "");
static_assert(VOXEL_Y_OFFSET + VOXEL_Y_BITS <= 32, "");
static_assert(VOXEL_Z_OFFSET + VOXEL_Z_BITS <= 32, "");
static_assert(VOXEL_U_OFFSET + VOXEL_U_BITS <= 32, "");
static_assert(VOXEL_V_OFFSET + VOXEL_V_BITS <= 32, "");
static_assert(VOXEL_DIRECTION_OFFSET + VOXEL_DIRECTION_BITS <= 32, "");

static uint32_t pack(
    const block_t block,
    const int x,
    const int y,
    const int z,
    const direction_t direction,
    const int i)
{
    assert(block > BLOCK_EMPTY);
    assert(block < BLOCK_COUNT);
    assert(direction < DIRECTION_3);
    assert(i < 4);
    static const int positions[][4][3] =
    {
        {{0, 0, 1}, {0, 1, 1}, {1, 0, 1}, {1, 1, 1}},
        {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {1, 1, 0}},
        {{1, 0, 0}, {1, 0, 1}, {1, 1, 0}, {1, 1, 1}},
        {{0, 0, 0}, {0, 1, 0}, {0, 0, 1}, {0, 1, 1}},
        {{0, 1, 0}, {1, 1, 0}, {0, 1, 1}, {1, 1, 1}},
        {{0, 0, 0}, {0, 0, 1}, {1, 0, 0}, {1, 0, 1}},
    };
    static const int uvs[][4][2] =
    {
        {{1, 1}, {1, 0}, {0, 1}, {0, 0}},
        {{1, 1}, {0, 1}, {1, 0}, {0, 0}},
        {{1, 1}, {0, 1}, {1, 0}, {0, 0}},
        {{1, 1}, {1, 0}, {0, 1}, {0, 0}},
        {{0, 0}, {1, 0}, {0, 1}, {1, 1}},
        {{0, 0}, {0, 1}, {1, 0}, {1, 1}},
    };
    const int a = positions[direction][i][0] + x;
    const int b = positions[direction][i][1] + y;
    const int c = positions[direction][i][2] + z;
    const int d = uvs[direction][i][0] + blocks[block][direction][0];
    const int e = uvs[direction][i][1] + blocks[block][direction][1];
    assert(a <= VOXEL_X_MASK);
    assert(b <= VOXEL_Y_MASK);
    assert(c <= VOXEL_Z_MASK);
    assert(d <= VOXEL_U_MASK);
    assert(e <= VOXEL_V_MASK);
    assert(direction <= VOXEL_DIRECTION_MASK);
    uint32_t voxel = 0;
    voxel |= a << VOXEL_X_OFFSET;
    voxel |= b << VOXEL_Y_OFFSET;
    voxel |= c << VOXEL_Z_OFFSET;
    voxel |= d << VOXEL_U_OFFSET;
    voxel |= e << VOXEL_V_OFFSET;
    voxel |= direction << VOXEL_DIRECTION_OFFSET;
    return voxel;
}

static uint32_t pack_sprite(
    const block_t block,
    const int x,
    const int y,
    const int z,
    const int direction,
    const int i)
{
    assert(block > BLOCK_EMPTY);
    assert(block < BLOCK_COUNT);
    assert(direction < 4);
    assert(i < 4);
    static const int positions[][4][3] =
    {
        {{0, 0, 0}, {0, 1, 0}, {1, 0, 1}, {1, 1, 1}},
        {{0, 0, 0}, {1, 0, 1}, {0, 1, 0}, {1, 1, 1}},
        {{0, 0, 1}, {1, 0, 0}, {0, 1, 1}, {1, 1, 0}},
        {{0, 0, 1}, {0, 1, 1}, {1, 0, 0}, {1, 1, 0}},
    };
    static const int uvs[][4][2] =
    {
        {{1, 1}, {1, 0}, {0, 1}, {0, 0}},
        {{1, 1}, {0, 1}, {1, 0}, {0, 0}},
        {{1, 1}, {0, 1}, {1, 0}, {0, 0}},
        {{1, 1}, {1, 0}, {0, 1}, {0, 0}},
    };
    const int a = positions[direction][i][0] + x;
    const int b = positions[direction][i][1] + y;
    const int c = positions[direction][i][2] + z;
    const int d = uvs[direction][i][0] + blocks[block][DIRECTION_N][0];
    const int e = uvs[direction][i][1] + blocks[block][DIRECTION_N][1];
    assert(a <= VOXEL_X_MASK);
    assert(b <= VOXEL_Y_MASK);
    assert(c <= VOXEL_Z_MASK);
    assert(d <= VOXEL_U_MASK);
    assert(e <= VOXEL_V_MASK);
    uint32_t voxel = 0;
    voxel |= a << VOXEL_X_OFFSET;
    voxel |= b << VOXEL_Y_OFFSET;
    voxel |= c << VOXEL_Z_OFFSET;
    voxel |= d << VOXEL_U_OFFSET;
    voxel |= e << VOXEL_V_OFFSET;
    return voxel;
}

static void fill(
    const chunk_t* chunk,
    const chunk_t* neighbors[DIRECTION_3],
    const int height,
    uint32_t* opaque_data,
    uint32_t* transparent_data,
    uint32_t* opaque_size,
    uint32_t* transparent_size,
    const uint32_t opaque_capacity,
    const uint32_t transparent_capacity)
{
    assert(chunk);
    assert(opaque_size);
    assert(transparent_size);
    *opaque_size = 0;
    *transparent_size = 0;
    for (int x = 0; x < CHUNK_X; x++)
    for (int y = 0; y < CHUNK_Y; y++)
    for (int z = 0; z < CHUNK_Z; z++)
    {
        const block_t a = chunk->blocks[x][y][z];
        if (a == BLOCK_EMPTY)
        {
            continue;
        }
        uint32_t* data;
        uint32_t* size;
        uint32_t capacity;
        if (block_opaque(a))
        {
            data = opaque_data;
            size = opaque_size;
            capacity = opaque_capacity;
        }
        else
        {
            data = transparent_data;
            size = transparent_size;
            capacity = transparent_capacity;
        }
        if (block_sprite(a))
        {
            *size += 4;
            if (*size > capacity)
            {
                continue;
            }
            for (int direction = 0; direction < 4; direction++)
            {
                for (int i = 0; i < 4; i++)
                {
                    data[*size * 4 - 4 * (direction + 1) + i] = pack_sprite(a, x, y, z, direction, i);
                }            
            }
            continue;
        }
        for (direction_t d = 0; d < DIRECTION_3; d++)
        {
            if (height == 0 && y == 0 && d != DIRECTION_U)
            {
                continue;
            }
            block_t b;
            int s = x + directions[d][0];
            int t = y + directions[d][1];
            int p = z + directions[d][2];
            if (chunk_in(s, t, p))
            {
                b = chunk->blocks[s][t][p];
            }
            else if (neighbors[d])
            {
                chunk_wrap(&s, &t, &p);
                b = neighbors[d]->blocks[s][t][p];
            }
            else
            {
                b = BLOCK_EMPTY;
            }
            if (b != BLOCK_EMPTY && !block_sprite(b) && !(block_opaque(a) && !block_opaque(b)))
            {
                continue;
            }
            if (++(*size) > capacity)
            {
                continue;
            }
            for (int i = 0; i < 4; i++)
            {
                data[*size * 4 - 4 + i] = pack(a, x, y, z, d, i);    
            }
        }
    }
}

bool voxmesh_vbo(
    chunk_t* chunk,
    chunk_t* neighbors[DIRECTION_3],
    const int height,
    SDL_GPUDevice* device,
    SDL_GPUTransferBuffer** opaque_tbo,
    SDL_GPUTransferBuffer** transparent_tbo,
    uint32_t* opaque_capacity,
    uint32_t* transparent_capacity)
{
    assert(chunk);
    assert(device);
    assert(opaque_tbo);
    assert(transparent_tbo);
    assert(opaque_capacity);
    assert(transparent_capacity);
    void* opaque_data = *opaque_tbo;
    void* transparent_data = *transparent_tbo;
    if (opaque_data)
    {
        opaque_data = SDL_MapGPUTransferBuffer(device, *opaque_tbo, true);
        if (!opaque_data)
        {
            SDL_Log("Failed to map tbo buffer: %s", SDL_GetError());
            return false;
        }
    }
    if (transparent_data)
    {
        transparent_data = SDL_MapGPUTransferBuffer(device, *transparent_tbo, true);
        if (!transparent_data)
        {
            SDL_Log("Failed to map tbo buffer: %s", SDL_GetError());
            return false;
        }
    }
    fill(
        chunk,
        neighbors,
        height,
        opaque_data,
        transparent_data,
        &chunk->opaque_size,
        &chunk->transparent_size,
        *opaque_capacity,
        *transparent_capacity);
    if (opaque_data)
    {
        SDL_UnmapGPUTransferBuffer(device, *opaque_tbo);
        opaque_data = NULL;
    }
    if (transparent_data)
    {
        SDL_UnmapGPUTransferBuffer(device, *transparent_tbo);
        transparent_data = NULL;
    }
    if (!chunk->opaque_size && !chunk->transparent_size)
    {
        return false;
    }
    if (chunk->opaque_size > *opaque_capacity || chunk->transparent_size > *transparent_capacity)
    {
        if (chunk->opaque_size > *opaque_capacity)
        {
            if (*opaque_tbo)
            {
                SDL_ReleaseGPUTransferBuffer(device, *opaque_tbo);
                *opaque_capacity = 0;
            }
            SDL_GPUTransferBufferCreateInfo tbci = {0};
            tbci.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
            tbci.size = chunk->opaque_size * 16;
            *opaque_tbo = SDL_CreateGPUTransferBuffer(device, &tbci);
            if (!(*opaque_tbo))
            {
                SDL_Log("Failed to create tbo buffer: %s", SDL_GetError());
                return false;
            }
            *opaque_capacity = chunk->opaque_size;
        }
        if (chunk->transparent_size > *transparent_capacity)
        {
            if (*transparent_tbo)
            {
                SDL_ReleaseGPUTransferBuffer(device, *transparent_tbo);
                *transparent_capacity = 0;
            }
            SDL_GPUTransferBufferCreateInfo tbci = {0};
            tbci.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
            tbci.size = chunk->transparent_size * 16;
            *transparent_tbo = SDL_CreateGPUTransferBuffer(device, &tbci);
            if (!(*transparent_tbo))
            {
                SDL_Log("Failed to create tbo buffer: %s", SDL_GetError());
                return false;
            }
            *transparent_capacity = chunk->transparent_size;
        }
        if (chunk->opaque_size)
        {
            opaque_data = SDL_MapGPUTransferBuffer(device, *opaque_tbo, false);
            if (!opaque_data)
            {
                SDL_Log("Failed to map tbo buffer: %s", SDL_GetError());
                return false;
            }
        }
        if (chunk->transparent_size)
        {
            transparent_data = SDL_MapGPUTransferBuffer(device, *transparent_tbo, false);
            if (!transparent_data)
            {
                SDL_Log("Failed to map tbo buffer: %s", SDL_GetError());
                return false;
            }
        }
        fill(
            chunk,
            neighbors,
            height,
            opaque_data,
            transparent_data,
            &chunk->opaque_size,
            &chunk->transparent_size,
            *opaque_capacity,
            *transparent_capacity);
        if (opaque_data)
        {
            SDL_UnmapGPUTransferBuffer(device, *opaque_tbo);
        }
        if (transparent_data)
        {
            SDL_UnmapGPUTransferBuffer(device, *transparent_tbo);
        }
    }
    if (chunk->opaque_size > chunk->opaque_capacity)
    {
        if (chunk->opaque_vbo)
        {
            SDL_ReleaseGPUBuffer(device, chunk->opaque_vbo);
            chunk->opaque_capacity = 0;
        }
        SDL_GPUBufferCreateInfo bci = {0};
        bci.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
        bci.size = chunk->opaque_size * 16;
        chunk->opaque_vbo = SDL_CreateGPUBuffer(device, &bci);
        if (!chunk->opaque_vbo)
        {
            SDL_Log("Failed to create vertex buffer: %s", SDL_GetError());
            return false;
        }
        chunk->opaque_capacity = chunk->opaque_size;
    }
    if (chunk->transparent_size > chunk->transparent_capacity)
    {
        if (chunk->transparent_vbo)
        {
            SDL_ReleaseGPUBuffer(device, chunk->transparent_vbo);
            chunk->transparent_capacity = 0;
        }
        SDL_GPUBufferCreateInfo bci = {0};
        bci.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
        bci.size = chunk->transparent_size * 16;
        chunk->transparent_vbo = SDL_CreateGPUBuffer(device, &bci);
        if (!chunk->transparent_vbo)
        {
            SDL_Log("Failed to create vertex buffer: %s", SDL_GetError());
            return false;
        }
        chunk->transparent_capacity = chunk->transparent_size;
    }
    SDL_GPUCommandBuffer* commands = SDL_AcquireGPUCommandBuffer(device);
    if (!commands)
    {
        SDL_Log("Failed to acquire command buffer: %s", SDL_GetError());
        return false;
    }
    SDL_GPUCopyPass* pass = SDL_BeginGPUCopyPass(commands);
    if (!pass)
    {
        SDL_Log("Failed to begin copy pass: %s", SDL_GetError());
        return false;
    }
    SDL_GPUTransferBufferLocation location = {0};
    SDL_GPUBufferRegion region = {0};
    if (chunk->opaque_size)
    {
        location.transfer_buffer = *opaque_tbo;
        region.size = chunk->opaque_size * 16;
        region.buffer = chunk->opaque_vbo;
        SDL_UploadToGPUBuffer(pass, &location, &region, 1);
    }
    if (chunk->transparent_size)
    {
        location.transfer_buffer = *transparent_tbo;
        region.size = chunk->transparent_size * 16;
        region.buffer = chunk->transparent_vbo;
        SDL_UploadToGPUBuffer(pass, &location, &region, 1);
    }
    SDL_EndGPUCopyPass(pass);
    SDL_SubmitGPUCommandBuffer(commands);
    return true;
}

bool voxmesh_ibo(
    SDL_GPUDevice* device,
    SDL_GPUBuffer** ibo,
    const uint32_t size)
{
    assert(device);
    assert(ibo);
    assert(size);
    SDL_GPUTransferBufferCreateInfo tbci = {0};
    tbci.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    tbci.size = size * 24;
    SDL_GPUTransferBuffer* tbo = SDL_CreateGPUTransferBuffer(device, &tbci);
    if (!tbo)
    {
        SDL_Log("Failed to create tbo buffer: %s", SDL_GetError());
        return false;
    }
    SDL_GPUBufferCreateInfo bci = {0};
    bci.usage = SDL_GPU_BUFFERUSAGE_INDEX;
    bci.size = size * 24;
    *ibo = SDL_CreateGPUBuffer(device, &bci);
    if (!(*ibo))
    {
        SDL_Log("Failed to create index buffer: %s", SDL_GetError());
        return false;
    }
    uint32_t* data = SDL_MapGPUTransferBuffer(device, tbo, false);
    if (!data)
    {
        SDL_Log("Failed to map tbo buffer: %s", SDL_GetError());
        return false;
    }
    for (uint32_t i = 0; i < size; i++)
    {
        data[i * 6 + 0] = i * 4 + 0;
        data[i * 6 + 1] = i * 4 + 1;
        data[i * 6 + 2] = i * 4 + 2;
        data[i * 6 + 3] = i * 4 + 3;
        data[i * 6 + 4] = i * 4 + 2;
        data[i * 6 + 5] = i * 4 + 1;
    }
    SDL_UnmapGPUTransferBuffer(device, tbo);
    SDL_GPUCommandBuffer* commands = SDL_AcquireGPUCommandBuffer(device);
    if (!commands)
    {
        SDL_Log("Failed to acquire command buffer: %s", SDL_GetError());
        return false;
    }
    SDL_GPUCopyPass* pass = SDL_BeginGPUCopyPass(commands);
    if (!pass)
    {
        SDL_Log("Failed to begin copy pass: %s", SDL_GetError());
        return false;
    }
    SDL_GPUTransferBufferLocation location = {0};
    location.transfer_buffer = tbo;
    SDL_GPUBufferRegion region = {0};
    region.size = size * 24;
    region.buffer = *ibo;
    SDL_UploadToGPUBuffer(pass, &location, &region, 1);
    SDL_EndGPUCopyPass(pass);
    SDL_SubmitGPUCommandBuffer(commands);
    SDL_ReleaseGPUTransferBuffer(device, tbo);
    return true;
}