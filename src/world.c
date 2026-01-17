#include <SDL3/SDL.h>

#include "block.h"
#include "buffer.h"
#include "camera.h"
#include "check.h"
#include "map.h"
#include "rand.h"
#include "save.h"
#include "voxel.h"
#include "world.h"

#define WORKERS 4

typedef enum job_type
{
    JOB_TYPE_NONE,
    JOB_TYPE_QUIT,
    JOB_TYPE_BLOCKS,
    JOB_TYPE_VOXELS,
    JOB_TYPE_LIGHTS,
}
job_type_t;

typedef enum job_state
{
    JOB_STATE_REQUESTED,
    JOB_STATE_RUNNING,
    JOB_STATE_COMPLETED,
}
job_state_t;

typedef enum mesh_type
{
    MESH_TYPE_OPAQUE,
    MESH_TYPE_TRANSPARENT,
    MESH_TYPE_COUNT,
}
mesh_type_t;

typedef struct job
{
    job_type_t type;
    int x;
    int z;
}
job_t;

typedef struct worker
{
    SDL_Thread* thread;
    SDL_Mutex* mutex;
    SDL_Condition* condition;
    job_t job;
    cpu_buffer_t voxels[MESH_TYPE_COUNT];
    cpu_buffer_t lights;
}
worker_t;

typedef struct chunk
{
    SDL_AtomicInt block_state;
    SDL_AtomicInt voxel_state;
    SDL_AtomicInt light_state;
    union
    {
        struct
        {
            Sint32 x;
            Sint32 z;
        };
        Sint32 position[2];
    };
    block_t blocks[CHUNK_WIDTH][CHUNK_HEIGHT][CHUNK_WIDTH];
    map_t lights;
    gpu_buffer_t gpu_voxels[MESH_TYPE_COUNT];
    gpu_buffer_t gpu_lights;
}
chunk_t;

static SDL_GPUDevice* device;
static int world_x;
static int world_z;
static bool is_moving;
static worker_t workers[WORKERS];
static cpu_buffer_t cpu_indices;
static gpu_buffer_t gpu_indices;
static cpu_buffer_t cpu_empty_lights;
static gpu_buffer_t gpu_empty_lights;
static cpu_buffer_t cpu_voxels[MESH_TYPE_COUNT];
static chunk_t* chunks[WORLD_WIDTH][WORLD_WIDTH];
static int sorted_chunks[WORLD_WIDTH][WORLD_WIDTH][2];
static SDL_Mutex* mutex;

static bool is_block_local(int bx, int by, int bz)
{
    CHECK(by >= 0 && by < CHUNK_HEIGHT);
    CHECK(bx >= -1 && bz >= -1 && bx <= CHUNK_WIDTH && bz <= CHUNK_WIDTH);;
    return bx >= 0 && bz >= 0 && bx < CHUNK_WIDTH && bz < CHUNK_WIDTH;
}

static void world_to_chunk(const chunk_t* chunk, int* bx, int* by, int* bz)
{
    *bx -= chunk->x;
    *bz -= chunk->z;
    CHECK(is_block_local(*bx, *by, *bz));
}

static void chunk_to_world(const chunk_t* chunk, int* bx, int* by, int* bz)
{
    CHECK(*by >= 0 && *by < CHUNK_HEIGHT);
    *bx += chunk->x;
    *bz += chunk->z;
}

static bool is_chunk_local(int cx, int cz)
{
    return cx >= 0 && cz >= 0 && cx < WORLD_WIDTH && cz < WORLD_WIDTH;
}

static bool is_chunk_on_border(int cx, int cz)
{
    return cx == 0 || cz == 0 || cx == WORLD_WIDTH - 1 || cz == WORLD_WIDTH - 1;
}

static int floor_chunk_index(float index)
{
    return SDL_floorf(index / CHUNK_WIDTH);
}

static chunk_t* get_chunk(int cx, int cz)
{
    if (is_chunk_local(cx, cz))
    {
        return chunks[cx][cz];
    }
    else
    {
        return NULL;
    }
}

static void get_neighborhood(int cx, int cz, chunk_t* neighborhood[3][3])
{
    for (int i = -1; i <= 1; i++)
    for (int j = -1; j <= 1; j++)
    {
        int x = cx + i;
        int z = cz + j;
        neighborhood[i + 1][j + 1] = get_chunk(x, z);
        CHECK(neighborhood[i + 1][j + 1]);
    }
}

