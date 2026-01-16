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
    SDL_AtomicInt block_job;
    SDL_AtomicInt voxel_job;
    SDL_AtomicInt light_job;
    int x;
    int z;
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
static cpu_buffer_t gpu_voxels[MESH_TYPE_COUNT];
static cpu_buffer_t gpu_lights;
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

static chunk_t* create_chunk()
{
    chunk_t* chunk = SDL_malloc(sizeof(chunk_t));
    if (!chunk)
    {
        SDL_Log("Failed to allocate chunk");
    }
    SDL_SetAtomicInt(&chunk->block_job, JOB_STATE_REQUESTED);
    SDL_SetAtomicInt(&chunk->voxel_job, JOB_STATE_COMPLETED);
    SDL_SetAtomicInt(&chunk->light_job, JOB_STATE_COMPLETED);
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
    map_free(&chunk->lights);
    SDL_free(chunk);
}

static block_t set_chunk_block(chunk_t* chunk, int bx, int by, int bz, block_t block)
{
    SDL_SetAtomicInt(&chunk->voxel_job, JOB_STATE_REQUESTED);
    world_to_chunk(chunk, &bx, &by, &bz);
    chunk->blocks[bx][by][bz] = block;
    block_t old_block = map_get(&chunk->lights, bx, by, bz);
    if (!block_is_light(block) && !block_is_light(old_block))
    {
        return old_block;
    }
    SDL_SetAtomicInt(&chunk->light_job, JOB_STATE_REQUESTED);
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
    CHECK(SDL_GetAtomicInt(&chunk->block_job) == JOB_STATE_RUNNING);
    set_chunk_block(userdata, bx, by, bz, block);
}

static block_t get_chunk_block(chunk_t* chunk, int bx, int by, int bz)
{
    CHECK(SDL_GetAtomicInt(&chunk->block_job) == JOB_STATE_COMPLETED);
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

static bool is_face_visible(block_t block, block_t neighbor)
{
    bool is_opaque = block_is_opaque(block);
    bool is_sprite = block_is_sprite(neighbor);
    bool is_opaque_next_to_transparent = is_opaque && !block_is_opaque(neighbor);
    return neighbor == BLOCK_EMPTY || is_sprite || is_opaque_next_to_transparent;
}

static mesh_type_t get_block_mesh(block_t block)
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

static void gen_blocks(chunk_t* chunk)
{
    CHECK(SDL_GetAtomicInt(&chunk->block_job) == JOB_STATE_RUNNING);
    SDL_memset(chunk->blocks, 0, sizeof(chunk->blocks));
    map_clear(&chunk->lights);
    rand_get_blocks(chunk, chunk->x, chunk->z, set_chunk_block_function);
    save_get_blocks(chunk, chunk->x, chunk->z, set_chunk_block_function);
    SDL_SetAtomicInt(&chunk->block_job, JOB_STATE_COMPLETED);
    if (SDL_GetAtomicInt(&chunk->voxel_job) == JOB_STATE_REQUESTED)
    {
        SDL_SetAtomicInt(&chunk->light_job, JOB_STATE_REQUESTED);
    }
}

static void gen_voxels(chunk_t* chunks[3][3], cpu_buffer_t voxels[MESH_TYPE_COUNT])
{
    chunk_t* chunk = chunks[1][1];
    CHECK(SDL_GetAtomicInt(&chunk->block_job) == JOB_STATE_COMPLETED);
    CHECK(SDL_GetAtomicInt(&chunk->voxel_job) == JOB_STATE_RUNNING);
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
            if (!is_face_visible(block, neighbor))
            {
                continue;
            }
            for (int k = 0; k < 4; k++)
            {
                voxel_t voxel = voxel_pack_cube(block, bx, by, bz, j, k);
                cpu_buffer_append(&voxels[get_block_mesh(block)], &voxel);
            }
        }
    }
    upload_voxels(chunk, voxels);
    SDL_SetAtomicInt(&chunk->voxel_job, JOB_STATE_COMPLETED);
}

