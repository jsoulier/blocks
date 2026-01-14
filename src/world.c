#include <SDL3/SDL.h>

#include "block.h"
#include "buffer.h"
#include "camera.h"
#include "map.h"
#include "noise.h"
#include "save.h"
#include "voxel.h"
#include "world.h"

#define WORKERS 4

typedef enum job_state
{
    JOB_STATE_REQUESTED,
    JOB_STATE_RUNNING,
    JOB_STATE_COMPLETED,
}
job_state_t;

typedef struct chunk
{
    SDL_AtomicInt block_job;
    SDL_AtomicInt voxel_job;
    SDL_AtomicInt light_job;
    int x;
    int z;
    block_t blocks[CHUNK_WIDTH][CHUNK_HEIGHT][CHUNK_WIDTH];
    map_t lights;
    gpu_buffer_t gpu_voxels[WORLD_MESH_COUNT];
    gpu_buffer_t gpu_lights;
}
chunk_t;

typedef enum job_type
{
    JOB_TYPE_NONE,
    JOB_TYPE_QUIT,
    JOB_TYPE_BLOCKS,
    JOB_TYPE_VOXELS,
    JOB_TYPE_LIGHTS,
}
job_type_t;

typedef struct worker_job
{
    job_type_t type;
    int x;
    int z;
}
worker_job_t;

typedef struct worker
{
    SDL_Thread* thread;
    SDL_Mutex* mutex;
    SDL_Condition* condition;
    worker_job_t job;
    cpu_buffer_t voxels[WORLD_MESH_COUNT];
    cpu_buffer_t lights;
}
worker_t;

static SDL_GPUDevice* device;
static int world_x;
static int world_z;
static worker_t workers[WORKERS];
static cpu_buffer_t cpu_indices;
static gpu_buffer_t gpu_indices;
static cpu_buffer_t cpu_empty_lights;
static gpu_buffer_t gpu_empty_lights;
static chunk_t* chunks[WORLD_WIDTH][WORLD_WIDTH];
static int sorted_chunks[WORLD_WIDTH][WORLD_WIDTH][2];
static bool should_move;
static SDL_Mutex* mutex;
static cpu_buffer_t gpu_voxels[WORLD_MESH_COUNT];
static cpu_buffer_t gpu_lights;

chunk_t* world_get_chunk(int x, int z);
void world_get_chunks(int x, int z, chunk_t* chunks[3][3]);

static int compare(void* userdata, const void* lhs, const void* rhs)
{
    int cx = ((int*) userdata)[0];
    int cy = ((int*) userdata)[1];
    const int* l = lhs;
    const int* r = rhs;
    int a = (l[0] - cx) * (l[0] - cx) + (l[1] - cy) * (l[1] - cy);
    int b = (r[0] - cx) * (r[0] - cx) + (r[1] - cy) * (r[1] - cy);
    return (a > b) - (a < b);
}

void sort_xy(int x, int y, int* data, int size)
{
    SDL_qsort_r(data, size, sizeof(int) * 2, compare, (int[]) {x, y});
}

static bool chunk_is_local(int x, int y, int z)
{
    SDL_assert(y >= 0);
    SDL_assert(y < CHUNK_HEIGHT);
    return x >= 0 && z >= 0 && x < CHUNK_WIDTH && z < CHUNK_WIDTH;
}

void chunk_world_to_local(const chunk_t* chunk, int* x, int* y, int* z)
{
    *x -= chunk->x;
    *z -= chunk->z;
    SDL_assert(chunk_is_local(*x, *y, *z));
}

void chunk_local_to_world(const chunk_t* chunk, int* x, int* y, int* z)
{
    // TODO: SDL_assert(chunk_is_local(*x, *y, *z));
    *x += chunk->x;
    *z += chunk->z;
}