static mesh_type_t get_mesh_for_flags(world_flags_t flags)
{
    if (flags & WORLD_FLAGS_OPAQUE)
    {
        return MESH_TYPE_OPAQUE;
    }
    else
    {
        return MESH_TYPE_TRANSPARENT;
    }
}

static mesh_type_t get_mesh_for_block(block_t block)
{
    if (block_is_opaque(block))
    {
        return MESH_TYPE_OPAQUE;
    }
    else
    {
        return MESH_TYPE_TRANSPARENT;
    }
}

static chunk_t* create_chunk()
{
    chunk_t* chunk = SDL_malloc(sizeof(chunk_t));
    if (!chunk)
    {
        SDL_Log("Failed to allocate chunk");
    }
    SDL_SetAtomicInt(&chunk->block_state, JOB_STATE_REQUESTED);
    SDL_SetAtomicInt(&chunk->voxel_state, JOB_STATE_COMPLETED);
    SDL_SetAtomicInt(&chunk->light_state, JOB_STATE_COMPLETED);
    chunk->x = 0;
    chunk->z = 0;
    map_init(&chunk->lights, 8);
    for (int i = 0; i < MESH_TYPE_COUNT; i++)
    {
        gpu_buffer_init(&chunk->gpu_voxels[i], device, SDL_GPU_BUFFERUSAGE_VERTEX);
    }
    gpu_buffer_init(&chunk->gpu_lights, device, SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ);
    return chunk;
}

static void free_chunk(chunk_t* chunk)
{
    gpu_buffer_free(&chunk->gpu_lights);
    for (int i = 0; i < MESH_TYPE_COUNT; i++)
    {
        gpu_buffer_free(&chunk->gpu_voxels[i]);
    }
    SDL_memset(chunk->blocks, 0, sizeof(chunk->blocks));
    map_free(&chunk->lights);
    SDL_free(chunk);
}

static block_t set_chunk_block(chunk_t* chunk, int bx, int by, int bz, block_t block)
{
    SDL_SetAtomicInt(&chunk->voxel_state, JOB_STATE_REQUESTED);
    world_to_chunk(chunk, &bx, &by, &bz);
    chunk->blocks[bx][by][bz] = block;
    block_t old_block = map_get(&chunk->lights, bx, by, bz);
    if (!block_is_light(block) && !block_is_light(old_block))
    {
        return old_block;
    }
    SDL_SetAtomicInt(&chunk->light_state, JOB_STATE_REQUESTED);
    if (block_is_light(block))
    {
        map_set(&chunk->lights, bx, by, bz, block);
    }
    else
    {
        map_remove(&chunk->lights, bx, by, bz);
    }
    return old_block;
}

static void set_chunk_block_function(void* userdata, int bx, int by, int bz, block_t block)
{
    chunk_t* chunk = userdata;
    CHECK(SDL_GetAtomicInt(&chunk->block_state) == JOB_STATE_RUNNING);
    set_chunk_block(userdata, bx, by, bz, block);
}

static block_t get_chunk_block(chunk_t* chunk, int bx, int by, int bz)
{
    CHECK(SDL_GetAtomicInt(&chunk->block_state) == JOB_STATE_COMPLETED);
    world_to_chunk(chunk, &bx, &by, &bz);
    return chunk->blocks[bx][by][bz];
}

static block_t get_neighborhood_block(chunk_t* chunks[3][3], int bx, int by, int bz, int dx, int dy, int dz)
{
    CHECK(dx >= -1 && dx <= 1);
    CHECK(dy >= -1 && dy <= 1);
    CHECK(dz >= -1 && dz <= 1);
    bx += dx;
    by += dy;
    bz += dz;
    const chunk_t* chunk = chunks[1][1];
    if (by == CHUNK_HEIGHT)
    {
        return BLOCK_EMPTY;
    }
    else if (by == -1)
    {
        return BLOCK_GRASS;
    }
    else if (is_block_local(bx, by, bz))
    {
        return chunk->blocks[bx][by][bz];
    }
    chunk_to_world(chunk, &bx, &by, &bz);
    chunk_t* neighbor = chunks[dx + 1][dz + 1];
    CHECK(neighbor);
    return get_chunk_block(neighbor, bx, by, bz);
}

