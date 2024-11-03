#include <SDL3/SDL.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>
#include "block.h"
#include "camera.h"
#include "containers.h"
#include "database.h"
#include "helpers.h"
#include "noise.h"
#include "voxmesh.h"
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
    tag_t tag;
    int x;
    int y;
    int z;
}
job_t;

typedef struct
{
    thrd_t thrd;
    mtx_t mtx;
    cnd_t cnd;
    const job_t* job;
    SDL_GPUTransferBuffer* opaque_tbo;
    SDL_GPUTransferBuffer* transparent_tbo;
    uint32_t opaque_size;
    uint32_t transparent_size;
}
worker_t;

static terrain_t terrain;
static SDL_GPUDevice* device;
static SDL_GPUBuffer* ibo;
static uint32_t ibo_size;
static worker_t workers[WORLD_MAX_WORKERS];
static queue_t queue;
static int sorted[WORLD_Y][WORLD_CHUNKS][3];
static int height;

static void get_neighbors(
    const int x,
    const int y,
    const int z,
    chunk_t* neighbors[DIRECTION_3])
{
    for (direction_t d = 0; d < DIRECTION_3; d++)
    {
        const int a = x + directions[d][0];
        const int b = y + directions[d][1];
        const int c = z + directions[d][2];
        if (terrain_in2(&terrain, a, c) && b >= 0 && b < WORLD_Y)
        {
            neighbors[d] = &terrain_get2(&terrain, a, c)->chunks[b];
        }
        else
        {
            neighbors[d] = NULL;
        }
    }
}

static int loop(void* args)
{
    assert(args);
    worker_t* worker = args;
    while (true)
    {
        mtx_lock(&worker->mtx);
        while (!worker->job)
        {
            cnd_wait(&worker->cnd, &worker->mtx);
        }
        if (worker->job->type == JOB_TYPE_QUIT)
        {
            worker->job = NULL;
            cnd_signal(&worker->cnd);
            mtx_unlock(&worker->mtx);
            return 0;
        }
        const int x = worker->job->x;
        const int y = worker->job->y;
        const int z = worker->job->z;
        group_t* group = terrain_get2(&terrain, x, z);
        switch (worker->job->type)
        {
        case JOB_TYPE_LOAD:
            {
                assert(!group->loaded);
                noise_generate(group, x, z);
                database_get_blocks(group, x, z);
                group->loaded = true;
            }
            break;
        case JOB_TYPE_MESH:
            {
                chunk_t* chunk = &group->chunks[y];
                assert(!chunk->empty);
                chunk_t* neighbors[DIRECTION_3];
                get_neighbors(x, y, z, neighbors);
                if (voxmesh_vbo(
                    chunk,
                    neighbors,
                    y,
                    device,
                    &worker->opaque_tbo,
                    &worker->transparent_tbo,
                    &worker->opaque_size,
                    &worker->transparent_size))
                {
                    chunk->renderable = 1;
                }
            }
            break;
        default:
            assert(0);
        }
        worker->job = NULL;
        cnd_signal(&worker->cnd);
        mtx_unlock(&worker->mtx);
    }
    return 0;
}

static void dispatch(worker_t* worker, const job_t* job)
{
    assert(worker);
    assert(job);
    mtx_lock(&worker->mtx);
    assert(!worker->job);
    worker->job = job;
    cnd_signal(&worker->cnd);
    mtx_unlock(&worker->mtx);
}

static void wait(worker_t* worker)
{
    assert(worker);
    mtx_lock(&worker->mtx);
    while (worker->job)
    {
        cnd_wait(&worker->cnd, &worker->mtx);
    }
    mtx_unlock(&worker->mtx);
}

static void load(
    group_t* group,
    const int x,
    const int z)
{
    assert(group);
    assert(!group->neighbors);
    assert(!group->loaded);
    assert(terrain_in2(&terrain, x, z));
    group_t* neighbors[DIRECTION_2];
    terrain_neighbors2(&terrain, x, z, neighbors);
    for (direction_t c = 0; c < DIRECTION_2; c++)
    {
        const group_t* neighbor = neighbors[c];
        if (neighbor)
        {
            group->neighbors += neighbor->loaded;
        }
    }
    tag_invalidate(&group->tag);
    job_t job;
    job.type = JOB_TYPE_LOAD;
    job.tag = group->tag;
    job.x = x;
    job.y = 0;
    job.z = z;
    if (!queue_add(&queue, &job, false))
    {
        SDL_Log("Failed to add load job");
    }
}