void chunk_init(chunk_t* chunk)
{
    SDL_SetAtomicInt(&chunk->block_job, JOB_STATE_REQUESTED);
    SDL_SetAtomicInt(&chunk->voxel_job, JOB_STATE_COMPLETED);
    SDL_SetAtomicInt(&chunk->light_job, JOB_STATE_COMPLETED);
    chunk->x = 0;
    chunk->z = 0;
    map_init(&chunk->lights, 1);
    for (int i = 0; i < WORLD_MESH_COUNT; i++)
    {
        gpu_buffer_init(&chunk->gpu_voxels[i], device, SDL_GPU_BUFFERUSAGE_VERTEX);
    }
    gpu_buffer_init(&chunk->gpu_lights, device, SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ);
}

void chunk_free(chunk_t* chunk)
{
    gpu_buffer_free(&chunk->gpu_lights);
    for (int i = 0; i < WORLD_MESH_COUNT; i++)
    {
        gpu_buffer_free(&chunk->gpu_voxels[i]);
    }
    map_free(&chunk->lights);
    chunk->x = 0;
    chunk->z = 0;
    SDL_SetAtomicInt(&chunk->block_job, JOB_STATE_COMPLETED);
    SDL_SetAtomicInt(&chunk->voxel_job, JOB_STATE_COMPLETED);
    SDL_SetAtomicInt(&chunk->light_job, JOB_STATE_COMPLETED);
}

block_t chunk_set_block(chunk_t* chunk, int x, int y, int z, block_t block)
{
    SDL_SetAtomicInt(&chunk->voxel_job, JOB_STATE_REQUESTED);
    chunk_world_to_local(chunk, &x, &y, &z);
    chunk->blocks[x][y][z] = block;
    block_t old_block = map_get(&chunk->lights, x, y, z);
    if (!block_is_light(block) && !block_is_light(old_block))
    {
        return old_block;
    }
    SDL_SetAtomicInt(&chunk->light_job, JOB_STATE_REQUESTED);
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
    SDL_assert(SDL_GetAtomicInt(&chunk->block_job) == JOB_STATE_COMPLETED);
    chunk_world_to_local(chunk, &x, &y, &z);
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
    if (chunk_is_local(x, y, z))
    {
        return chunk->blocks[x][y][z];
    }
    chunk_local_to_world(chunk, &x, &y, &z);
    chunk_t* neighbor = chunks[dx + 1][dz + 1];
    SDL_assert(neighbor);
    return chunk_get_block(neighbor, x, y, z);
}

static void upload_voxels(chunk_t* chunk, cpu_buffer_t voxels[WORLD_MESH_COUNT])
{
    bool has_voxels = false;
    for (int i = 0; i < WORLD_MESH_COUNT; i++)
    {
        gpu_buffer_clear(&chunk->gpu_voxels[i]);
        has_voxels |= voxels[i].size > 0;
    }
    if (!has_voxels)
    {
        return;
    }
    SDL_GPUCommandBuffer* command_buffer = SDL_AcquireGPUCommandBuffer(device);
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
    for (int i = 0; i < WORLD_MESH_COUNT; i++)
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
    SDL_GPUCommandBuffer* command_buffer = SDL_AcquireGPUCommandBuffer(device);
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

void chunk_set_voxels(chunk_t* chunks[3][3], cpu_buffer_t voxels[WORLD_MESH_COUNT])
{
    chunk_t* chunk = chunks[1][1];
    SDL_assert(SDL_GetAtomicInt(&chunk->block_job) == JOB_STATE_COMPLETED);
    SDL_assert(SDL_GetAtomicInt(&chunk->voxel_job) == JOB_STATE_RUNNING);
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
                cpu_buffer_append(&voxels[WORLD_MESH_OPAQUE], &voxel);
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
                    cpu_buffer_append(&voxels[WORLD_MESH_OPAQUE], &voxel);
                }
                else
                {
                    cpu_buffer_append(&voxels[WORLD_MESH_TRANSPARENT], &voxel);
                }
            }
        }
    }
    upload_voxels(chunk, voxels);
    SDL_SetAtomicInt(&chunk->voxel_job, JOB_STATE_COMPLETED);
}