static void upload_voxels(chunk_t* chunk, cpu_buffer_t voxels[MESH_TYPE_COUNT])
{
    CHECK(SDL_GetAtomicInt(&chunk->block_state) == JOB_STATE_COMPLETED);
    CHECK(SDL_GetAtomicInt(&chunk->voxel_state) == JOB_STATE_RUNNING);
    bool has_voxels = false;
    for (int i = 0; i < MESH_TYPE_COUNT; i++)
    {
        gpu_buffer_clear(&chunk->gpu_voxels[i]);
        has_voxels |= voxels[i].size > 0;
    }
    if (!has_voxels)
    {
        return;
    }
    if (!gpu_buffer_begin_upload(&chunk->gpu_voxels[0]))
    {
        return;
    }
    for (int i = 0; i < MESH_TYPE_COUNT; i++)
    {
        gpu_buffer_upload(&chunk->gpu_voxels[i], &voxels[i]);
    }
    gpu_buffer_end_upload(&chunk->gpu_voxels[0]);
}

static void upload_lights(chunk_t* chunk, cpu_buffer_t* lights)
{
    CHECK(SDL_GetAtomicInt(&chunk->block_state) == JOB_STATE_COMPLETED);
    CHECK(SDL_GetAtomicInt(&chunk->light_state) == JOB_STATE_RUNNING);
    gpu_buffer_clear(&chunk->gpu_lights);
    if (!lights->size)
    {
        return;
    }
    if (!gpu_buffer_begin_upload(&chunk->gpu_lights))
    {
        return;
    }
    gpu_buffer_upload(&chunk->gpu_lights, lights);
    gpu_buffer_end_upload(&chunk->gpu_lights);
}

static bool is_visible(block_t block, block_t neighbor)
{
    if (neighbor == BLOCK_EMPTY)
    {
        return true;
    }
    if (block_is_sprite(neighbor))
    {
        return true;
    }
    if (block_is_opaque(block) && !block_is_opaque(neighbor))
    {
        return true;
    }
    return false;
}

static void gen_chunk_blocks(chunk_t* chunk)
{
    CHECK(SDL_GetAtomicInt(&chunk->block_state) == JOB_STATE_RUNNING);
    CHECK(SDL_GetAtomicInt(&chunk->voxel_state) == JOB_STATE_REQUESTED);
    CHECK(SDL_GetAtomicInt(&chunk->light_state) == JOB_STATE_REQUESTED);
    SDL_memset(chunk->blocks, 0, sizeof(chunk->blocks));
    map_clear(&chunk->lights);
    rand_get_blocks(chunk, chunk->x, chunk->z, set_chunk_block_function);
    save_get_blocks(chunk, chunk->x, chunk->z, set_chunk_block_function);
    SDL_SetAtomicInt(&chunk->block_state, JOB_STATE_COMPLETED);
    // Schedule because neighbors might have lights
    if (SDL_GetAtomicInt(&chunk->voxel_state) == JOB_STATE_REQUESTED)
    {
        SDL_SetAtomicInt(&chunk->light_state, JOB_STATE_REQUESTED);
    }
}

static void gen_chunk_voxels(chunk_t* chunks[3][3], cpu_buffer_t voxels[MESH_TYPE_COUNT])
{
    chunk_t* chunk = chunks[1][1];
    CHECK(SDL_GetAtomicInt(&chunk->block_state) == JOB_STATE_COMPLETED);
    CHECK(SDL_GetAtomicInt(&chunk->voxel_state) == JOB_STATE_RUNNING);
    for (int bx = 0; bx < CHUNK_WIDTH; bx++)
    for (int by = 0; by < CHUNK_HEIGHT; by++)
    for (int bz = 0; bz < CHUNK_WIDTH; bz++)
    {
        block_t block = chunk->blocks[bx][by][bz];
        if (block == BLOCK_EMPTY)
        {
            continue;
        }
        if (block_is_sprite(block))
        {
            for (int j = 0; j < 4; j++)
            for (int k = 0; k < 4; k++)
            {
                voxel_t voxel = voxel_pack_sprite(block, bx, by, bz, j, k);
                cpu_buffer_append(&voxels[MESH_TYPE_OPAQUE], &voxel);
            }
            continue;
        }
        for (int j = 0; j < 6; j++)
        {
            int dx = DIRECTIONS[j][0];
            int dy = DIRECTIONS[j][1];
            int dz = DIRECTIONS[j][2];
            block_t neighbor = get_neighborhood_block(chunks, bx, by, bz, dx, dy, dz);
            if (!is_visible(block, neighbor))
            {
                continue;
            }
            for (int k = 0; k < 4; k++)
            {
                voxel_t voxel = voxel_pack_cube(block, bx, by, bz, j, k);
                cpu_buffer_append(&voxels[get_mesh_for_block(block)], &voxel);
            }
        }
    }
    upload_voxels(chunk, voxels);
    SDL_SetAtomicInt(&chunk->voxel_state, JOB_STATE_COMPLETED);
}

