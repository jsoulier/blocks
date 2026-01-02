#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "block.h"
#include "camera.h"
#include "chunk.h"
#include "database.h"
#include "helpers.h"
#include "noise.h"
#include "voxel.h"
#include "world.h"

typedef enum
{
    JOB_TYPE_QUIT,
    JOB_TYPE_LOAD,
    JOB_TYPE_MESH,
}
job_type_t;

typedef struct
{
    job_type_t type;
    int x;
    int z;
}
job_t;

typedef struct
{
    SDL_Thread* thrd;
    SDL_Mutex* mtx;
    SDL_Condition* cnd;
    const job_t* job;
    SDL_GPUTransferBuffer* tbos[CHUNK_TYPE_COUNT];
    uint32_t sizes[CHUNK_TYPE_COUNT];
}
worker_t;

static terrain_t terrain;
static SDL_GPUDevice* device;
static SDL_GPUBuffer* ibo;
static uint32_t ibo_size;
static worker_t workers[WORLD_WORKERS];
static int sorted[WORLD_CHUNKS][2];

static int loop(
    void* args)
{
    assert(args);
    worker_t* worker = args;
    while (true)
    {
        SDL_LockMutex(worker->mtx);
        while (!worker->job)
        {
            SDL_WaitCondition(worker->cnd, worker->mtx);
        }
        if (worker->job->type == JOB_TYPE_QUIT)
        {
            worker->job = NULL;
            SDL_SignalCondition(worker->cnd);
            SDL_UnlockMutex(worker->mtx);
            return 0;
        }
        const int x = terrain.x + worker->job->x;
        const int z = terrain.z + worker->job->z;
        chunk_t* chunk = terrain_get2(&terrain, x, z);
        switch (worker->job->type)
        {
        case JOB_TYPE_LOAD:
            assert(chunk->load);
            assert(chunk->mesh);
            noise_generate(chunk, x, z);
            database_get_blocks(chunk, x, z);
            chunk->load = false;
            break;
        case JOB_TYPE_MESH:
            assert(!chunk->load);
            assert(chunk->mesh);
            chunk_t* neighbors[DIRECTION_2];
            terrain_neighbors2(&terrain, x, z, neighbors);
            chunk->mesh = !voxel_vbo(
                chunk,
                (const chunk_t**) neighbors,
                device,
                worker->tbos,
                worker->sizes);
            break;
        default:
            assert(0);
        }
        worker->job = NULL;
        SDL_SignalCondition(worker->cnd);
        SDL_UnlockMutex(worker->mtx);
    }
    return 0;
}

static void dispatch(
    worker_t* worker,
    const job_t* job)
{
    assert(worker);
    assert(job);
    SDL_LockMutex(worker->mtx);
    assert(!worker->job);
    worker->job = job;
    SDL_SignalCondition(worker->cnd);
    SDL_UnlockMutex(worker->mtx);
}

bool world_init(
    SDL_GPUDevice* handle)
{
    assert(handle);
    device = handle;
    terrain_init(&terrain);
    for (int i = 0; i < WORLD_WORKERS; i++)
    {
        worker_t* worker = &workers[i];
        worker->mtx = SDL_CreateMutex();
        if (!worker->mtx)
        {
            SDL_Log("Failed to create mutex: %s", SDL_GetError());
            return false;
        }
        worker->cnd = SDL_CreateCondition();
        if (!worker->cnd)
        {
            SDL_Log("Failed to create condition variable: %s", SDL_GetError());
            return false;
        }
        worker->thrd = SDL_CreateThread(loop, "worker", worker);
        if (!worker->thrd)
        {
            SDL_Log("Failed to create thread: %s", SDL_GetError());
            return false;
        }
    }
    int i = 0;
    for (int x = 0; x < WORLD_X; x++)
    for (int z = 0; z < WORLD_Z; z++)
    {
        sorted[i][0] = x;
        sorted[i][1] = z;
        i++;
    }
    const int w = WORLD_X / 2;
    const int h = WORLD_Z / 2;
    sort_2d(w, h, sorted, WORLD_CHUNKS);
    return true;
}