void chunk_set_lights(chunk_t* chunks[3][3], cpu_buffer_t* lights)
{
    chunk_t* chunk = chunks[1][1];
    SDL_assert(SDL_GetAtomicInt(&chunk->block_job) == JOB_STATE_COMPLETED);
    SDL_assert(SDL_GetAtomicInt(&chunk->light_job) == JOB_STATE_RUNNING);
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
    SDL_SetAtomicInt(&chunk->light_job, JOB_STATE_COMPLETED);
}

static void get_blocks(void* userdata, int bx, int by, int bz, block_t block)
{
    chunk_t* chunk = userdata;
    chunk_set_block(chunk, bx, by, bz, block);
}

void chunk_set_blocks(chunk_t* chunk)
{
    SDL_assert(SDL_GetAtomicInt(&chunk->block_job) == JOB_STATE_RUNNING);
    SDL_memset(chunk->blocks, 0, sizeof(chunk->blocks));
    map_clear(&chunk->lights);
    noise_set_blocks(chunk, chunk->x, chunk->z, get_blocks);
    SDL_SetAtomicInt(&chunk->block_job, JOB_STATE_COMPLETED);
    if (SDL_GetAtomicInt(&chunk->voxel_job) == JOB_STATE_REQUESTED)
    {
        SDL_SetAtomicInt(&chunk->light_job, JOB_STATE_REQUESTED);
    }
}

void world_create_indices(Uint32 size);

static int worker_func(void* args)
{
    worker_t* worker = args;
    while (true)
    {
        SDL_LockMutex(worker->mutex);
        while (worker->job.type == JOB_TYPE_NONE)
        {
            SDL_WaitCondition(worker->condition, worker->mutex);
        }
        worker_job_t job = worker->job;
        SDL_UnlockMutex(worker->mutex);
        if (job.type == JOB_TYPE_QUIT)
        {
            SDL_LockMutex(worker->mutex);
            worker->job.type = JOB_TYPE_NONE;
            SDL_SignalCondition(worker->condition);
            SDL_UnlockMutex(worker->mutex);
            return 0;
        }
        chunk_t* chunk = world_get_chunk(job.x, job.z);
        SDL_assert(chunk);
        if (job.type == JOB_TYPE_BLOCKS)
        {
            chunk_set_blocks(chunk);
            save_get_blocks(chunk, chunk->x, chunk->z, get_blocks);
        }
        else
        {
            chunk_t* chunks[3][3];
            world_get_chunks(job.x, job.z, chunks);
            if (job.type == JOB_TYPE_VOXELS)
            {
                chunk_set_voxels(chunks, worker->voxels);
                for (int i = 0; i < WORLD_MESH_COUNT; i++)
                {
                    world_create_indices(chunk->gpu_voxels[i].size);
                }
            }
            else if (job.type == JOB_TYPE_LIGHTS)
            {
                chunk_set_lights(chunks, &worker->lights);
            }
            else
            {
                SDL_assert(false);
            }
        }
        SDL_LockMutex(worker->mutex);
        worker->job.type = JOB_TYPE_NONE;
        SDL_SignalCondition(worker->condition);
        SDL_UnlockMutex(worker->mutex);
    }
    return 0;
}

void worker_init(worker_t* worker, SDL_GPUDevice* device)
{
    for (int i = 0; i < WORLD_MESH_COUNT; i++)
    {
        cpu_buffer_init(&worker->voxels[i], device, sizeof(voxel_t));
    }
    cpu_buffer_init(&worker->lights, device, sizeof(light_t));
    worker->mutex = SDL_CreateMutex();
    if (!worker->mutex)
    {
        SDL_Log("Failed to create mutex: %s", SDL_GetError());
    }
    worker->condition = SDL_CreateCondition();
    if (!worker->condition)
    {
        SDL_Log("Failed to create condition variable: %s", SDL_GetError());
    }
    worker->thread = SDL_CreateThread(worker_func, "worker", worker);
    if (!worker->thread)
    {
        SDL_Log("Failed to create thread: %s", SDL_GetError());
    }
}

void worker_wait(const worker_t* worker)
{
    SDL_LockMutex(worker->mutex);
    while (worker->job.type != JOB_TYPE_NONE)
    {
        SDL_WaitCondition(worker->condition, worker->mutex);
    }
    SDL_UnlockMutex(worker->mutex);
}