static void gen_chunk_lights(chunk_t* chunks[3][3], cpu_buffer_t* lights)
{
    chunk_t* chunk = chunks[1][1];
    CHECK(SDL_GetAtomicInt(&chunk->block_job) == JOB_STATE_COMPLETED);
    CHECK(SDL_GetAtomicInt(&chunk->light_job) == JOB_STATE_RUNNING);
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
    SDL_SetAtomicInt(&chunk->light_job, JOB_STATE_COMPLETED);
}

static void gen_indices_impl(Uint32 size)
{
    size *= 1.5;
    if (gpu_indices.size >= size)
    {
        return;
    }
    if (!gpu_buffer_begin_upload(&gpu_indices))
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
    gpu_buffer_upload(&gpu_indices, &cpu_indices);
    gpu_buffer_end_upload(&gpu_indices);
}

static void gen_indices(Uint32 size)
{
    SDL_LockMutex(mutex);
    gen_indices_impl(size);
    SDL_UnlockMutex(mutex);
}

static job_t wait_for_job(worker_t* worker)
{
    SDL_LockMutex(worker->mutex);
    while (worker->job.type == JOB_TYPE_NONE)
    {
        SDL_WaitCondition(worker->condition, worker->mutex);
    }
    job_t job = worker->job;
    SDL_UnlockMutex(worker->mutex);
    return job;
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

static void clear_job(worker_t* worker)
{
    SDL_LockMutex(worker->mutex);
    worker->job.type = JOB_TYPE_NONE;
    SDL_SignalCondition(worker->condition);
    SDL_UnlockMutex(worker->mutex);
}

static int worker_func(void* args)
{
    worker_t* worker = args;
    while (true)
    {
        job_t job = wait_for_job(worker);
        if (job.type == JOB_TYPE_QUIT)
        {
            clear_job(worker);
            return 0;
        }
        chunk_t* chunk = get_chunk(job.x, job.z);
        CHECK(chunk);
        if (job.type == JOB_TYPE_BLOCKS)
        {
            gen_blocks(chunk);
        }
        else
        {
            chunk_t* chunks[3][3];
            get_neighborhood(job.x, job.z, chunks);
            if (job.type == JOB_TYPE_VOXELS)
            {
                gen_voxels(chunks, worker->voxels);
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
    worker->thread = SDL_CreateThread(worker_func, "worker", worker);
    if (!worker->thread)
    {
        SDL_Log("Failed to create thread: %s", SDL_GetError());
    }
}

static bool is_working(const worker_t* worker)
{
    SDL_LockMutex(worker->mutex);
    bool working = worker->job.type != JOB_TYPE_NONE;
    SDL_UnlockMutex(worker->mutex);
    return working;
}

static void dispatch_to_worker(worker_t* worker, const job_t* job)
{
    if (job->type != JOB_TYPE_QUIT)
    {
        CHECK(!is_working(worker));
        chunk_t* chunk = get_chunk(job->x, job->z);
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
            CHECK(false);
        }
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

static void stop_worker(worker_t* worker)
{
    job_t job = {0};
    job.type = JOB_TYPE_QUIT;
    dispatch_to_worker(worker, &job);
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

static int sort_compare_func(void* userdata, const void* lhs, const void* rhs)
{
    int cx = ((int*) userdata)[0];
    int cy = ((int*) userdata)[1];
    const int* l = lhs;
    const int* r = rhs;
    int a = (l[0] - cx) * (l[0] - cx) + (l[1] - cy) * (l[1] - cy);
    int b = (r[0] - cx) * (r[0] - cx) + (r[1] - cy) * (r[1] - cy);
    return (a > b) - (a < b);
}

static void create_empty_lights()
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

void world_init(SDL_GPUDevice* handle)
{
    device = handle;
    world_x = SDL_MAX_SINT32;
    world_z = SDL_MAX_SINT32;
    cpu_buffer_init(&cpu_indices, device, sizeof(Uint32));
    gpu_buffer_init(&gpu_indices, device, SDL_GPU_BUFFERUSAGE_INDEX);
    cpu_buffer_init(&cpu_empty_lights, device, sizeof(light_t));
    gpu_buffer_init(&gpu_empty_lights, device, SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ);
    for (int i = 0; i < MESH_TYPE_COUNT; i++)
    {
        cpu_buffer_init(&gpu_voxels[i], device, sizeof(voxel_t));
    }
    cpu_buffer_init(&gpu_lights, device, sizeof(light_t));
    for (int i = 0; i < WORKERS; i++)
    {
        start_worker(&workers[i]);
    }
    for (int x = 0; x < WORLD_WIDTH; x++)
    for (int z = 0; z < WORLD_WIDTH; z++)
    {
        chunks[x][z] = create_chunk();
    }
    for (int x = 0; x < WORLD_WIDTH; x++)
    for (int z = 0; z < WORLD_WIDTH; z++)
    {
        sorted_chunks[x][z][0] = x;
        sorted_chunks[x][z][1] = z;
    }
    int w = WORLD_WIDTH;
    SDL_qsort_r(sorted_chunks, w * w, sizeof(int) * 2, sort_compare_func, (int[]) {w / 2, w / 2});
    create_empty_lights();
    mutex = SDL_CreateMutex();
    if (!mutex)
    {
        SDL_Log("Failed to create mutex: %s", SDL_GetError());
    }
    gen_indices(1000000);
}

void world_free()
{
    SDL_DestroyMutex(mutex);
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
        cpu_buffer_free(&gpu_voxels[i]);
    }
    cpu_buffer_free(&gpu_lights);
}

static int get_working_count()
{
    int count = 0;
    for (int i = 0; i < WORKERS; i++)
    {
        count += is_working(&workers[i]);
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
        is_moving = true;
        return;
    }
    is_moving = false;
    world_x = camera_x;
    world_z = camera_z;
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
            SDL_SetAtomicInt(&chunk->block_job, JOB_STATE_REQUESTED);
            chunks[x][z] = chunk;
        }
        chunk_t* chunk = chunks[x][z];
        chunk->x = (world_x + x) * CHUNK_WIDTH;
        chunk->z = (world_z + z) * CHUNK_WIDTH;
    }
    CHECK(!size);
}

static void update_chunks()
{
    worker_t* local_workers[WORKERS] = {0};
    job_t jobs[WORKERS] = {0};
    int num_workers = 0;
    for (int i = 0; i < WORKERS; i++)
    {
        if (!is_working(&workers[i]))
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
            jobs[num_workers] = (job_t) {JOB_TYPE_BLOCKS, a, b};
            CHECK(!is_working(local_workers[num_workers]));
            dispatch_to_worker(local_workers[num_workers], &jobs[num_workers]);
            if (num_workers == 0)
            {
                return;
            }
            continue;
        }
        if (is_chunk_on_border(a, b))
        {
            continue;
        }
        if (SDL_GetAtomicInt(&chunk->voxel_job) == JOB_STATE_REQUESTED || SDL_GetAtomicInt(&chunk->light_job) == JOB_STATE_REQUESTED)
        {
            bool should_work = true;
            chunk_t* local_chunks[3][3];
            get_neighborhood(a, b, local_chunks);
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
                jobs[num_workers] = (job_t) {JOB_TYPE_VOXELS, a, b};
            }
            else if (SDL_GetAtomicInt(&chunk->light_job) == JOB_STATE_REQUESTED)
            {
                jobs[num_workers] = (job_t) {JOB_TYPE_LIGHTS, a, b};
            }
            else
            {
                CHECK(false);
            }
            dispatch_to_worker(local_workers[num_workers], &jobs[num_workers]);
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
    if (!is_moving)
    {
        update_chunks();
    }
}

void world_render(const camera_t* camera, SDL_GPUCommandBuffer* command_buffer, SDL_GPURenderPass* render_pass, world_flag_t flags)
{
    CHECK(!(flags & WORLD_FLAG_OPAQUE) || !(flags & WORLD_FLAG_TRANSPARENT));
    mesh_type_t mesh;
    if (flags & WORLD_FLAG_OPAQUE)
    {
        mesh = MESH_TYPE_OPAQUE;
    }
    else
    {
        mesh = MESH_TYPE_TRANSPARENT;
    }
    bool use_lights = flags & WORLD_FLAG_LIGHT;
    SDL_PushGPUDebugGroup(command_buffer, "world");
    SDL_PushGPUVertexUniformData(command_buffer, 0, camera->proj, 64);
    SDL_PushGPUVertexUniformData(command_buffer, 1, camera->view, 64);
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
        if (SDL_GetAtomicInt(&chunk->voxel_job) != JOB_STATE_COMPLETED)
        {
            continue;
        }
        gpu_buffer_t* gpu_voxels = &chunk->gpu_voxels[mesh];
        if (!gpu_voxels->size)
        {
            continue;
        }
        int w = CHUNK_WIDTH;
        int h = CHUNK_HEIGHT;
        if (!camera_get_vis(camera, chunk->x, 0, chunk->z, w, h, w))
        {
            continue;
        }
        float position[] = {chunk->x, 0, chunk->z};
        SDL_GPUBufferBinding voxel_binding = {gpu_voxels->buffer};
        SDL_GPUBufferBinding index_binding = {gpu_indices.buffer};
        if (use_lights)
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

block_t world_get_block(const int index[3])
{
    if (index[1] < 0 || index[1] >= CHUNK_HEIGHT)
    {
        return BLOCK_EMPTY;
    }
    int chunk_x = floor_chunk_index(index[0] - world_x * CHUNK_WIDTH);
    int chunk_z = floor_chunk_index(index[2] - world_z * CHUNK_WIDTH);
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
    if (SDL_GetAtomicInt(&chunk->block_job) != JOB_STATE_COMPLETED || SDL_GetAtomicInt(&chunk->voxel_job) != JOB_STATE_COMPLETED)
    {
        return BLOCK_EMPTY;
    }
    return get_chunk_block(chunk, index[0], index[1], index[2]);
}

static void set_voxels(int x, int z)
{
    CHECK(!is_chunk_on_border(x, z));
    chunk_t* chunks[3][3] = {0};
    get_neighborhood(x, z, chunks);
    SDL_SetAtomicInt(&chunks[1][1]->voxel_job, JOB_STATE_RUNNING);
    gen_voxels(chunks, gpu_voxels);
}

void world_set_block(const int index[3], block_t block)
{
    if (index[1] < 0 || index[1] >= CHUNK_HEIGHT)
    {
        return;
    }
    int chunk_x = floor_chunk_index(index[0] - world_x * CHUNK_WIDTH);
    int chunk_z = floor_chunk_index(index[2] - world_z * CHUNK_WIDTH);
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
    if (SDL_GetAtomicInt(&chunk->block_job) != JOB_STATE_COMPLETED || SDL_GetAtomicInt(&chunk->voxel_job) != JOB_STATE_COMPLETED)
    {
        return;
    }
    block_t old_block = set_chunk_block(chunk, index[0], index[1], index[2], block);
    set_voxels(chunk_x, chunk_z);
    int local_x = index[0];
    int local_y = index[1];
    int local_z = index[2];
    world_to_chunk(chunk, &local_x, &local_y, &local_z);
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
    get_neighborhood(chunk_x, chunk_z, local_chunks);
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

world_raycast_t world_raycast(const camera_t* camera, float length)
{
    world_raycast_t query = {0};
    float start[3] = {0};
    float direction[3] = {0};
    float distances[3] = {0};
    int steps[3] = {0};
    float deltas[3] = {0};
    camera_get_vector(camera, &direction[0], &direction[1], &direction[2]);
    for (int i = 0; i < 3; i++)
    {
        start[i] = camera->position[i];
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