void world_free()
{
    for (int i = 0; i < WORLD_WORKERS; i++)
    {
        job_t job;
        job.type = JOB_TYPE_QUIT;
        dispatch(&workers[i], &job);
    }
    for (int i = 0; i < WORLD_WORKERS; i++)
    {
        worker_t* worker = &workers[i];
        SDL_WaitThread(worker->thrd, NULL);
        SDL_DestroyMutex(worker->mtx);
        SDL_DestroyCondition(worker->cnd);
        worker->thrd = NULL;
        worker->mtx = NULL;
        worker->cnd = NULL;
        for (chunk_type_t type = 0; type < CHUNK_TYPE_COUNT; type++)
        {
            if (worker->tbos[type])
            {
                SDL_ReleaseGPUTransferBuffer(device, worker->tbos[type]);
                worker->tbos[type] = NULL;
            }
        }
    }
    for (int x = 0; x < WORLD_X; x++)
    for (int z = 0; z < WORLD_Z; z++)
    {
        chunk_t* chunk = terrain_get(&terrain, x, z);
        for (chunk_type_t type = 0; type < CHUNK_TYPE_COUNT; type++)
        {
            if (chunk->vbos[type])
            {
                SDL_ReleaseGPUBuffer(device, chunk->vbos[type]);
                chunk->vbos[type] = NULL;
            }
        }
    }
    terrain_free(&terrain);
    if (ibo)
    {
        SDL_ReleaseGPUBuffer(device, ibo);
        ibo = NULL;
    }
    device = NULL;
}

static void move(
    const int x,
    const int y,
    const int z)
{
    const int a = x / CHUNK_X - WORLD_X / 2;
    const int c = z / CHUNK_Z - WORLD_Z / 2;
    int size;
    int* data = terrain_move(&terrain, a, c, &size);
    if (!data)
    {
        return;
    }
    for (int i = 0; i < size; i++)
    {
        const int j = data[i * 2 + 0];
        const int k = data[i * 2 + 1];
        chunk_t* chunk = terrain_get(&terrain, j, k);
        memset(chunk->blocks, 0, sizeof(chunk->blocks));
        chunk->load = true;
        chunk->mesh = true;
    }
    free(data);
}

void world_update(
    const int x,
    const int y,
    const int z)
{
    move(x, y, z);
    int n = 0;
    job_t jobs[WORLD_WORKERS];
    for (int i = 0; i < WORLD_CHUNKS && n < WORLD_WORKERS; i++)
    {
        const int j = sorted[i][0];
        const int k = sorted[i][1];
        chunk_t* chunk = terrain_get(&terrain, j, k);
        if (chunk->load)
        {
            job_t* job = &jobs[n++];
            job->type = JOB_TYPE_LOAD;
            job->x = j;
            job->z = k;
            continue;
        }
        if (!chunk->mesh || terrain_border(&terrain, j, k))
        {
            continue;
        }
        bool status = true;
        chunk_t* neighbors[DIRECTION_2];
        terrain_neighbors(&terrain, j, k, neighbors);
        for (direction_t direction = 0; direction < DIRECTION_2; direction++)
        {
            const chunk_t* neighbor = neighbors[direction];
            if (!neighbor || neighbor->load)
            {
                status = false;
                break;
            }
        }
        if (status)
        {
            job_t* job = &jobs[n++];
            job->type = JOB_TYPE_MESH;
            job->x = j;
            job->z = k;
            continue;
        }
    }
    uint32_t size = 0;
    for (int i = 0; i < n; i++)
    {
        dispatch(&workers[i], &jobs[i]);
    }
    for (int i = 0; i < n; i++)
    {
        worker_t* worker = &workers[i];
        SDL_LockMutex(worker->mtx);
        while (worker->job)
        {
            SDL_WaitCondition(worker->cnd, worker->mtx);
        }
        SDL_UnlockMutex(worker->mtx);
    }
    for (int i = 0; i < n; i++)
    {
        const job_t* job = &jobs[i];
        if (job->type != JOB_TYPE_MESH)
        {
            continue;
        }
        chunk_t* chunk = terrain_get(&terrain, job->x, job->z);
        for (chunk_type_t type = 0; type < CHUNK_TYPE_COUNT; type++)
        {
            size = max(size, chunk->sizes[type]);
        }
    }
    if (size > ibo_size)
    {
        if (ibo)
        {
            SDL_ReleaseGPUBuffer(device, ibo);
            ibo = NULL;
            ibo_size = 0;
        }
        if (voxel_ibo(device, &ibo, size))
        {
            ibo_size = size;
        }
    }
}