bool worker_is_working(const worker_t* worker)
{
    SDL_LockMutex(worker->mutex);
    bool working = worker->job.type != JOB_TYPE_NONE;
    SDL_UnlockMutex(worker->mutex);
    return working;
}

void worker_dispatch(worker_t* worker, const worker_job_t* job)
{
    if (job->type != JOB_TYPE_QUIT)
    {
        SDL_assert(!worker_is_working(worker));
        chunk_t* chunk = world_get_chunk(job->x, job->z);
        switch (job->type)
        {
        case JOB_TYPE_BLOCKS:
            SDL_SetAtomicInt(&chunk->block_job, JOB_STATE_RUNNING);
            break;
        case JOB_TYPE_VOXELS:
            SDL_SetAtomicInt(&chunk->voxel_job, JOB_STATE_RUNNING);
            break;
        case JOB_TYPE_LIGHTS:
            SDL_SetAtomicInt(&chunk->light_job, JOB_STATE_RUNNING);
            break;
        default:
            SDL_assert(false);
        }
    }
    else
    {
        worker_wait(worker);
    }
    SDL_LockMutex(worker->mutex);
    SDL_assert(worker->job.type == JOB_TYPE_NONE);
    worker->job = *job;
    SDL_SignalCondition(worker->condition);
    SDL_UnlockMutex(worker->mutex);
}

void worker_free(worker_t* worker)
{
    worker_job_t job = {0};
    job.type = JOB_TYPE_QUIT;
    worker_dispatch(worker, &job);
    SDL_WaitThread(worker->thread, NULL);
    SDL_DestroyMutex(worker->mutex);
    SDL_DestroyCondition(worker->condition);
    worker->thread = NULL;
    worker->mutex = NULL;
    worker->condition = NULL;
    for (int i = 0; i < WORLD_MESH_COUNT; i++)
    {
        cpu_buffer_free(&worker->voxels[i]);
    }
    cpu_buffer_free(&worker->lights);
}

static bool world_is_local(int x, int z)
{
    return x >= 0 && z >= 0 && x < WORLD_WIDTH && z < WORLD_WIDTH;
}

static bool world_is_bordering(int x, int z)
{
    return x == 0 || z == 0 || x == WORLD_WIDTH - 1 || z == WORLD_WIDTH - 1;
}

static int floor_chunk_index(float index)
{
    return SDL_floorf(index / CHUNK_WIDTH);
}

static void create_empty_lights()
{
    SDL_GPUCommandBuffer* command_buffer = SDL_AcquireGPUCommandBuffer(device);
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
    light_t light = {0};
    cpu_buffer_append(&cpu_empty_lights, &light);
    gpu_buffer_update(&gpu_empty_lights, copy_pass, &cpu_empty_lights);
    SDL_EndGPUCopyPass(copy_pass);
    SDL_SubmitGPUCommandBuffer(command_buffer);
}

static void create_indices(Uint32 size)
{
    if (gpu_indices.size >= size)
    {
        return;
    }
    static const int INDICES[] = {0, 1, 2, 3, 2, 1};
    for (Uint32 i = 0; i < size; i++)
    for (Uint32 j = 0; j < 6; j++)
    {
        Uint32 index = i * 4 + INDICES[j];
        cpu_buffer_append(&cpu_indices, &index);
    }
    SDL_GPUCommandBuffer* command_buffer = SDL_AcquireGPUCommandBuffer(device);
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
    gpu_buffer_update(&gpu_indices, copy_pass, &cpu_indices);
    SDL_EndGPUCopyPass(copy_pass);
    SDL_SubmitGPUCommandBuffer(command_buffer);
}

