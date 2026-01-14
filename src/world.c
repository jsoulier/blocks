// moved voxels, workers, chunks, noise, lights, sort, in here

#include <SDL3/SDL.h>

#include "block.h"
#include "buffer.h"
#include "camera.h"
#include "chunk.h"
#include "light.h"
#include "save.h"
#include "sort.h"
#include "voxel.h"
#include "worker.h"
#include "world.h"

#define WORKERS 4

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
static cpu_buffer_t gpu_voxels[CHUNK_MESH_TYPE_COUNT];
static cpu_buffer_t gpu_lights;

static bool is_local(int x, int z)
{
    return x >= 0 && z >= 0 && x < WORLD_WIDTH && z < WORLD_WIDTH;
}

static bool is_bordering(int x, int z)
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
    for (int i = 0; i < CHUNK_MESH_TYPE_COUNT; i++)
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
        chunk_init(chunks[x][z], device);
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
    for (int i = 0; i < CHUNK_MESH_TYPE_COUNT; i++)
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
        if (is_local(a, b))
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
            SDL_SetAtomicInt(&chunk->set_blocks, true);
            SDL_SetAtomicInt(&chunk->has_blocks, false);
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
        if (SDL_GetAtomicInt(&chunk->set_blocks))
        {
            num_workers--;
            jobs[num_workers] = (worker_job_t) {WORKER_JOB_TYPE_SET_BLOCKS, a, b};
            SDL_assert(!worker_is_working(local_workers[num_workers]));
            worker_dispatch(local_workers[num_workers], &jobs[num_workers]);
            if (num_workers == 0)
            {
                return;
            }
            continue;
        }
        if (is_bordering(a, b))
        {
            continue;
        }
        if (SDL_GetAtomicInt(&chunk->set_voxels) || SDL_GetAtomicInt(&chunk->set_lights))
        {
            bool should_work = true;
            chunk_t* local_chunks[3][3];
            world_get_chunks(a, b, local_chunks);
            for (int i = 0; i < 3; i++)
            for (int j = 0; j < 3; j++)
            {
                if (!SDL_GetAtomicInt(&local_chunks[i][j]->has_blocks))
                {
                    should_work = false;
                }
            }
            if (!should_work)
            {
                continue;
            }
            num_workers--;
            if (SDL_GetAtomicInt(&chunk->set_voxels))
            {
                jobs[num_workers] = (worker_job_t) {WORKER_JOB_TYPE_SET_VOXELS, a, b};
            }
            else if (SDL_GetAtomicInt(&chunk->set_lights))
            {
                jobs[num_workers] = (worker_job_t) {WORKER_JOB_TYPE_SET_LIGHTS, a, b};
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

void world_render(const world_render_data_t* data)
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
        if (is_bordering(a, b))
        {
            continue;
        }
        chunk_t* chunk = chunks[a][b];
        if (!SDL_GetAtomicInt(&chunk->has_voxels))
        {
            continue;
        }
        gpu_buffer_t* gpu_voxels = &chunk->gpu_voxels[data->type];
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
        if (data->use_lights)
        {
            Sint32 light_count = 0;
            if (!SDL_GetAtomicInt(&chunk->has_lights) || !chunk->gpu_lights.size)
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
    if (is_local(x, z))
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
    if (!SDL_GetAtomicInt(&chunk->has_blocks) || SDL_GetAtomicInt(&chunk->set_voxels))
    {
        return BLOCK_EMPTY;
    }
    return chunk_get_block(chunk, index[0], index[1], index[2]);
}

static void set_voxels(int x, int z)
{
    SDL_assert(!is_bordering(x, z));
    chunk_t* chunks[3][3] = {0};
    world_get_chunks(x, z, chunks);

    // todo: this shit has gotta be at the chunk level like do_request_set_blocks
    SDL_SetAtomicInt(&chunks[1][1]->set_voxels, false);
    SDL_SetAtomicInt(&chunks[1][1]->has_voxels, false);

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
    if (!SDL_GetAtomicInt(&chunk->has_blocks) || SDL_GetAtomicInt(&chunk->set_voxels))
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
            SDL_SetAtomicInt(&local_chunks[i + 1][j + 1]->set_lights, true);
        }
    }
    save_set_block(chunk->x, chunk->z, index[0], index[1], index[2], block);
}

world_query_t world_query(float x, float y, float z, float dx, float dy, float dz, float length)
{
    world_query_t query = {0};
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