void world_render(
    const camera_t* camera,
    SDL_GPUCommandBuffer* commands,
    SDL_GPURenderPass* pass,
    const chunk_type_t type)
{
    assert(commands);
    assert(pass);
    if (!ibo)
    {
        return;
    }
    SDL_GPUBufferBinding ibb = {0};
    ibb.buffer = ibo;
    SDL_BindGPUIndexBuffer(pass, &ibb, SDL_GPU_INDEXELEMENTSIZE_32BIT);
    for (int i = 0; i < WORLD_CHUNKS; i++)
    {
        int x;
        int z;
        if (type == CHUNK_TYPE_TRANSPARENT)
        {
            x = sorted[WORLD_CHUNKS - i - 1][0] + terrain.x;
            z = sorted[WORLD_CHUNKS - i - 1][1] + terrain.z;
        }
        else
        {
            x = sorted[i][0] + terrain.x;
            z = sorted[i][1] + terrain.z;
        }
        const chunk_t* chunk = terrain_get2(&terrain, x, z);
        if (terrain_border2(&terrain, x, z) || chunk->mesh || !chunk->sizes[type])
        {
            continue;
        }
        assert(chunk->sizes[type] <= ibo_size);
        x *= CHUNK_X;
        z *= CHUNK_Z;
        if (camera && !camera_test(camera, x, 0, z, CHUNK_X, CHUNK_Y, CHUNK_Z))
        {
            continue;
        }
        int32_t position[3] = { x, 0, z };
        SDL_GPUBufferBinding vbb = {0};
        vbb.buffer = chunk->vbos[type];
        SDL_PushGPUVertexUniformData(commands, 0, position, sizeof(position));
        SDL_BindGPUVertexBuffers(pass, 0, &vbb, 1);
        SDL_DrawGPUIndexedPrimitives(pass, chunk->sizes[type] * 6, 1, 0, 0, 0);
    }
}

void world_set_block(
    int x,
    int y,
    int z,
    const block_t block)
{
    const int a = SDL_floor((float) x / CHUNK_X);
    const int c = SDL_floor((float) z / CHUNK_Z);
    if (!terrain_in2(&terrain, a, c) || y < 0 || y >= CHUNK_Y)
    {
        return;
    }
    chunk_wrap(&x, &y, &z);
    chunk_t* chunk = terrain_get2(&terrain, a, c);
    database_set_block(a, c, x, y, z, block);
    chunk_set_block(chunk, x, y, z, block);
    chunk->mesh = true;
    chunk_t* neighbors[DIRECTION_2];
    terrain_neighbors2(&terrain, a, c, neighbors);
    if (x == 0 && neighbors[DIRECTION_W])
    {
        neighbors[DIRECTION_W]->mesh = true;
    }
    else if (x == CHUNK_X - 1 && neighbors[DIRECTION_E])
    {
        neighbors[DIRECTION_E]->mesh = true;
    }
    if (z == 0 && neighbors[DIRECTION_S])
    {
        neighbors[DIRECTION_S]->mesh = true;
    }
    else if (z == CHUNK_Z - 1 && neighbors[DIRECTION_N])
    {
        neighbors[DIRECTION_N]->mesh = true;
    }
}

block_t world_get_block(
    int x,
    int y,
    int z)
{
    const int a = SDL_floor((float) x / CHUNK_X);
    const int c = SDL_floor((float) z / CHUNK_Z);
    if (!terrain_in2(&terrain, a, c) || y < 0 || y >= CHUNK_Y)
    {
        return BLOCK_EMPTY;
    }
    chunk_wrap(&x, &y, &z);
    const chunk_t* chunk = terrain_get2(&terrain, a, c);
    if (!chunk->load)
    {
        return chunk->blocks[x][y][z];
    }
    else
    {
        return BLOCK_EMPTY;
    }
}