void world_init(SDL_GPUDevice* handle)
{
    device = handle;
    world_x = SDL_MAX_SINT32;
    world_z = SDL_MAX_SINT32;
    cpu_buffer_init(&cpu_indices, device, sizeof(Uint32));
    gpu_buffer_init(&gpu_indices, device, SDL_GPU_BUFFERUSAGE_INDEX);
    cpu_buffer_init(&cpu_empty_lights, device, sizeof(light_t));
    gpu_buffer_init(&gpu_empty_lights, device, SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ);
    for (int i = 0; i < WORLD_MESH_COUNT; i++)
    {
        cpu_buffer_init(&gpu_voxels[i], device, sizeof(voxel_t));
    }
    cpu_buffer_init(&gpu_lights, device, sizeof(light_t));
    for (int i = 0; i < WORKERS; i++)
    {
        worker_init(&workers[i], device);
    }
    for (int x = 0; x < WORLD_WIDTH; x++)
    for (int z = 0; z < WORLD_WIDTH; z++)
    {
        chunks[x][z] = SDL_calloc(1, sizeof(chunk_t));
        chunk_init(chunks[x][z]);
    }
    for (int x = 0; x < WORLD_WIDTH; x++)
    for (int z = 0; z < WORLD_WIDTH; z++)
    {
        sorted_chunks[x][z][0] = x;
        sorted_chunks[x][z][1] = z;
    }
    int w = WORLD_WIDTH;
    sort_xy(w / 2, w / 2, (int*) sorted_chunks, w * w);
    create_empty_lights();
    create_indices(1000000);
    mutex = SDL_CreateMutex();
    if (!mutex)
    {
        SDL_Log("Failed to create mutex: %s", SDL_GetError());
    }
}

void world_free()
{
    SDL_DestroyMutex(mutex);
    for (int i = 0; i < WORKERS; i++)
    {
        worker_free(&workers[i]);
    }
    for (int x = 0; x < WORLD_WIDTH; x++)
    for (int z = 0; z < WORLD_WIDTH; z++)
    {
        chunk_free(chunks[x][z]);
        SDL_free(chunks[x][z]);
    }
    cpu_buffer_free(&cpu_indices);
    gpu_buffer_free(&gpu_indices);
    cpu_buffer_free(&cpu_empty_lights);
    gpu_buffer_free(&gpu_empty_lights);
    for (int i = 0; i < WORLD_MESH_COUNT; i++)
    {
        cpu_buffer_free(&gpu_voxels[i]);
    }
    cpu_buffer_free(&gpu_lights);
}

static int get_working_count()
{
    int count = 0;
    for (int i = 0; i < WORKERS; i++)
    {
        count += worker_is_working(&workers[i]);
    }
    return count;
}

static void move_chunks(const camera_t* camera)
{
    const int camera_x = camera->x / CHUNK_WIDTH - WORLD_WIDTH / 2;
    const int camera_z = camera->z / CHUNK_WIDTH - WORLD_WIDTH / 2;
    const int offset_x = camera_x - world_x;
    const int offset_z = camera_z - world_z;
    if (!offset_x && !offset_z)
    {
        return;
    }
    if (get_working_count())
    {
        should_move = true;
        return;
    }
    should_move = false;
    world_x = camera_x;
    world_z = camera_z;
    chunk_t* in[WORLD_WIDTH][WORLD_WIDTH] = {0};
    chunk_t* out[WORLD_WIDTH * WORLD_WIDTH] = {0};
    int size = 0;
    for (int x = 0; x < WORLD_WIDTH; x++)
    for (int z = 0; z < WORLD_WIDTH; z++)
    {
        SDL_assert(chunks[x][z]);
        const int a = x - offset_x;
        const int b = z - offset_z;
        if (world_is_local(a, b))
        {
            in[a][b] = chunks[x][z];
        }
        else
        {
            out[size++] = chunks[x][z];
        }
        chunks[x][z] = NULL;
    }
    SDL_memcpy(chunks, in, sizeof(in));
    for (int x = 0; x < WORLD_WIDTH; x++)
    for (int z = 0; z < WORLD_WIDTH; z++)
    {
        if (!chunks[x][z])
        {
            SDL_assert(size > 0);
            chunk_t* chunk = out[--size];
            SDL_SetAtomicInt(&chunk->block_job, JOB_STATE_REQUESTED);
            chunks[x][z] = chunk;
        }
        chunk_t* chunk = chunks[x][z];
        chunk->x = (world_x + x) * CHUNK_WIDTH;
        chunk->z = (world_z + z) * CHUNK_WIDTH;
    }
    SDL_assert(!size);
}