static void gen_chunk_lights(chunk_t* chunks[3][3], cpu_buffer_t* lights)
{
    chunk_t* chunk = chunks[1][1];
    CHECK(SDL_GetAtomicInt(&chunk->block_state) == JOB_STATE_COMPLETED);
    CHECK(SDL_GetAtomicInt(&chunk->light_state) == JOB_STATE_RUNNING);
    for (int i = 0; i < 3; i++)
    for (int j = 0; j < 3; j++)
    {
        const chunk_t* neighbor = chunks[i][j];
        CHECK(neighbor);
        for (Uint32 i = 0; i < neighbor->lights.capacity; i++)
        {
            if (!map_is_row_valid(&neighbor->lights, i))
            {
                continue;
            }
            map_row_t row = map_get_row(&neighbor->lights, i);
            CHECK(row.value != BLOCK_EMPTY);
            CHECK(block_is_light(row.value));
            light_t light = block_get_light(row.value);
            light.x = neighbor->x + row.x;
            light.y = row.y;
            light.z = neighbor->z + row.z;
            cpu_buffer_append(lights, &light);
        }
    }
    upload_lights(chunk, lights);
    SDL_SetAtomicInt(&chunk->light_state, JOB_STATE_COMPLETED);
}

static void gen_lights()
{
    if (!gpu_buffer_begin_upload(&gpu_empty_lights))
    {
        return;
    }
    light_t light = {0};
    cpu_buffer_append(&cpu_empty_lights, &light);
    gpu_buffer_upload(&gpu_empty_lights, &cpu_empty_lights);
    gpu_buffer_end_upload(&gpu_empty_lights);
}

static void gen_indices(Uint32 size)
{
    SDL_LockMutex(mutex);
    size *= 1.5;
    if (gpu_indices.size >= size)
    {
        SDL_UnlockMutex(mutex);
        return;
    }
    if (!gpu_buffer_begin_upload(&gpu_indices))
    {
        SDL_UnlockMutex(mutex);
        return;
    }
    static const int INDICES[] = {0, 1, 2, 3, 2, 1};
    for (Uint32 i = 0; i < size; i++)
    for (Uint32 j = 0; j < 6; j++)
    {
        Uint32 index = i * 4 + INDICES[j];
        cpu_buffer_append(&cpu_indices, &index);
    }
    gpu_buffer_upload(&gpu_indices, &cpu_indices);
    gpu_buffer_end_upload(&gpu_indices);
    SDL_UnlockMutex(mutex);
}

static void wait_for_job(worker_t* worker)
{
    SDL_LockMutex(worker->mutex);
    while (worker->job.type == JOB_TYPE_NONE)
    {
        SDL_WaitCondition(worker->condition, worker->mutex);
    }
    SDL_UnlockMutex(worker->mutex);
}

static void wait_for_job_finish(const worker_t* worker)
{
    SDL_LockMutex(worker->mutex);
    while (worker->job.type != JOB_TYPE_NONE)
    {
        SDL_WaitCondition(worker->condition, worker->mutex);
    }
    SDL_UnlockMutex(worker->mutex);
}

static int clear_job(worker_t* worker)
{
    SDL_LockMutex(worker->mutex);
    SDL_assert(worker->job.type != JOB_TYPE_NONE);
    worker->job.type = JOB_TYPE_NONE;
    SDL_SignalCondition(worker->condition);
    SDL_UnlockMutex(worker->mutex);
    return 0;
}

static int worker_function(void* args)
{
    worker_t* worker = args;
    while (true)
    {
        wait_for_job(worker);
        job_t job = worker->job;
        if (job.type == JOB_TYPE_QUIT)
        {
            return clear_job(worker);
        }
        chunk_t* chunk = get_chunk(job.x, job.z);
        CHECK(chunk);
        if (job.type == JOB_TYPE_BLOCKS)
        {
            gen_chunk_blocks(chunk);
        }
        else
        {
            chunk_t* chunks[3][3];
            get_neighborhood(job.x, job.z, chunks);
            if (job.type == JOB_TYPE_VOXELS)
            {
                gen_chunk_voxels(chunks, worker->voxels);
                for (int i = 0; i < MESH_TYPE_COUNT; i++)
                {
                    gen_indices(chunk->gpu_voxels[i].size);
                }
            }
            else if (job.type == JOB_TYPE_LIGHTS)
            {
                gen_chunk_lights(chunks, &worker->lights);
            }
            else
            {
                CHECK(false);
            }
        }
        clear_job(worker);
    }
    return 0;
}

