#include <SDL3/SDL_error.h>
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_log.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <threads.h>
#include "block.h"
#include "camera.h"
#include "containers.h"
#include "helpers.h"
#include "voxmesh.h"
#include "world.h"

#define MAX_JOBS 1000
#define MAX_WORKERS 4

typedef enum 
{
    JOB_TYPE_QUIT,
    JOB_TYPE_LOAD,
    JOB_TYPE_MESH,
} job_type_t;

typedef struct
{
    job_type_t type;
    tag_t tag;
    int32_t x;
    int32_t y;
    int32_t z;
} job_t;

typedef struct
{
    thrd_t thrd;
    mtx_t mtx;
    cnd_t cnd;
    const job_t* job;
    SDL_GPUTransferBuffer* tbo;
    uint32_t size;
} worker_t;

static worker_t workers[MAX_WORKERS];
static ring_t jobs;
static grid_t groups;
static int32_t wx;
static int32_t wy;
static int32_t wz;
static SDL_GPUDevice* device;
static SDL_GPUBuffer* ibo;
static uint32_t capacity;
static int presort[WORLD_Y][WORLD_CHUNKS][3];
static int sort[WORLD_GROUPS][2];

static void load(worker_t* worker)
{
    const int32_t x = worker->job->x;
    const int32_t z = worker->job->z;
    group_t* group = world_get_group(x, z);
    assert(!group->loaded);
    noise_generate(group, x, z);
    group->loaded = true;
}

static void mesh(worker_t* worker)
{
    const int32_t x = worker->job->x;
    const int32_t y = worker->job->y;
    const int32_t z = worker->job->z;
    chunk_t* chunk = world_get_chunk(x, y, z);
    assert(!chunk->renderable);
    assert(!chunk->empty);
    chunk_t* neighbors[DIRECTION_3];
    world_get_chunk_neighbors(x, y, z, neighbors);
    void* data = worker->tbo;
    if (data)
    {
        data = SDL_MapGPUTransferBuffer(device, worker->tbo, true);
        if (!data)
        {
            SDL_Log("Failed to map worker transfer buffer: %s", SDL_GetError());
            return;
        }
    }
    chunk->size = voxmesh_make(chunk, neighbors, data, worker->size);
    if (data)
    {
        SDL_UnmapGPUTransferBuffer(device, worker->tbo);
    }
    if (!chunk->size)
    {
        return;
    }
    if (chunk->size > worker->size)
    {
        if (worker->tbo)
        {
            SDL_ReleaseGPUTransferBuffer(device, worker->tbo);
            worker->size = 0;
        }
        SDL_GPUTransferBufferCreateInfo tbci = {0};
        tbci.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        tbci.size = chunk->size * 16;
        worker->tbo = SDL_CreateGPUTransferBuffer(device, &tbci);
        if (!worker->tbo)
        {
            SDL_Log("Failed to create worker transfer buffer: %s", SDL_GetError());
            return;
        }
        worker->size = chunk->size;
        data = SDL_MapGPUTransferBuffer(device, worker->tbo, true);
        if (!data)
        {
            SDL_Log("Failed to map worker transfer buffer: %s", SDL_GetError());
            return;
        }
        chunk->size = voxmesh_make(chunk, neighbors, data, worker->size);
        SDL_UnmapGPUTransferBuffer(device, worker->tbo);
    }
    if (chunk->size > chunk->capacity)
    {
        if (chunk->vbo)
        {
            SDL_ReleaseGPUBuffer(device, chunk->vbo);
            chunk->size = 0;
        }
        SDL_GPUBufferCreateInfo bci = {0};
        bci.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
        bci.size = chunk->size * 16;
        chunk->vbo = SDL_CreateGPUBuffer(device, &bci);
        if (!chunk->vbo)
        {
            SDL_Log("Failed to create chunk buffer: %s", SDL_GetError());
            return;
        }
        chunk->capacity = chunk->size;
    }
    SDL_GPUCommandBuffer* commands = SDL_AcquireGPUCommandBuffer(device);
    if (!commands)
    {
        SDL_Log("Failed to acquire worker command buffer: %s", SDL_GetError());
        return;
    }
    SDL_GPUCopyPass* pass = SDL_BeginGPUCopyPass(commands);
    if (!pass)
    {
        SDL_Log("Failed to begin worker copy pass: %s", SDL_GetError());
        return;
    }
    SDL_GPUTransferBufferLocation tbl = {0};
    tbl.transfer_buffer = worker->tbo;
    SDL_GPUBufferRegion region = {0};
    region.size = chunk->size * 16;
    region.buffer = chunk->vbo;
    SDL_UploadToGPUBuffer(pass, &tbl, &region, 1);
    SDL_EndGPUCopyPass(pass);
    SDL_SubmitGPUCommandBuffer(commands);
    chunk->renderable = 1;
}