static void update_chunks()
{
    worker_t* local_workers[WORKERS] = {0};
    worker_job_t jobs[WORKERS] = {0};
    int num_workers = 0;
    for (int i = 0; i < WORKERS; i++)
    {
        if (!worker_is_working(&workers[i]))
        {
            local_workers[num_workers++] = &workers[i];
        }
    }
    if (num_workers == 0)
    {
        return;
    }
    for (int x = 0; x < WORLD_WIDTH; x++)
    for (int z = 0; z < WORLD_WIDTH; z++)
    {
        int a = sorted_chunks[x][z][0];
        int b = sorted_chunks[x][z][1];
        chunk_t* chunk = chunks[a][b];
        if (SDL_GetAtomicInt(&chunk->block_job) == JOB_STATE_REQUESTED)
        {
            num_workers--;
            jobs[num_workers] = (worker_job_t) {JOB_TYPE_BLOCKS, a, b};
            SDL_assert(!worker_is_working(local_workers[num_workers]));
            worker_dispatch(local_workers[num_workers], &jobs[num_workers]);
            if (num_workers == 0)
            {
                return;
            }
            continue;
        }
        if (world_is_bordering(a, b))
        {
            continue;
        }
        if (SDL_GetAtomicInt(&chunk->voxel_job) == JOB_STATE_REQUESTED || SDL_GetAtomicInt(&chunk->light_job) == JOB_STATE_REQUESTED)
        {
            bool should_work = true;
            chunk_t* local_chunks[3][3];
            world_get_chunks(a, b, local_chunks);
            for (int i = 0; i < 3; i++)
            for (int j = 0; j < 3; j++)
            {
                if (SDL_GetAtomicInt(&local_chunks[i][j]->block_job) != JOB_STATE_COMPLETED)
                {
                    should_work = false;
                }
            }
            if (!should_work)
            {
                continue;
            }
            num_workers--;
            if (SDL_GetAtomicInt(&chunk->voxel_job) == JOB_STATE_REQUESTED)
            {
                jobs[num_workers] = (worker_job_t) {JOB_TYPE_VOXELS, a, b};
            }
            else if (SDL_GetAtomicInt(&chunk->light_job) == JOB_STATE_REQUESTED)
            {
                jobs[num_workers] = (worker_job_t) {JOB_TYPE_LIGHTS, a, b};
            }
            else
            {
                SDL_assert(false);
            }
            worker_dispatch(local_workers[num_workers], &jobs[num_workers]);
            if (num_workers == 0)
            {
                return;
            }
        }
    }
}

void world_update(const camera_t* camera)
{
    move_chunks(camera);
    if (!should_move)
    {
        update_chunks();
    }
}