static bool is_job_running(const worker_t* worker)
{
    SDL_LockMutex(worker->mutex);
    if (worker->job.type != JOB_TYPE_NONE)
    {
        SDL_UnlockMutex(worker->mutex);
        return true;
    }
    else
    {
        SDL_UnlockMutex(worker->mutex);
        return false;
    }
}

static SDL_AtomicInt* get_job_state(chunk_t* chunk, job_type_t type)
{
    switch (type)
    {
    case JOB_TYPE_BLOCKS:
        return &chunk->block_state;
    case JOB_TYPE_VOXELS:
        return &chunk->voxel_state;
    case JOB_TYPE_LIGHTS:
        return &chunk->light_state;
    default:
        return NULL;
    }
}

static void dispatch_job(worker_t* worker, const job_t* job)
{
    if (job->type != JOB_TYPE_QUIT)
    {
        CHECK(!is_job_running(worker));
        chunk_t* chunk = get_chunk(job->x, job->z);
        SDL_SetAtomicInt(get_job_state(chunk, job->type), JOB_STATE_RUNNING);
    }
    else
    {
        wait_for_job_finish(worker);
    }
    SDL_LockMutex(worker->mutex);
    CHECK(worker->job.type == JOB_TYPE_NONE);
    worker->job = *job;
    SDL_SignalCondition(worker->condition);
    SDL_UnlockMutex(worker->mutex);
}

static void start_worker(worker_t* worker)
{
    for (int i = 0; i < MESH_TYPE_COUNT; i++)
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
    worker->thread = SDL_CreateThread(worker_function, "worker", worker);
    if (!worker->thread)
    {
        SDL_Log("Failed to create thread: %s", SDL_GetError());
    }
}

static void stop_worker(worker_t* worker)
{
    job_t job = {0};
    job.type = JOB_TYPE_QUIT;
    dispatch_job(worker, &job);
    SDL_WaitThread(worker->thread, NULL);
    SDL_DestroyMutex(worker->mutex);
    SDL_DestroyCondition(worker->condition);
    worker->thread = NULL;
    worker->mutex = NULL;
    worker->condition = NULL;
    for (int i = 0; i < MESH_TYPE_COUNT; i++)
    {
        cpu_buffer_free(&worker->voxels[i]);
    }
    cpu_buffer_free(&worker->lights);
}

static int sort_function(void* userdata, const void* lhs, const void* rhs)
{
    int w = WORLD_WIDTH / 2;
    const int* l = lhs;
    const int* r = rhs;
    int a = (l[0] - w) * (l[0] - w) + (l[1] - w) * (l[1] - w);
    int b = (r[0] - w) * (r[0] - w) + (r[1] - w) * (r[1] - w);
    if (a < b)
    {
        return -1;
    }
    else if (a > b)
    {
        return 1;
    }
    return 0;
}

void world_init(SDL_GPUDevice* handle)
{
    mutex = SDL_CreateMutex();
    if (!mutex)
    {
        SDL_Log("Failed to create mutex: %s", SDL_GetError());
    }
    device = handle;
    world_x = SDL_MAX_SINT32;
    world_z = SDL_MAX_SINT32;
    cpu_buffer_init(&cpu_indices, device, sizeof(Uint32));
    gpu_buffer_init(&gpu_indices, device, SDL_GPU_BUFFERUSAGE_INDEX);
    cpu_buffer_init(&cpu_empty_lights, device, sizeof(light_t));
    gpu_buffer_init(&gpu_empty_lights, device, SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ);
    for (int i = 0; i < MESH_TYPE_COUNT; i++)
    {
        cpu_buffer_init(&cpu_voxels[i], device, sizeof(voxel_t));
    }
    for (int i = 0; i < WORKERS; i++)
    {
        start_worker(&workers[i]);
    }
    for (int x = 0; x < WORLD_WIDTH; x++)
    for (int z = 0; z < WORLD_WIDTH; z++)
    {
        chunks[x][z] = create_chunk();
        sorted_chunks[x][z][0] = x;
        sorted_chunks[x][z][1] = z;
    }
    SDL_qsort_r(sorted_chunks, WORLD_WIDTH * WORLD_WIDTH, sizeof(int) * 2, sort_function, NULL);
    gen_lights();
    gen_indices(1000000);
}