static int func(void* args)
{
    int loop = 1;
    worker_t* worker = args;
    while (loop)
    {
        mtx_lock(&worker->mtx);
        while (!worker->job)
        {
            cnd_wait(&worker->cnd, &worker->mtx);
        }
        switch (worker->job->type)
        {
        case JOB_TYPE_QUIT:
            loop = 0;
            break;
        case JOB_TYPE_LOAD:
            load(worker);
            break;
        case JOB_TYPE_MESH:
            mesh(worker);
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

bool worker_init()
{
    for (int i = 0; i < MAX_WORKERS; i++)
    {
        worker_t* worker = &workers[i];
        worker->job = NULL;
        worker->tbo = NULL;
        worker->size = 0;
        if (mtx_init(&worker->mtx, mtx_plain) != thrd_success)
        {
            SDL_Log("Failed to create worker mutex");
            return false;
        }
        if (cnd_init(&worker->cnd) != thrd_success)
        {
            SDL_Log("Failed to create worker condition variable");
            return false;
        }
        if (thrd_create(&worker->thrd, func, worker) != thrd_success)
        {
            SDL_Log("Failed to create worker thread");
            return false;
        }
    }
    ring_init(&jobs, MAX_JOBS, sizeof(job_t));
    return true;
}

static void work(
    worker_t* worker,
    const job_t* job)
{
    assert(worker);
    assert(job);
    mtx_lock(&worker->mtx);
    assert(!worker->job);
    worker->job = job;
    cnd_signal(&worker->cnd);
    mtx_unlock(&worker->mtx);
}

void worker_free()
{
    job_t job;
    job.type = JOB_TYPE_QUIT;
    for (int i = 0; i < MAX_WORKERS; i++)
    {
        worker_t* worker = &workers[i];
        job.type = JOB_TYPE_QUIT;
        work(worker, &job);
    }
    for (int i = 0; i < MAX_WORKERS; i++)
    {
        worker_t* worker = &workers[i];
        thrd_join(worker->thrd, NULL);
        if (worker->tbo)
        {
            SDL_ReleaseGPUTransferBuffer(device, worker->tbo);
            worker->tbo = NULL;
        }
    }
    ring_free(&jobs);
}

void worker_load(
    group_t* group,
    const int32_t x,
    const int32_t z)
{
    assert(group);
    assert(!group->neighbors);
    assert(!group->loaded);
    assert(world_in(x, 0, z));
    group_t* neighbors[DIRECTION_2];
    world_get_group_neighbors(x, z, neighbors);
    for (direction_t dir = 0; dir < DIRECTION_2; dir++)
    {
        const group_t* neighbor = neighbors[dir];
        group->neighbors += neighbor && neighbor->loaded;
    }
    tag_invalidate(&group->tag);
    job_t job;
    job.type = JOB_TYPE_LOAD;
    job.tag = group->tag;
    job.x = x;
    job.y = 0;
    job.z = z;
    if (!ring_add(&jobs, &job, false))
    {
        SDL_Log("Failed to add load job");
    }
}

void worker_mesh(
    group_t* group,
    const int32_t x,
    const int32_t z)
{
    assert(group);
    assert(group->neighbors >= DIRECTION_2);
    assert(group->loaded);
    assert(world_in(x, 0, z));
    if (world_on_border(x, 1, z))
    {
        return;
    }
    for (int y = 0; y < GROUP_CHUNKS; y++)
    {
        chunk_t* chunk = &group->chunks[y];
        if (chunk->empty || chunk->renderable)
        {
            continue;
        }
        tag_invalidate(&chunk->tag);
        job_t job;
        job.type = JOB_TYPE_MESH;
        job.tag = chunk->tag;
        job.x = x;
        job.y = y;
        job.z = z;
        if (!ring_add(&jobs, &job, true))
        {
            SDL_Log("Failed to add mesh job");
            break;
        }
    }
}

static bool should_work(const job_t* job)
{
    assert(job);
    const int32_t x = job->x;
    const int32_t y = job->y;
    const int32_t z = job->z;
    if (!world_in(x, y, z))
    {
        return false;
    }
    const group_t* group = world_get_group(x, z);
    const chunk_t* chunk = &group->chunks[y];
    switch (job->type)
    {
    case JOB_TYPE_LOAD:
        return tag_same(job->tag, group->tag);
    case JOB_TYPE_MESH:
        return tag_same(job->tag, chunk->tag) && !world_on_border(x, 1, z);
    }
    assert(0);
    return false;
}

static void post_mesh(
    group_t* group,
    const int32_t x,
    const int32_t z)
{
    assert(group);
    group_t* neighbors[DIRECTION_2];
    world_get_group_neighbors(x, z, neighbors);
    int i = 0;
    for (direction_t direction = 0; direction < DIRECTION_2; direction++)
    {
        group_t* neighbor = neighbors[direction];
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
        const int32_t a = x + directions[direction][0];
        const int32_t b = z + directions[direction][2];
        worker_mesh(neighbor, a, b);
    }
    if (i >= DIRECTION_2)
    {
        worker_mesh(group, x, z);
    }
}

uint32_t worker_update()
{
    uint32_t load;
    job_t data[MAX_WORKERS];
    uint32_t buffer = 0;
    for (load = 0; load < MAX_WORKERS;)
    {
        if (!ring_remove(&jobs, &data[load]))
        {
            break;
        }
        if (!should_work(&data[load]))
        {
            continue;
        }
        load++;
    }
    for (int i = 0; i < load; i++)
    {
        work(&workers[i], &data[i]);
    }
    for (int i = 0; i < MAX_WORKERS; i++)
    {
        worker_t* worker = &workers[i];
        mtx_lock(&worker->mtx);
        while (worker->job)
        {
            cnd_wait(&worker->cnd, &worker->mtx);
        }
        mtx_unlock(&worker->mtx);
    }
    for (int i = 0; i < load; i++)
    {
        const job_t* job = &data[i];
        group_t* group = world_get_group(job->x, job->z);
        switch (job->type)
        {
        case JOB_TYPE_LOAD:
            post_mesh(group, job->x, job->z);
            break;
        case JOB_TYPE_MESH:
            const chunk_t* chunk = world_get_chunk(job->x, job->y, job->z);
            if (buffer < chunk->size)
            {
                buffer = chunk->size;
            }
            break;
        default:
            assert(0);
        }
    }
    return buffer;
}

bool world_init(void* handle)
{
    assert(handle);
    device = handle;
    wx = INT32_MAX;
    wy = INT32_MAX;
    wz = INT32_MAX;
    for (int i = 0; i < WORLD_Y; i++)
    {
        int j = 0;
        for (int x = 0; x < WORLD_X; x++)
        for (int y = 0; y < WORLD_Y; y++)
        for (int z = 0; z < WORLD_Z; z++)
        {
            presort[i][j][0] = x;
            presort[i][j][1] = y;
            presort[i][j][2] = z;
            j++;
        }
        sort_3d(WORLD_X / 2, i, WORLD_Z / 2, presort[i], WORLD_CHUNKS, true);
    }
    grid_init(&groups, WORLD_X, WORLD_Z);
    for (int x = 0; x < WORLD_X; x++)
    for (int z = 0; z < WORLD_Z; z++)
    {
        group_t* group = malloc(sizeof(group_t));
        if (!group)
        {
            SDL_Log("Failed to allocate group");
            return false;
        }
        grid_set(&groups, x, z, group);
        for (int i = 0; i < GROUP_CHUNKS; i++)
        {
            chunk_t* chunk = &group->chunks[i];
            tag_init(&chunk->tag);
            chunk->vbo = NULL;
            chunk->size = 0;
            chunk->capacity = 0;
        }
        tag_init(&group->tag);
    }
    if (!worker_init())
    {
        SDL_Log("Failed to initialize workers");
        return false;
    }
    world_move(0, 0, 0);
    return true;
}

void world_free()
{
    worker_free();
    for (int x = 0; x < WORLD_X; x++)
    for (int z = 0; z < WORLD_Z; z++)
    {
        group_t* group = grid_get(&groups, x, z);
        for (int i = 0; i < GROUP_CHUNKS; i++)
        {
            chunk_t* chunk = &group->chunks[i];
            if (chunk->vbo)
            {
                SDL_ReleaseGPUBuffer(device, chunk->vbo);
                chunk->vbo = NULL;
            }
        }
    }
    grid_free(&groups);
    if (ibo)
    {
        SDL_ReleaseGPUBuffer(device, ibo);
        ibo = NULL;
    }
    device = NULL;
    capacity = 0;
}

void world_update()
{
    const uint32_t size = worker_update();
    if (size <= capacity)
    {
        return;
    }
    if (ibo)
    {
        SDL_ReleaseGPUBuffer(device, ibo);
        ibo = NULL;
        capacity = 0;
    }
    SDL_GPUTransferBufferCreateInfo tbci = {0};
    tbci.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    tbci.size = size * 24;
    SDL_GPUTransferBuffer* buffer = SDL_CreateGPUTransferBuffer(device, &tbci);
    if (!buffer)
    {
        SDL_Log("Failed to create world transfer buffer: %s", SDL_GetError());
        return;
    }
    SDL_GPUBufferCreateInfo bci = {0};
    bci.usage = SDL_GPU_BUFFERUSAGE_INDEX;
    bci.size = size * 24;
    ibo = SDL_CreateGPUBuffer(device, &bci);
    if (!ibo)
    {
        SDL_Log("Failed to create world index buffer: %s", SDL_GetError());
        return;
    }
    capacity = size;
    uint32_t* data = SDL_MapGPUTransferBuffer(device, buffer, false);
    if (!data)
    {
        SDL_Log("Failed to map world transfer buffer: %s", SDL_GetError());
        return;
    }
    voxmesh_indices(data, size);
    SDL_UnmapGPUTransferBuffer(device, buffer);
    SDL_GPUCommandBuffer* commands = SDL_AcquireGPUCommandBuffer(device);
    if (!commands)
    {
        SDL_Log("Failed to acquire world command buffer: %s", SDL_GetError());
        return;
    }
    SDL_GPUCopyPass* pass = SDL_BeginGPUCopyPass(commands);
    if (!pass)
    {
        SDL_Log("Failed to begin world copy pass: %s", SDL_GetError());
        return;
    }
    SDL_GPUTransferBufferLocation location = {0};
    location.transfer_buffer = buffer;
    SDL_GPUBufferRegion region = {0};
    region.size = size * 24;
    region.buffer = ibo;
    SDL_UploadToGPUBuffer(pass, &location, &region, 1);
    SDL_EndGPUCopyPass(pass);
    SDL_SubmitGPUCommandBuffer(commands);
    SDL_ReleaseGPUTransferBuffer(device, buffer);
}

void world_render(
    const camera_t* camera,
    void* commands,
    void* pass)
{
    if (!ibo)
    {
        return;
    }
    SDL_GPUBufferBinding ibb = {0};
    ibb.buffer = ibo;
    SDL_BindGPUIndexBuffer(pass, &ibb, SDL_GPU_INDEXELEMENTSIZE_32BIT);
    for (int i = 0; i < WORLD_CHUNKS; i++)
    {
        int x = presort[wy][i][0];
        int y = presort[wy][i][1];
        int z = presort[wy][i][2];
        if (on_world_border(x, 1, z))
        {
            continue;
        }
        const group_t* group = grid_get(&groups, x, z);
        if (!group->loaded)
        {
            continue;
        }
        const chunk_t* chunk = &group->chunks[y];
        if (!chunk->renderable)
        {
            continue;
        }
        x += wx;
        z += wz;
        x *= CHUNK_X;
        y *= CHUNK_Y;
        z *= CHUNK_Z;
        if (!camera_test(camera, x, y, z, CHUNK_X, CHUNK_Y, CHUNK_Z))
        {
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

void world_move(
    const int32_t x,
    const int32_t y,
    const int32_t z)
{
    const int a = x / CHUNK_X - WORLD_X / 2;
    const int b = y / CHUNK_Y - WORLD_Y / 2;
    const int c = z / CHUNK_Z - WORLD_Z / 2;

    wx = a;
    wy = clamp(b, 0, WORLD_Y - 1);
    wz = c;

    int num_oob;
    int* oob = grid_move(&groups, a, c, &num_oob);
    if (!oob) {
        return;
    }
    sort_2d(WORLD_X / 2, WORLD_Z / 2, oob, num_oob, true);
    for (int i = 0; i < num_oob; i++) {
        const int j = oob[i * 2 + 0];
        const int k = oob[i * 2 + 1];
        group_t* group = grid_get(&groups, j, k);
        for (int j = 0; j < GROUP_CHUNKS; j++) {
            chunk_t* chunk = &group->chunks[j];
            memset(chunk->blocks, 0, sizeof(chunk->blocks));
            chunk->renderable = false;
            chunk->empty = true;
        }
        group->neighbors = 0;
        group->loaded = false;
    }
    for (int i = 0; i < num_oob; i++) {
        const int j = oob[i * 2 + 0];
        const int k = oob[i * 2 + 1];
        assert(in_world(j, 0, k));
        group_t* group = grid_get(&groups, j, k);
        worker_load(group, j + wx, k + wz);
    }
    free(oob);
}

group_t* world_get_group(
    const int32_t x,
    const int32_t z)
{
    return grid_get2(&groups, x, z);
}

chunk_t* world_get_chunk(
    const int32_t x,
    const int32_t y,
    const int32_t z)
{
    return &(((group_t*) grid_get2(&groups, x, z))->chunks[y]);
}

void world_get_group_neighbors(
    const int32_t x,
    const int32_t z,
    group_t* neighbors[DIRECTION_2])
{
    grid_neighbors2(&groups, x, z, neighbors);
}

void world_get_chunk_neighbors(
    const int32_t x,
    const int32_t y,
    const int32_t z,
    chunk_t* neighbors[DIRECTION_3])
{
    for (direction_t direction = 0; direction < DIRECTION_3; direction++)
    {
        const int32_t a = x + directions[direction][0];
        const int32_t b = y + directions[direction][1];
        const int32_t c = z + directions[direction][2];
        if (!world_in(a, b, c))
        {
            neighbors[direction] = NULL;
            continue;
        }
        group_t* group = world_get_group(a, c);
        neighbors[direction] = &group->chunks[b];
    }
}

bool world_in(
    const int32_t x,
    const int32_t y,
    const int32_t z)
{
    const int32_t a = x - wx;
    const int32_t b = z - wz;
    return in_world(a, y, b);
}

bool world_on_border(
    const int32_t x,
    const int32_t y,
    const int32_t z)
{
    const int32_t a = x - wx;
    const int32_t b = z - wz;
    return on_world_border(a, y, b);
}