void world_render(const world_pass_t* data)
{
    const camera_t* camera = data->camera;
    SDL_GPUCommandBuffer* command_buffer = data->command_buffer;
    SDL_GPURenderPass* render_pass = data->render_pass;
    SDL_PushGPUDebugGroup(command_buffer, "world");
    SDL_PushGPUVertexUniformData(command_buffer, 0, camera->proj, 64);
    SDL_PushGPUVertexUniformData(command_buffer, 1, camera->view, 64);
    for (int x = 0; x < WORLD_WIDTH; x++)
    for (int y = 0; y < WORLD_WIDTH; y++)
    {
        int a = sorted_chunks[x][y][0];
        int b = sorted_chunks[x][y][1];
        if (world_is_bordering(a, b))
        {
            continue;
        }
        chunk_t* chunk = chunks[a][b];
        if (SDL_GetAtomicInt(&chunk->voxel_job) != JOB_STATE_COMPLETED)
        {
            continue;
        }
        gpu_buffer_t* gpu_voxels = &chunk->gpu_voxels[data->mesh];
        if (!gpu_voxels->size)
        {
            continue;
        }
        int w = CHUNK_WIDTH;
        int h = CHUNK_HEIGHT;
        if (!camera_get_visibility(camera, chunk->x, 0, chunk->z, w, h, w))
        {
            continue;
        }
        float position[] = {chunk->x, 0, chunk->z};
        SDL_GPUBufferBinding voxel_binding = {gpu_voxels->buffer};
        SDL_GPUBufferBinding index_binding = {gpu_indices.buffer};
        if (data->lights)
        {
            Sint32 light_count = 0;
            if (SDL_GetAtomicInt(&chunk->light_job) != JOB_STATE_COMPLETED || !chunk->gpu_lights.size)
            {
                SDL_BindGPUFragmentStorageBuffers(render_pass, 0, &gpu_empty_lights.buffer, 1);
                light_count = 0;
            }
            else
            {
                SDL_BindGPUFragmentStorageBuffers(render_pass, 0, &chunk->gpu_lights.buffer, 1);
                light_count = chunk->gpu_lights.size;
            }
            SDL_PushGPUFragmentUniformData(command_buffer, 0, &light_count, sizeof(light_count));
        }
        SDL_PushGPUVertexUniformData(command_buffer, 2, position, sizeof(position));
        SDL_BindGPUVertexBuffers(render_pass, 0, &voxel_binding, 1);
        SDL_BindGPUIndexBuffer(render_pass, &index_binding, SDL_GPU_INDEXELEMENTSIZE_32BIT);
        SDL_DrawGPUIndexedPrimitives(render_pass, gpu_voxels->size * 1.5, 1, 0, 0, 0);
    }
    SDL_PopGPUDebugGroup(command_buffer);
}

chunk_t* world_get_chunk(int x, int z)
{
    if (world_is_local(x, z))
    {
        return chunks[x][z];
    }
    else
    {
        return NULL;
    }
}

void world_get_chunks(int x, int z, chunk_t* chunks[3][3])
{
    for (int i = -1; i <= 1; i++)
    for (int j = -1; j <= 1; j++)
    {
        int k = x + i;
        int l = z + j;
        chunks[i + 1][j + 1] = world_get_chunk(k, l);
    }
}

block_t world_get_block(int index[3])
{
    if (index[1] < 0 || index[1] >= CHUNK_HEIGHT)
    {
        return BLOCK_EMPTY;
    }
    int chunk_x = floor_chunk_index(index[0] - world_x * CHUNK_WIDTH);
    int chunk_z = floor_chunk_index(index[2] - world_z * CHUNK_WIDTH);
    chunk_t* chunk = world_get_chunk(chunk_x, chunk_z);
    if (chunk)
    {
        SDL_assert(chunk->x == (world_x + chunk_x) * CHUNK_WIDTH);
        SDL_assert(chunk->z == (world_z + chunk_z) * CHUNK_WIDTH);
    }
    else
    {
        SDL_Log("Bad chunk position: %d, %d", chunk_x, chunk_z);
        return BLOCK_EMPTY;
    }
    if (SDL_GetAtomicInt(&chunk->block_job) != JOB_STATE_COMPLETED || SDL_GetAtomicInt(&chunk->voxel_job) != JOB_STATE_COMPLETED)
    {
        return BLOCK_EMPTY;
    }
    return chunk_get_block(chunk, index[0], index[1], index[2]);
}

static void set_voxels(int x, int z)
{
    SDL_assert(!world_is_bordering(x, z));
    chunk_t* chunks[3][3] = {0};
    world_get_chunks(x, z, chunks);
    SDL_SetAtomicInt(&chunks[1][1]->voxel_job, JOB_STATE_RUNNING);
    chunk_set_voxels(chunks, gpu_voxels);
}