void world_free()
{
    for (int i = 0; i < WORKERS; i++)
    {
        stop_worker(&workers[i]);
    }
    for (int x = 0; x < WORLD_WIDTH; x++)
    for (int z = 0; z < WORLD_WIDTH; z++)
    {
        free_chunk(chunks[x][z]);
    }
    cpu_buffer_free(&cpu_indices);
    gpu_buffer_free(&gpu_indices);
    cpu_buffer_free(&cpu_empty_lights);
    gpu_buffer_free(&gpu_empty_lights);
    for (int i = 0; i < MESH_TYPE_COUNT; i++)
    {
        cpu_buffer_free(&cpu_voxels[i]);
    }
    SDL_DestroyMutex(mutex);
}

static int get_running_count()
{
    int num_running = 0;
    for (int i = 0; i < WORKERS; i++)
    {
        if (is_job_running(&workers[i]))
        {
            num_running++;
        }
    }
    return num_running;
}

static int get_workers(worker_t* local_workers[WORKERS])
{
    int num_workers = 0;
    for (int i = 0; i < WORKERS; i++)
    {
        if (!is_job_running(&workers[i]))
        {
            local_workers[num_workers++] = &workers[i];
        }
    }
    return num_workers;
}

static void shuffle(int offset_x, int offset_z)
{
    world_x += offset_x;
    world_z += offset_z;
    chunk_t* in[WORLD_WIDTH][WORLD_WIDTH] = {0};
    chunk_t* out[WORLD_WIDTH * WORLD_WIDTH] = {0};
    int size = 0;
    for (int x = 0; x < WORLD_WIDTH; x++)
    for (int z = 0; z < WORLD_WIDTH; z++)
    {
        CHECK(chunks[x][z]);
        const int a = x - offset_x;
        const int b = z - offset_z;
        if (is_chunk_local(a, b))
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
            CHECK(size > 0);
            chunk_t* chunk = out[--size];
            SDL_SetAtomicInt(&chunk->block_state, JOB_STATE_REQUESTED);
            SDL_SetAtomicInt(&chunk->voxel_state, JOB_STATE_REQUESTED);
            SDL_SetAtomicInt(&chunk->light_state, JOB_STATE_REQUESTED);
            chunks[x][z] = chunk;
        }
        chunk_t* chunk = chunks[x][z];
        chunk->x = (world_x + x) * CHUNK_WIDTH;
        chunk->z = (world_z + z) * CHUNK_WIDTH;
    }
    CHECK(!size);
    is_moving = false;
}

static void move_chunks(const camera_t* camera)
{
    const int offset_x = camera->x / CHUNK_WIDTH - WORLD_WIDTH / 2 - world_x;
    const int offset_z = camera->z / CHUNK_WIDTH - WORLD_WIDTH / 2 - world_z;
    if (offset_x || offset_z)
    {
        is_moving = true;
        if (!get_running_count())
        {
            shuffle(offset_x, offset_z);
        }
    }
}

static bool try_update_blocks(int x, int z, worker_t* worker)
{
    CHECK(is_chunk_local(x, z));
    chunk_t* chunk = chunks[x][z];
    if (SDL_GetAtomicInt(&chunk->block_state) != JOB_STATE_REQUESTED)
    {
        return false;
    }
    job_t job = {JOB_TYPE_BLOCKS, x, z};
    CHECK(!is_job_running(worker));
    dispatch_job(worker, &job);
    return true;
}

static bool try_update_voxels_or_lights(int x, int z, worker_t* worker)
{
    CHECK(is_chunk_local(x, z));
    chunk_t* chunk = chunks[x][z];
    if (is_chunk_on_border(x, z))
    {
        return false;
    }
    bool do_voxel = SDL_GetAtomicInt(&chunk->voxel_state) == JOB_STATE_REQUESTED;
    bool do_light = SDL_GetAtomicInt(&chunk->light_state) == JOB_STATE_REQUESTED;
    if (!do_voxel && !do_light)
    {
        return false;;
    }
    chunk_t* neighborhood[3][3];
    get_neighborhood(x, z, neighborhood);
    for (int i = 0; i < 3; i++)
    for (int j = 0; j < 3; j++)
    {
        if (SDL_GetAtomicInt(&neighborhood[i][j]->block_state) != JOB_STATE_COMPLETED)
        {
            return false;
        }
    }
    job_t job;
    if (do_voxel)
    {
        job = (job_t) {JOB_TYPE_VOXELS, x, z};
    }
    else if (do_light)
    {
        job = (job_t) {JOB_TYPE_LIGHTS, x, z};
    }
    else
    {
        CHECK(false);
    }
    dispatch_job(worker, &job);
    return true;
}