static bool mesh(
    chunk_t* chunk,
    const int x,
    const int y,
    const int z)
{
    assert(chunk);
    assert(terrain_in2(&terrain, x, z));
    if (chunk->empty)
    {
        return true;
    }
    tag_invalidate(&chunk->tag);
    job_t job;
    job.type = JOB_TYPE_MESH;
    job.tag = chunk->tag;
    job.x = x;
    job.y = y;
    job.z = z;
    if (!queue_add(&queue, &job, true))
    {
        SDL_Log("Failed to add mesh job");
        return false;
    }
    return true;
}

static void mesh_all(
    group_t* group,
    const int x,
    const int z)
{
    assert(group);
    assert(group->neighbors >= DIRECTION_2);
    assert(group->loaded);
    assert(terrain_in2(&terrain, x, z));
    if (terrain_border2(&terrain, x, z))
    {
        return;
    }
    for (int y = 0; y < GROUP_CHUNKS; y++)
    {
        chunk_t* chunk = &group->chunks[y];
        if (chunk->renderable)
        {
            continue;
        }
        if (!mesh(chunk, x, y, z))
        {
            return;
        }
    }
}

static bool test(const job_t* job)
{
    assert(job);
    const int x = job->x;
    const int y = job->y;
    const int z = job->z;
    if (!terrain_in2(&terrain, x, z))
    {
        return false;
    }
    const group_t* group = terrain_get2(&terrain, x, z);
    const chunk_t* chunk = &group->chunks[y];
    switch (job->type)
    {
    case JOB_TYPE_LOAD:
        return tag_same(job->tag, group->tag);
    case JOB_TYPE_MESH:
        return tag_same(job->tag, chunk->tag) && !terrain_border2(&terrain, x, z);
    }
    assert(0);
    return false;
}

bool world_init(SDL_GPUDevice* handle)
{
    assert(handle);
    device = handle;
    height = INT32_MAX;
    for (int i = 0; i < WORLD_Y; i++)
    {
        int j = 0;
        for (int x = 0; x < WORLD_X; x++)
        for (int y = 0; y < WORLD_Y; y++)
        for (int z = 0; z < WORLD_Z; z++)
        {
            sorted[i][j][0] = x;
            sorted[i][j][1] = y;
            sorted[i][j][2] = z;
            j++;
        }
        const int w = WORLD_X / 2;
        const int h = WORLD_Z / 2;
        sort_3d(w, i, h, sorted[i], WORLD_CHUNKS);
    }
    terrain_init(&terrain);
    for (int x = 0; x < WORLD_X; x++)
    {
        for (int z = 0; z < WORLD_Z; z++)
        {
            group_t* group = terrain_get(&terrain, x, z);
            for (int i = 0; i < GROUP_CHUNKS; i++)
            {
                chunk_t* chunk = &group->chunks[i];
                tag_init(&chunk->tag);
            }
            tag_init(&group->tag);
        }
    }
    for (int i = 0; i < WORLD_MAX_WORKERS; i++)
    {
        worker_t* worker = &workers[i];
        memset(worker, 0, sizeof(worker_t));
        if (mtx_init(&worker->mtx, mtx_plain) != thrd_success)
        {
            SDL_Log("Failed to create mutex");
            return false;
        }
        if (cnd_init(&worker->cnd) != thrd_success)
        {
            SDL_Log("Failed to create condition variable");
            return false;
        }
        if (thrd_create(&worker->thrd, loop, worker) != thrd_success)
        {
            SDL_Log("Failed to create thread");
            return false;
        }
    }
    queue_init(&queue, WORLD_MAX_JOBS, sizeof(job_t));
    return true;
}