void world_set_block(int index[3], block_t block)
{
    // TODO: you're able to set on something that is working
    // convert has_blocks/set_blocks -> job_state
    if (index[1] < 0 || index[1] >= CHUNK_HEIGHT)
    {
        return;
    }
    int chunk_x = floor_chunk_index(index[0] - world_x * CHUNK_WIDTH);
    int chunk_z = floor_chunk_index(index[2] - world_z * CHUNK_WIDTH);
    chunk_t* chunk = world_get_chunk(chunk_x, chunk_z);
    if (chunk)
    {
        SDL_assert(chunk->x == (world_x + chunk_x) * CHUNK_WIDTH);
        SDL_assert(chunk->z == (world_z + chunk_z) * CHUNK_WIDTH);
    }
    else
    {
        SDL_Log("Bad chunk position: %d, %d", chunk_x, chunk_z);
        return;
    }
    if (SDL_GetAtomicInt(&chunk->block_job) != JOB_STATE_COMPLETED || SDL_GetAtomicInt(&chunk->voxel_job) != JOB_STATE_COMPLETED)
    {
        return;
    }
    block_t old_block = chunk_set_block(chunk, index[0], index[1], index[2], block);
    set_voxels(chunk_x, chunk_z);
    int local_x = index[0];
    int local_y = index[1];
    int local_z = index[2];
    chunk_world_to_local(chunk, &local_x, &local_y, &local_z);
    if (local_x == 0)
    {
        set_voxels(chunk_x - 1, chunk_z);
    }
    else if (local_x == CHUNK_WIDTH - 1)
    {
        set_voxels(chunk_x + 1, chunk_z);
    }
    if (local_z == 0)
    {
        set_voxels(chunk_x, chunk_z - 1);
    }
    else if (local_z == CHUNK_WIDTH - 1)
    {
        set_voxels(chunk_x, chunk_z + 1);
    }
    chunk_t* local_chunks[3][3] = {0};
    world_get_chunks(chunk_x, chunk_z, local_chunks);
    if (block_is_light(block) || block_is_light(old_block))
    {
        for (int i = -1; i <= 1; i++)
        for (int j = -1; j <= 1; j++)
        {
            SDL_SetAtomicInt(&local_chunks[i + 1][j + 1]->light_job, JOB_STATE_REQUESTED);
        }
    }
    save_set_block(chunk->x, chunk->z, index[0], index[1], index[2], block);
}

world_raycast_t world_raycast(float x, float y, float z, float dx, float dy, float dz, float length)
{
    world_raycast_t query = {0};
    query.current[0] = SDL_floorf(x);
    query.current[1] = SDL_floorf(y);
    query.current[2] = SDL_floorf(z);
    float start[3] = {x, y, z};
    float direction[3] = {dx, dy, dz};
    float distances[3] = {0};
    int steps[3] = {0};
    float deltas[3] = {0};
    for (int i = 0; i < 3; i++)
    {
        query.previous[i] = query.current[i];
        if (SDL_fabsf(direction[i]) > SDL_FLT_EPSILON)
        {
            deltas[i] = SDL_fabsf(1.0f / direction[i]);
        }
        else
        {
            deltas[i] = 1e6;
        }
        if (direction[i] < 0.0f)
        {
            steps[i] = -1;
            distances[i] = (start[i] - query.current[i]) * deltas[i];
        }
        else
        {
            steps[i] = 1;
            distances[i] = (query.current[i] + 1.0f - start[i]) * deltas[i];
        }
    }
    float traveled = 0.0f;
    while (traveled <= length)
    {
        query.block = world_get_block(query.current);
        if (block_is_solid(query.block))
        {
            return query;
        }
        for (int i = 0; i < 3; i++)
        {
            query.previous[i] = query.current[i];
        }
        if (distances[0] < distances[1])
        {
            if (distances[0] < distances[2])
            {
                traveled = distances[0];
                distances[0] += deltas[0];
                query.current[0] += steps[0];
            }
            else
            {
                traveled = distances[2];
                distances[2] += deltas[2];
                query.current[2] += steps[2];
            }
        }
        else
        {
            if (distances[1] < distances[2])
            {
                traveled = distances[1];
                distances[1] += deltas[1];
                query.current[1] += steps[1];
            }
            else
            {
                traveled = distances[2];
                distances[2] += deltas[2];
                query.current[2] += steps[2];
            }
        }
    }
    query.block = BLOCK_EMPTY;
    return query;
}

void world_create_indices(Uint32 size)
{
    SDL_LockMutex(mutex);
    create_indices(size * 1.5);
    SDL_UnlockMutex(mutex);
}
