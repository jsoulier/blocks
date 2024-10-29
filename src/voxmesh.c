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

static uint32_t fill(
    const chunk_t* chunk,
    const chunk_t* neighbors[DIRECTION_3],
    const int height,
    uint32_t* data,
    const uint32_t capacity)
{
    assert(chunk);
    uint32_t size = 0;
    for (int x = 0; x < CHUNK_X; x++)
    for (int y = 0; y < CHUNK_Y; y++)
    for (int z = 0; z < CHUNK_Z; z++) {
        const block_t a = chunk->blocks[x][y][z];
        if (a == BLOCK_EMPTY) {
            continue;
        }
        for (direction_t d = 0; d < DIRECTION_3; d++) {
            if (height == 0 && y == 0 && d != DIRECTION_U) {
                continue;
            }
            block_t b;
            int s = x + directions[d][0];
            int t = y + directions[d][1];
            int p = z + directions[d][2];
            if (chunk_in(s, t, p)) {
                b = chunk->blocks[s][t][p];
            } else if (neighbors[d]) {
                s = (s + CHUNK_X) % CHUNK_X;
                t = (t + CHUNK_Y) % CHUNK_Y;
                p = (p + CHUNK_Z) % CHUNK_Z;
                b = neighbors[d]->blocks[s][t][p];
            } else {
                b = BLOCK_EMPTY;
            }
            if ((b != BLOCK_EMPTY && ( block_opaque(a) ||
                !block_opaque(b))) || ++size > capacity) {
                continue;
            }
            for (int i = 0; i < 4; i++) {
                data[(size * 4) - 4 + i] = pack(a, x, y, z, d, i);    
            }
        }
    }
    return size;
}

bool voxmesh_vbo(
    chunk_t* chunk,
    chunk_t* neighbors[DIRECTION_3],
    const int height,
    SDL_GPUDevice* device,
    SDL_GPUTransferBuffer** tbo,
    uint32_t* capacity)
{
    assert(chunk);
    assert(device);
    assert(tbo);
    assert(capacity);
    void* data = *tbo;
    if (data) {
        data = SDL_MapGPUTransferBuffer(device, *tbo, true);
        if (!data) {
            SDL_Log("Failed to map tbo buffer: %s", SDL_GetError());
            return false;
        }
    }
    chunk->size = fill(chunk, neighbors, height, data, *capacity);
    if (data) {
        SDL_UnmapGPUTransferBuffer(device, *tbo);
    }
    if (!chunk->size) {
        return false;
    }
    if (chunk->size > *capacity) {
        if (*tbo) {
            SDL_ReleaseGPUTransferBuffer(device, *tbo);
            *capacity = 0;
        }
        SDL_GPUTransferBufferCreateInfo tbci = {0};
        tbci.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        tbci.size = chunk->size * 16;
        *tbo = SDL_CreateGPUTransferBuffer(device, &tbci);
        if (!*tbo) {
            SDL_Log("Failed to create tbo buffer: %s", SDL_GetError());
            return false;
        }
        *capacity = chunk->size;
        data = SDL_MapGPUTransferBuffer(device, *tbo, false);
        if (!data) {
            SDL_Log("Failed to map tbo buffer: %s", SDL_GetError());
            return false;
        }
        chunk->size = fill(chunk, neighbors, height, data, *capacity);
        SDL_UnmapGPUTransferBuffer(device, *tbo);
    }
    if (chunk->size > chunk->capacity) {
        if (chunk->vbo) {
            SDL_ReleaseGPUBuffer(device, chunk->vbo);
            chunk->capacity = 0;
        }
        SDL_GPUBufferCreateInfo bci = {0};
        bci.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
        bci.size = chunk->size * 16;
        chunk->vbo = SDL_CreateGPUBuffer(device, &bci);
        if (!chunk->vbo) {
            SDL_Log("Failed to create vertex buffer: %s", SDL_GetError());
            return false;
        }
        chunk->capacity = chunk->size;
    }
    SDL_GPUCommandBuffer* commands = SDL_AcquireGPUCommandBuffer(device);
    if (!commands) {
        SDL_Log("Failed to acquire command buffer: %s", SDL_GetError());
        return false;
    }
    SDL_GPUCopyPass* pass = SDL_BeginGPUCopyPass(commands);
    if (!pass) {
        SDL_Log("Failed to begin copy pass: %s", SDL_GetError());
        return false;
    }
    SDL_GPUTransferBufferLocation location = {0};
    location.transfer_buffer = *tbo;
    SDL_GPUBufferRegion region = {0};
    region.size = chunk->size * 16;
    region.buffer = chunk->vbo;
    SDL_UploadToGPUBuffer(pass, &location, &region, 1);
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
    if (!tbo) {
        SDL_Log("Failed to create tbo buffer: %s", SDL_GetError());
        return false;
    }
    SDL_GPUBufferCreateInfo bci = {0};
    bci.usage = SDL_GPU_BUFFERUSAGE_INDEX;
    bci.size = size * 24;
    *ibo = SDL_CreateGPUBuffer(device, &bci);
    if (!*ibo) {
        SDL_Log("Failed to create index buffer: %s", SDL_GetError());
        return false;
    }
    uint32_t* data = SDL_MapGPUTransferBuffer(device, tbo, false);
    if (!data) {
        SDL_Log("Failed to map tbo buffer: %s", SDL_GetError());
        return false;
    }
    for (uint32_t i = 0; i < size; i++) {
        data[i * 6 + 0] = i * 4 + 0;
        data[i * 6 + 1] = i * 4 + 1;
        data[i * 6 + 2] = i * 4 + 2;
        data[i * 6 + 3] = i * 4 + 3;
        data[i * 6 + 4] = i * 4 + 2;
        data[i * 6 + 5] = i * 4 + 1;
    }
    SDL_UnmapGPUTransferBuffer(device, tbo);
    SDL_GPUCommandBuffer* commands = SDL_AcquireGPUCommandBuffer(device);
    if (!commands) {
        SDL_Log("Failed to acquire command buffer: %s", SDL_GetError());
        return false;
    }
    SDL_GPUCopyPass* pass = SDL_BeginGPUCopyPass(commands);
    if (!pass) {
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