void world_free()
{
    job_t job;
    job.type = JOB_TYPE_QUIT;
    for (int i = 0; i < WORLD_MAX_WORKERS; i++)
    {
        worker_t* worker = &workers[i];
        job.type = JOB_TYPE_QUIT;
        dispatch(worker, &job);
    }
    for (int i = 0; i < WORLD_MAX_WORKERS; i++)
    {
        worker_t* worker = &workers[i];
        thrd_join(worker->thrd, NULL);
        mtx_destroy(&worker->mtx);
        cnd_destroy(&worker->cnd);
        if (worker->opaque_tbo)
        {
            SDL_ReleaseGPUTransferBuffer(device, worker->opaque_tbo);
            worker->opaque_tbo = NULL;
        }
        if (worker->transparent_tbo)
        {
            SDL_ReleaseGPUTransferBuffer(device, worker->transparent_tbo);
            worker->transparent_tbo = NULL;
        }
    }
    queue_free(&queue);
    for (int x = 0; x < WORLD_X; x++)
    {
        for (int z = 0; z < WORLD_Z; z++)
        {
            group_t* group = terrain_get(&terrain, x, z);
            for (int i = 0; i < GROUP_CHUNKS; i++)
            {
                chunk_t* chunk = &group->chunks[i];
                if (chunk->opaque_vbo)
                {
                    SDL_ReleaseGPUBuffer(device, chunk->opaque_vbo);
                    chunk->opaque_vbo = NULL;
                }
                if (chunk->transparent_vbo)
                {
                    SDL_ReleaseGPUBuffer(device, chunk->transparent_vbo);
                    chunk->transparent_vbo = NULL;
                }
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
    ibo_size = 0;
}

static void move(
    const int x,
    const int y,
    const int z)
{
    const int a = x / CHUNK_X - WORLD_X / 2;
    const int b = y / CHUNK_Y;
    const int c = z / CHUNK_Z - WORLD_Z / 2;
    height = clamp(b, 0, WORLD_Y - 1);
    int size;
    int* data = terrain_move(&terrain, a, c, &size);
    if (!data)
    {
        return;
    }
    sort_2d(WORLD_X / 2, WORLD_Z / 2, data, size);
    for (int i = 0; i < size; i++)
    {
        const int j = data[i * 2 + 0];
        const int k = data[i * 2 + 1];
        group_t* group = terrain_get(&terrain, j, k);
        for (int j = 0; j < GROUP_CHUNKS; j++)
        {
            chunk_t* chunk = &group->chunks[j];
            memset(chunk->blocks, 0, sizeof(chunk->blocks));
            chunk->renderable = false;
            chunk->empty = true;
        }
        group->neighbors = 0;
        group->loaded = false;
    }
    for (int i = 0; i < size; i++)
    {
        const int j = data[i * 2 + 0];
        const int k = data[i * 2 + 1];
        assert(terrain_in(&terrain, j, k));
        group_t* group = terrain_get(&terrain, j, k);
        load(group, j + terrain.x, k + terrain.z);
    }
    free(data);
}

static void on_load(
    group_t* group,
    const int x,
    const int z)
{
    assert(group);
    group_t* neighbors[DIRECTION_2];
    terrain_neighbors2(&terrain, x, z, neighbors);
    int i = 0;
    for (direction_t d = 0; d < DIRECTION_2; d++)
    {
        group_t* neighbor = neighbors[d];
        if (!neighbor)
        {
            continue;
        }
        neighbor->neighbors++;
        if (!neighbor->loaded)
        {
            continue;
        }
        i++;
        if (neighbor->neighbors < DIRECTION_2)
        {
            continue;
        }
        const int a = x + directions[d][0];
        const int b = z + directions[d][2];
        mesh_all(neighbor, a, b);
    }
    if (i >= DIRECTION_2)
    {
        group->neighbors = i;
        mesh_all(group, x, z);
    }
}

void world_update(
    const int x,
    const int y,
    const int z)
{
    uint32_t n;
    job_t data[WORLD_MAX_WORKERS];
    uint32_t size = 0;
    for (n = 0; n < WORLD_MAX_WORKERS;)
    {
        if (!queue_remove(&queue, &data[n]))
        {
            break;
        }
        if (!test(&data[n]))
        {
            continue;
        }
        n++;
    }
    for (int i = 0; i < n; i++)
    {
        dispatch(&workers[i], &data[i]);
    }
    for (int i = 0; i < WORLD_MAX_WORKERS; i++)
    {
        wait(&workers[i]);
    }
    for (int i = 0; i < n; i++)
    {
        const job_t* job = &data[i];
        group_t* group = terrain_get2(&terrain, job->x, job->z);
        switch (job->type)
        {
        case JOB_TYPE_LOAD:
            on_load(group, job->x, job->z);
            break;
        case JOB_TYPE_MESH:
            size = max3(
                size,
                group->chunks[job->y].opaque_size,
                group->chunks[job->y].transparent_size);
            break;
        default:
            assert(0);
        }
    }
    move(x, y, z);
    if (size > ibo_size)
    {
        if (ibo)
        {
            SDL_ReleaseGPUBuffer(device, ibo);
            ibo = NULL;
            ibo_size = 0;
        }
        if (voxmesh_ibo(device, &ibo, size))
        {
            ibo_size = size;
        }
    }
}

void world_render(
    const camera_t* camera,
    SDL_GPUCommandBuffer* commands,
    SDL_GPURenderPass* pass,
    const bool opaque)
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
        const int j = opaque ? i : WORLD_CHUNKS - i - 1;
        int x = sorted[height][j][0] + terrain.x;
        int y = sorted[height][j][1];
        int z = sorted[height][j][2] + terrain.z;
        if (terrain_border2(&terrain, x, z))
        {
            continue;
        }
        const group_t* group = terrain_get2(&terrain, x, z);
        if (!group->loaded)
        {
            continue;
        }
        const chunk_t* chunk = &group->chunks[y];
        if (!chunk->renderable)
        {
            continue;
        }
        SDL_GPUBuffer* vbo;
        uint32_t size;
        if (opaque)
        {
            vbo = chunk->opaque_vbo;
            size = chunk->opaque_size;
        }
        else
        {
            vbo = chunk->transparent_vbo;
            size = chunk->transparent_size;
        }
        if (!size)
        {
            continue;
        }
        x *= CHUNK_X;
        y *= CHUNK_Y;
        z *= CHUNK_Z;
        if (camera && !camera_test(camera, x, y, z, CHUNK_X, CHUNK_Y, CHUNK_Z))
        {
            continue;
        }
        int32_t position[3] = { x, y, z };
        SDL_PushGPUVertexUniformData(commands, 0, position, sizeof(position));
        SDL_GPUBufferBinding vbb = {0};
        vbb.buffer = vbo;
        SDL_BindGPUVertexBuffers(pass, 0, &vbb, 1);
        assert(size <= ibo_size);
        SDL_DrawGPUIndexedPrimitives(pass, size * 6, 1, 0, 0, 0);
    }
}

void world_set_block(
    const int x,
    const int y,
    const int z,
    const block_t block)
{
    const int a = floor((float) x / CHUNK_X);
    const int c = floor((float) z / CHUNK_Z);
    if (!terrain_in2(&terrain, a, c) || y < 0 || y >= GROUP_Y)
    {
        return;
    }
    const int b = y % CHUNK_Y;
    int d = x;
    int w = 0;
    int f = z;
    chunk_wrap(&d, &w, &f);
    group_t* group = terrain_get2(&terrain, a, c);
    database_set_block(a, c, d, y, f, block);
    group_set_block(group, d, y, f, block);
    const int e = y / CHUNK_Y;
    chunk_t* chunk = &group->chunks[e];
    mesh(chunk, a, e, c);
    chunk_t* neighbors[DIRECTION_3];
    get_neighbors(a, e, c, neighbors);
    if (d == 0 && neighbors[DIRECTION_W])
    {
        mesh(neighbors[DIRECTION_W], a - 1, e, c);
    }
    else if (d == CHUNK_X - 1 && neighbors[DIRECTION_E])
    {
        mesh(neighbors[DIRECTION_E], a + 1, e, c);
    }
    if (f == 0 && neighbors[DIRECTION_S])
    {
        mesh(neighbors[DIRECTION_S], a, e, c - 1);
    }
    else if (f == CHUNK_Z - 1 && neighbors[DIRECTION_N])
    {
        mesh(neighbors[DIRECTION_N], a, e, c + 1);
    }
    if (b == 0 && neighbors[DIRECTION_D])
    {
        mesh(neighbors[DIRECTION_D], a, e - 1, c);
    }
    else if (b == CHUNK_Y - 1 && neighbors[DIRECTION_U])
    {
        mesh(neighbors[DIRECTION_U], a, e + 1, c);
    }
}

block_t world_get_block(
    const int x,
    const int y,
    const int z)
{
    const int a = floor((float) x / CHUNK_X);
    const int c = floor((float) z / CHUNK_Z);
    if (!terrain_in2(&terrain, a, c) || y < 0 || y >= GROUP_Y)
    {
        return BLOCK_EMPTY;
    }
    int d = x;
    int w = 0;
    int f = z;
    chunk_wrap(&d, &w, &f);
    const group_t* group = terrain_get2(&terrain, a, c);
    return group_get_group(group, d, y, f);
}