void world_update(const camera_t* camera)
{
    move_chunks(camera);
    if (is_moving)
    {
        return;
    }
    worker_t* local_workers[WORKERS] = {0};
    int num_workers = get_workers(local_workers);
    for (int x = 0; x < WORLD_WIDTH; x++)
    for (int z = 0; z < WORLD_WIDTH; z++)
    {
        if (num_workers == 0)
        {
            return;
        }
        int a = sorted_chunks[x][z][0];
        int b = sorted_chunks[x][z][1];
        worker_t* worker = local_workers[num_workers - 1];
        if (try_update_blocks(a, b, worker))
        {
            num_workers--;
        }
        else if (try_update_voxels_or_lights(a, b, worker))
        {
            num_workers--;
        }
    }
}

static void render(chunk_t* chunk, SDL_GPUCommandBuffer* cbuf, SDL_GPURenderPass* pass, world_flags_t flags)
{
    gpu_buffer_t* gpu_voxels = &chunk->gpu_voxels[get_mesh_for_flags(flags)];
    if (gpu_voxels->size == 0)
    {
        return;
    }
    SDL_GPUBufferBinding voxel_binding = {0};
    SDL_GPUBufferBinding index_binding = {0};
    voxel_binding.buffer = gpu_voxels->buffer;
    index_binding.buffer = gpu_indices.buffer;
    if (flags & WORLD_FLAGS_LIGHT)
    {
        Sint32 light_count;
        SDL_GPUBuffer* light_binding;
        if (SDL_GetAtomicInt(&chunk->light_state) == JOB_STATE_COMPLETED && chunk->gpu_lights.size)
        {
            light_binding = chunk->gpu_lights.buffer;
            light_count = chunk->gpu_lights.size;
        }
        else
        {
            light_binding = gpu_empty_lights.buffer;
            light_count = 0;
        }
        SDL_PushGPUFragmentUniformData(cbuf, 0, &light_count, 4);
        SDL_BindGPUFragmentStorageBuffers(pass, 0, &light_binding, 1);
    }
    SDL_PushGPUVertexUniformData(cbuf, 2, chunk->position, 12);
    SDL_BindGPUVertexBuffers(pass, 0, &voxel_binding, 1);
    SDL_BindGPUIndexBuffer(pass, &index_binding, SDL_GPU_INDEXELEMENTSIZE_32BIT);
    SDL_DrawGPUIndexedPrimitives(pass, gpu_voxels->size * 1.5, 1, 0, 0, 0);
}

void world_render(const camera_t* camera, SDL_GPUCommandBuffer* cbuf, SDL_GPURenderPass* pass, world_flags_t flags)
{
    SDL_PushGPUVertexUniformData(cbuf, 0, camera->proj, 64);
    SDL_PushGPUVertexUniformData(cbuf, 1, camera->view, 64);
    for (int x = 0; x < WORLD_WIDTH; x++)
    for (int y = 0; y < WORLD_WIDTH; y++)
    {
        int a = sorted_chunks[x][y][0];
        int b = sorted_chunks[x][y][1];
        if (is_chunk_on_border(a, b))
        {
            continue;
        }
        chunk_t* chunk = chunks[a][b];
        if (SDL_GetAtomicInt(&chunk->voxel_state) != JOB_STATE_COMPLETED)
        {
            continue;
        }
        float sx = CHUNK_WIDTH;
        float sy = CHUNK_HEIGHT;
        float sz = CHUNK_WIDTH;
        if (!camera_get_vis(camera, chunk->x, 0.0f, chunk->z, sx, sy, sz))
        {
            continue;
        }
        render(chunk, cbuf, pass, flags);
    }
}

