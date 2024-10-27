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
#include "helpers.h"
#include "noise.h"
#include "voxmesh.h"
#include "world.h"

#define MAX_JOBS 1000
#define MAX_WORKERS 4

typedef enum {
    JOB_TYPE_QUIT,
    JOB_TYPE_LOAD,
    JOB_TYPE_MESH,
} job_type_t;

typedef struct {
    job_type_t type;
    tag_t tag;
    int32_t x;
    int32_t y;
    int32_t z;
} job_t;

typedef struct {
    thrd_t thrd;
    mtx_t mtx;
    cnd_t cnd;
    const job_t* job;
    SDL_GPUTransferBuffer* tbo;
    uint32_t size;
} worker_t;

static SDL_GPUDevice* device;
static SDL_GPUBuffer* ibo;
static uint32_t capacity;
static grid_t grid;
static worker_t workers[MAX_WORKERS];
static ring_t jobs;
static int sorted[WORLD_Y][WORLD_CHUNKS][3];
static int height;

////////////////////////////////////////////////////////////////////////////////
// Worker Implementation  //////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

static int loop(void* args)
{
    worker_t* worker = args;
    while (true) {
        mtx_lock(&worker->mtx);
        while (!worker->job) {
            cnd_wait(&worker->cnd, &worker->mtx);
        }
        if (worker->job->type == JOB_TYPE_QUIT) {
            worker->job = NULL;
            cnd_signal(&worker->cnd);
            mtx_unlock(&worker->mtx);
            return 0;
        }
        const int32_t x = worker->job->x;
        const int32_t y = worker->job->y;
        const int32_t z = worker->job->z;
        group_t* group = grid_get2(&grid, x, z);
        switch (worker->job->type) {
        case JOB_TYPE_LOAD:
            assert(!group->loaded);
            noise_generate(group, x, z);
            group->loaded = true;
            break;
        case JOB_TYPE_MESH:
            chunk_t* chunk = &group->chunks[y];
            assert(!chunk->empty);
            chunk_t* neighbors[DIRECTION_3] = {0};
            for (direction_t d = 0; d < DIRECTION_3; d++) {
                const int32_t a = x + directions[d][0];
                const int32_t b = y + directions[d][1];
                const int32_t c = z + directions[d][2];
                if (grid_in2(&grid, a, c) && y >= 0 && y < WORLD_Y) {
                    group_t* neighbor = grid_get2(&grid, x, z);
                    neighbors[d] = &neighbor->chunks[b];
                }
            }
            if (voxmesh_vbo(chunk, neighbors, device,
                    &worker->tbo, &worker->size)) {
                chunk->renderable = 1;
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
    mtx_lock(&worker->mtx);
    assert(!worker->job);
    worker->job = job;
    cnd_signal(&worker->cnd);
    mtx_unlock(&worker->mtx);
}

static void wait(worker_t* worker)
{
    mtx_lock(&worker->mtx);
    while (worker->job) {
        cnd_wait(&worker->cnd, &worker->mtx);
    }
    mtx_unlock(&worker->mtx);
}

////////////////////////////////////////////////////////////////////////////////
// Job Helpers /////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

static void load(
    group_t* group,
    const int32_t x,
    const int32_t z)
{
    assert(group);
    assert(!group->neighbors);
    assert(!group->loaded);
    assert(grid_in2(&grid, x, z));
    group_t* neighbors[DIRECTION_2];
    grid_neighbors2(&grid, x, z, neighbors);
    for (direction_t c = 0; c < DIRECTION_2; c++) {
        const group_t* neighbor = neighbors[c];
        group->neighbors += neighbor && neighbor->loaded;
    }
    tag_invalidate(&group->tag);
    job_t job;
    job.type = JOB_TYPE_LOAD;
    job.tag = group->tag;
    job.x = x;
    job.y = 0;
    job.z = z;
    if (!ring_add(&jobs, &job, false)) {
        SDL_Log("Failed to add load job");
    }
}

static bool mesh(
    chunk_t* chunk,
    const int32_t x,
    const int32_t y,
    const int32_t z)
{
    assert(chunk);
    assert(grid_in2(&grid, x, z));
    tag_invalidate(&chunk->tag);
    job_t job;
    job.type = JOB_TYPE_MESH;
    job.tag = chunk->tag;
    job.x = x;
    job.y = y;
    job.z = z;
    if (!ring_add(&jobs, &job, true)) {
        SDL_Log("Failed to add mesh job");
        return false;
    }
    return true;
}

static void mesh_all(
    group_t* group,
    const int32_t x,
    const int32_t z)
{
    assert(group);
    assert(group->neighbors >= DIRECTION_2);
    assert(group->loaded);
    assert(grid_in2(&grid, x, z));
    if (grid_border2(&grid, x, z)) {
        return;
    }
    for (int y = 0; y < GROUP_CHUNKS; y++) {
        chunk_t* chunk = &group->chunks[y];
        if (chunk->empty || chunk->renderable) {
            continue;
        }
        if (!mesh(chunk, x, y, z)) {
            return;
        }
    }
}

static bool test(const job_t* job)
{
    assert(job);
    const int32_t x = job->x;
    const int32_t y = job->y;
    const int32_t z = job->z;
    if (!grid_in2(&grid, x, z)) {
        return false;
    }
    const group_t* group = grid_get2(&grid, x, z);
    const chunk_t* chunk = &group->chunks[y];
    switch (job->type) {
    case JOB_TYPE_LOAD:
        return tag_same(job->tag, group->tag);
    case JOB_TYPE_MESH:
        return tag_same(job->tag, chunk->tag) && !grid_border2(&grid, x, z);
    }
    assert(0);
    return false;
}

////////////////////////////////////////////////////////////////////////////////
// World Implementation ////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

bool world_init(SDL_GPUDevice* handle)
{
    assert(handle);
    device = handle;
    height = INT32_MAX;
    for (int i = 0; i < WORLD_Y; i++) {
        int j = 0;
        for (int x = 0; x < WORLD_X; x++)
        for (int y = 0; y < WORLD_Y; y++)
        for (int z = 0; z < WORLD_Z; z++) {
            sorted[i][j][0] = x;
            sorted[i][j][1] = y;
            sorted[i][j][2] = z;
            j++;
        }
        const int w = WORLD_X / 2;
        const int h = WORLD_Z / 2;
        sort_3d(w, i, h, sorted[i], WORLD_CHUNKS, true);
    }
    grid_init(&grid, WORLD_X, WORLD_Z);
    for (int x = 0; x < WORLD_X; x++) {
        for (int z = 0; z < WORLD_Z; z++) {
            group_t* group = calloc(1, sizeof(group_t));
            if (!group) {
                SDL_Log("Failed to allocate group");
                return false;
            }
            grid_set(&grid, x, z, group);
            for (int i = 0; i < GROUP_CHUNKS; i++) {
                chunk_t* chunk = &group->chunks[i];
                tag_init(&chunk->tag);
            }
            tag_init(&group->tag);
        }
    }
    for (int i = 0; i < MAX_WORKERS; i++) {
        worker_t* worker = &workers[i];
        memset(worker, 0, sizeof(worker_t));
        if (mtx_init(&worker->mtx, mtx_plain) != thrd_success) {
            SDL_Log("Failed to create mutex");
            return false;
        }
        if (cnd_init(&worker->cnd) != thrd_success) {
            SDL_Log("Failed to create condition variable");
            return false;
        }
        if (thrd_create(&worker->thrd, loop, worker) != thrd_success) {
            SDL_Log("Failed to create thread");
            return false;
        }
    }
    ring_init(&jobs, MAX_JOBS, sizeof(job_t));
    world_update(0, 0, 0);
    return true;
}

void world_free()
{
    job_t job;
    job.type = JOB_TYPE_QUIT;
    for (int i = 0; i < MAX_WORKERS; i++) {
        worker_t* worker = &workers[i];
        job.type = JOB_TYPE_QUIT;
        dispatch(worker, &job);
    }
    for (int i = 0; i < MAX_WORKERS; i++) {
        worker_t* worker = &workers[i];
        thrd_join(worker->thrd, NULL);
        if (worker->tbo) {
            SDL_ReleaseGPUTransferBuffer(device, worker->tbo);
            worker->tbo = NULL;
        }
    }
    ring_free(&jobs);
    for (int x = 0; x < WORLD_X; x++) {
        for (int z = 0; z < WORLD_Z; z++) {
            group_t* group = grid_get(&grid, x, z);
            for (int i = 0; i < GROUP_CHUNKS; i++) {
                chunk_t* chunk = &group->chunks[i];
                if (chunk->vbo) {
                    SDL_ReleaseGPUBuffer(device, chunk->vbo);
                    chunk->vbo = NULL;
                }
            }
        }
    }
    grid_free(&grid);
    if (ibo) {
        SDL_ReleaseGPUBuffer(device, ibo);
        ibo = NULL;
    }
    device = NULL;
    capacity = 0;
}

static void move(
    const int32_t x,
    const int32_t y,
    const int32_t z)
{
    const int a = x / CHUNK_X - WORLD_X / 2;
    const int b = y / CHUNK_Y;
    const int c = z / CHUNK_Z - WORLD_Z / 2;
    height = clamp(b, 0, WORLD_Y - 1);
    int size;
    int* data = grid_move(&grid, a, c, &size);
    if (!data) {
        return;
    }
    sort_2d(WORLD_X / 2, WORLD_Z / 2, data, size, true);
    for (int i = 0; i < size; i++) {
        const int j = data[i * 2 + 0];
        const int k = data[i * 2 + 1];
        group_t* group = grid_get(&grid, j, k);
        for (int j = 0; j < GROUP_CHUNKS; j++) {
            chunk_t* chunk = &group->chunks[j];
            memset(chunk->blocks, 0, sizeof(chunk->blocks));
            chunk->renderable = false;
            chunk->empty = true;
        }
        group->neighbors = 0;
        group->loaded = false;
    }
    for (int i = 0; i < size; i++) {
        const int j = data[i * 2 + 0];
        const int k = data[i * 2 + 1];
        assert(in_world(j, 0, k));
        group_t* group = grid_get(&grid, j, k);
        load(group, j + grid.x, k + grid.y);
    }
    free(data);
}

static void on_load(
    group_t* group,
    const int32_t x,
    const int32_t z)
{
    assert(group);
    group_t* neighbors[DIRECTION_2];
    grid_neighbors2(&grid, x, z, neighbors);
    int i = 0;
    for (direction_t d = 0; d < DIRECTION_2; d++) {
        group_t* neighbor = neighbors[d];
        if (!neighbor) {
            continue;
        }
        neighbor->neighbors++;
        if (!neighbor->loaded) {
            continue;
        }
        i++;
        if (neighbor->neighbors < DIRECTION_2) {
            continue;
        }
        const int32_t a = x + directions[d][0];
        const int32_t b = z + directions[d][2];
        mesh_all(neighbor, a, b);
    }
    if (i >= DIRECTION_2) {
        mesh_all(group, x, z);
    }
}

void world_update(
    const int32_t x,
    const int32_t y,
    const int32_t z)
{
    uint32_t n;
    job_t data[MAX_WORKERS];
    uint32_t size = 0;
    for (n = 0; n < MAX_WORKERS;) {
        if (!ring_remove(&jobs, &data[n])) {
            break;
        }
        if (!test(&data[n])) {
            continue;
        }
        n++;
    }
    for (int i = 0; i < n; i++) {
        dispatch(&workers[i], &data[i]);
    }
    for (int i = 0; i < MAX_WORKERS; i++) {
        wait(&workers[i]);
    }
    for (int i = 0; i < n; i++) {
        const job_t* job = &data[i];
        group_t* group = grid_get2(&grid, job->x, job->z);
        switch (job->type) {
        case JOB_TYPE_LOAD:
            on_load(group, job->x, job->z);
            break;
        case JOB_TYPE_MESH:
            const chunk_t* chunk = &group->chunks[job->y];
            size = max(size, chunk->size);
            break;
        default:
            assert(0);
        }
    }
    move(x, y, z);
    if (size > capacity) {
        if (ibo) {
            SDL_ReleaseGPUBuffer(device, ibo);
            ibo = NULL;
            capacity = 0;
        }
        if (voxmesh_ibo(device, &ibo, size)) {
            capacity = size;
        }
    }
}

void world_render(
    const camera_t* camera,
    SDL_GPUCommandBuffer* commands,
    SDL_GPURenderPass* pass)
{
    if (!ibo) {
        return;
    }
    SDL_GPUBufferBinding ibb = {0};
    ibb.buffer = ibo;
    SDL_BindGPUIndexBuffer(pass, &ibb, SDL_GPU_INDEXELEMENTSIZE_32BIT);
    for (int i = 0; i < WORLD_CHUNKS; i++) {
        int x = sorted[height][i][0] + grid.x;
        int y = sorted[height][i][1];
        int z = sorted[height][i][2] + grid.y;
        if (grid_border2(&grid, x, z)) {
            continue;
        }
        const group_t* group = grid_get2(&grid, x, z);
        if (!group->loaded) {
            continue;
        }
        const chunk_t* chunk = &group->chunks[y];
        if (!chunk->renderable) {
            continue;
        }
        x *= CHUNK_X;
        y *= CHUNK_Y;
        z *= CHUNK_Z;
        if (!camera_test(camera, x, y, z, CHUNK_X, CHUNK_Y, CHUNK_Z)) {
            continue;
        }
        int32_t position[3] = { x, y, z };
        SDL_PushGPUVertexUniformData(commands, 1, position, 12);
        SDL_GPUBufferBinding vbb = {0};
        vbb.buffer = chunk->vbo;
        SDL_BindGPUVertexBuffers(pass, 0, &vbb, 1);
        assert(chunk->size <= capacity);
        SDL_DrawGPUIndexedPrimitives(pass, chunk->size * 6, 1, 0, 0, 0);
    }
}

block_t world_get_block(
    const int32_t x,
    const int32_t y,
    const int32_t z)
{
    int a = floor((float) x / CHUNK_X);
    int c = floor((float) z / CHUNK_Z);
    int d = wrap_x(x);
    int f = wrap_z(z);
    const group_t* group = grid_get2(&grid, a, c);
    return get_chunk_block_from_group(group, d, y, f);
}

void world_set_block(
    const int32_t x,
    const int32_t y,
    const int32_t z,
    const block_t block)
{
    int a = floor((float) x / CHUNK_X);
    int c = floor((float) z / CHUNK_Z);
    int d = wrap_x(x);
    int f = wrap_z(z);
    group_t* group = grid_get2(&grid, a, c);
    set_block_in_group(group, d, y, f, block);
    const int e = y / CHUNK_Y;
    chunk_t* chunk = &group->chunks[e];
    mesh(chunk, a, e, c);
}