block_t world_get_block(const int position[3])
{
    if (position[1] < 0 || position[1] >= CHUNK_HEIGHT)
    {
        return BLOCK_EMPTY;
    }
    int chunk_x = floor_chunk_index(position[0] - world_x * CHUNK_WIDTH);
    int chunk_z = floor_chunk_index(position[2] - world_z * CHUNK_WIDTH);
    chunk_t* chunk = get_chunk(chunk_x, chunk_z);
    if (chunk)
    {
        CHECK(chunk->x == (world_x + chunk_x) * CHUNK_WIDTH);
        CHECK(chunk->z == (world_z + chunk_z) * CHUNK_WIDTH);
    }
    else
    {
        SDL_Log("Bad chunk position: %d, %d", chunk_x, chunk_z);
        return BLOCK_EMPTY;
    }
    bool has_blocks = SDL_GetAtomicInt(&chunk->block_state) == JOB_STATE_COMPLETED;
    bool has_voxels = SDL_GetAtomicInt(&chunk->voxel_state) == JOB_STATE_COMPLETED;
    if (has_blocks && has_voxels)
    {
        return get_chunk_block(chunk, position[0], position[1], position[2]);
    }
    else
    {
        return BLOCK_EMPTY;
    }
}

static void gen_voxels_sync(int x, int z)
{
    CHECK(!is_chunk_on_border(x, z));
    chunk_t* chunks[3][3] = {0};
    get_neighborhood(x, z, chunks);
    SDL_SetAtomicInt(&chunks[1][1]->voxel_state, JOB_STATE_RUNNING);
    gen_chunk_voxels(chunks, cpu_voxels);
}

void world_set_block(const int position[3], block_t block)
{
    if (position[1] < 0 || position[1] >= CHUNK_HEIGHT)
    {
        return;
    }
    int chunk_x = floor_chunk_index(position[0] - world_x * CHUNK_WIDTH);
    int chunk_z = floor_chunk_index(position[2] - world_z * CHUNK_WIDTH);
    chunk_t* chunk = get_chunk(chunk_x, chunk_z);
    if (chunk)
    {
        CHECK(chunk->x == (world_x + chunk_x) * CHUNK_WIDTH);
        CHECK(chunk->z == (world_z + chunk_z) * CHUNK_WIDTH);
    }
    else
    {
        SDL_Log("Bad chunk position: %d, %d", chunk_x, chunk_z);
        return;
    }
    bool has_blocks = SDL_GetAtomicInt(&chunk->block_state) == JOB_STATE_COMPLETED;
    bool has_voxels = SDL_GetAtomicInt(&chunk->voxel_state) == JOB_STATE_COMPLETED;
    if (!has_blocks || !has_voxels)
    {
        return;
    }
    save_set_block(chunk->x, chunk->z, position[0], position[1], position[2], block);
    int local_x = position[0];
    int local_y = position[1];
    int local_z = position[2];
    world_to_chunk(chunk, &local_x, &local_y, &local_z);
    block_t old_block = set_chunk_block(chunk, position[0], position[1], position[2], block);
    gen_voxels_sync(chunk_x, chunk_z);
    if (local_x == 0)
    {
        gen_voxels_sync(chunk_x - 1, chunk_z);
    }
    else if (local_x == CHUNK_WIDTH - 1)
    {
        gen_voxels_sync(chunk_x + 1, chunk_z);
    }
    if (local_z == 0)
    {
        gen_voxels_sync(chunk_x, chunk_z - 1);
    }
    else if (local_z == CHUNK_WIDTH - 1)
    {
        gen_voxels_sync(chunk_x, chunk_z + 1);
    }
    chunk_t* neighborhood[3][3] = {0};
    get_neighborhood(chunk_x, chunk_z, neighborhood);
    if (block_is_light(block) || block_is_light(old_block))
    {
        for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
        {
            SDL_SetAtomicInt(&neighborhood[i][j]->light_state, JOB_STATE_REQUESTED);
        }
    }
}

world_query_t world_raycast(const camera_t* camera, float length)
{
    world_query_t query = {0};
    float direction[3] = {0};
    float distances[3] = {0};
    int steps[3] = {0};
    float deltas[3] = {0};
    camera_get_vector(camera, &direction[0], &direction[1], &direction[2]);
    for (int i = 0; i < 3; i++)
    {
        query.current[i] = SDL_floorf(camera->position[i]);
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
            distances[i] = (camera->position[i] - query.current[i]) * deltas[i];
        }
        else
        {
            steps[i] = 1;
            distances[i] = (query.current[i] + 1.0f - camera->position[i]) * deltas[i];
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
