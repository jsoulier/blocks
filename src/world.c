#include <SDL3/SDL.h>

#include "block.h"
#include "buffer.h"
#include "camera.h"
#include "rand.h"
#include "save.h"
#include "voxel.h"
#include "world.h"

#define WORKERS 4

static const int AO_LEVEL_CLEAR = 3;
static const Uint32 INDEX_COUNT = CHUNK_WIDTH * CHUNK_HEIGHT * CHUNK_WIDTH * DIRECTION_COUNT * 6;

typedef enum JobType
{
    JOB_TYPE_NONE,
    JOB_TYPE_QUIT,
    JOB_TYPE_BLOCKS,
    JOB_TYPE_VOXELS,
    JOB_TYPE_LIGHTS,
} JobType;

typedef enum JobState
{
    JOB_STATE_REQUESTED,
    JOB_STATE_RUNNING,
    JOB_STATE_COMPLETED,
} JobState;

typedef enum MeshType
{
    MESH_TYPE_OPAQUE,
    MESH_TYPE_TRANSPARENT,
    MESH_TYPE_COUNT,
} MeshType;

typedef struct Job
{
    JobType type;
    int x;
    int z;
} Job;

typedef struct Worker
{
    SDL_Thread* thread;
    SDL_Mutex* mutex;
    SDL_Condition* condition;
    Job job;
    CPUBuffer voxels[MESH_TYPE_COUNT];
    CPUBuffer lights;
} Worker;

typedef struct Chunk
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
    Block blocks[CHUNK_WIDTH][CHUNK_HEIGHT][CHUNK_WIDTH];
    Light* lights;
    int light_size;
    int light_capacity;
    GPUBuffer gpu_voxels[MESH_TYPE_COUNT];
    GPUBuffer gpu_lights;
} Chunk;

static SDL_GPUDevice* device;
static int world_x;
static int world_z;
static Worker workers[WORKERS];
static GPUBuffer gpu_indices;
static GPUBuffer gpu_empty_lights;
static CPUBuffer cpu_voxels[MESH_TYPE_COUNT];
static Chunk* chunks[WORLD_WIDTH][WORLD_WIDTH];
static int sorted_chunks[WORLD_WIDTH][WORLD_WIDTH][2];

static bool IsBlockLocal(int bx, int by, int bz)
{
    SDL_assert(by >= 0 && by < CHUNK_HEIGHT);
    SDL_assert(bx >= -1 && bz >= -1 && bx <= CHUNK_WIDTH && bz <= CHUNK_WIDTH);
    return bx >= 0 && bz >= 0 && bx < CHUNK_WIDTH && bz < CHUNK_WIDTH;
}

static void WorldToChunk(const Chunk* chunk, int* bx, int* by, int* bz)
{
    *bx -= chunk->x;
    *bz -= chunk->z;
    SDL_assert(IsBlockLocal(*bx, *by, *bz));
}

static bool IsChunkLocal(int cx, int cz)
{
    return cx >= 0 && cz >= 0 && cx < WORLD_WIDTH && cz < WORLD_WIDTH;
}

static bool IsChunkOnBorder(int cx, int cz)
{
    return cx == 0 || cz == 0 || cx == WORLD_WIDTH - 1 || cz == WORLD_WIDTH - 1;
}

static int FloorChunkIndex(float index)
{
    return SDL_floorf(index / CHUNK_WIDTH);
}

static Chunk* GetChunk(int cx, int cz)
{
    return IsChunkLocal(cx, cz) ? chunks[cx][cz] : NULL;
}

static void GetNeighborhood(int cx, int cz, Chunk* neighborhood[3][3])
{
    for (int i = -1; i <= 1; i++)
    {
        for (int j = -1; j <= 1; j++)
        {
            int x = cx + i;
            int z = cz + j;
            neighborhood[i + 1][j + 1] = GetChunk(x, z);
            SDL_assert(neighborhood[i + 1][j + 1]);
        }
    }
}

static Chunk* CreateChunk()
{
    Chunk* chunk = SDL_calloc(1, sizeof(Chunk));
    if (!chunk)
    {
        SDL_Log("Failed to allocate chunk");
    }
    SDL_SetAtomicInt(&chunk->block_state, JOB_STATE_REQUESTED);
    SDL_SetAtomicInt(&chunk->voxel_state, JOB_STATE_COMPLETED);
    SDL_SetAtomicInt(&chunk->light_state, JOB_STATE_COMPLETED);
    for (int i = 0; i < MESH_TYPE_COUNT; i++)
    {
        GPUBuffer_Init(&chunk->gpu_voxels[i], device, SDL_GPU_BUFFERUSAGE_VERTEX);
    }
    GPUBuffer_Init(&chunk->gpu_lights, device, SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ);
    return chunk;
}

static void FreeChunk(Chunk* chunk)
{
    GPUBuffer_Free(&chunk->gpu_lights);
    for (int i = 0; i < MESH_TYPE_COUNT; i++)
    {
        GPUBuffer_Free(&chunk->gpu_voxels[i]);
    }
    SDL_free(chunk->lights);
    SDL_free(chunk);
}

static Block SetChunkBlock(Chunk* chunk, int bx, int by, int bz, Block block)
{
    SDL_SetAtomicInt(&chunk->voxel_state, JOB_STATE_REQUESTED);
    WorldToChunk(chunk, &bx, &by, &bz);
    Block old_block = chunk->blocks[bx][by][bz];
    chunk->blocks[bx][by][bz] = block;
    if (!Block_IsLight(block) && !Block_IsLight(old_block))
    {
        return old_block;
    }
    SDL_SetAtomicInt(&chunk->light_state, JOB_STATE_REQUESTED);
    int light_index = -1;
    for (int i = 0; i < chunk->light_size; i++)
    {
        Light* light = &chunk->lights[i];
        if (light->x == chunk->x + bx && light->y == by && light->z == chunk->z + bz)
        {
            light_index = i;
            break;
        }
    }
    if (Block_IsLight(block))
    {
        if (light_index < 0)
        {
            if (chunk->light_size == chunk->light_capacity)
            {
                int capacity = SDL_max(8, chunk->light_capacity * 2);
                Light* lights = SDL_realloc(chunk->lights, capacity * sizeof(Light));
                if (!lights)
                {
                    SDL_Log("Failed to allocate chunk lights");
                    return old_block;
                }
                chunk->lights = lights;
                chunk->light_capacity = capacity;
            }
            light_index = chunk->light_size++;
        }
        Light light = Block_GetLight(block);
        light.x = chunk->x + bx;
        light.y = by;
        light.z = chunk->z + bz;
        chunk->lights[light_index] = light;
    }
    else if (light_index >= 0)
    {
        chunk->lights[light_index] = chunk->lights[--chunk->light_size];
    }
    return old_block;
}

static void SetChunkBlockFunction(void* userdata, int bx, int by, int bz, Block block)
{
    Chunk* chunk = userdata;
    SDL_assert(SDL_GetAtomicInt(&chunk->block_state) == JOB_STATE_RUNNING);
    SetChunkBlock(userdata, bx, by, bz, block);
}

static Block GetChunkBlock(Chunk* chunk, int bx, int by, int bz)
{
    SDL_assert(SDL_GetAtomicInt(&chunk->block_state) == JOB_STATE_COMPLETED);
    WorldToChunk(chunk, &bx, &by, &bz);
    return chunk->blocks[bx][by][bz];
}

static Block GetNeighborhoodBlock(Chunk* chunks[3][3], int bx, int by, int bz, int dx, int dy, int dz)
{
    SDL_assert(dx >= -1 && dx <= 1);
    SDL_assert(dy >= -1 && dy <= 1);
    SDL_assert(dz >= -1 && dz <= 1);
    bx += dx;
    by += dy;
    bz += dz;
    const Chunk* chunk = chunks[1][1];
    if (by == CHUNK_HEIGHT)
    {
        return BLOCK_EMPTY;
    }
    else if (by == -1)
    {
        return BLOCK_GRASS;
    }
    else if (IsBlockLocal(bx, by, bz))
    {
        return chunk->blocks[bx][by][bz];
    }
    SDL_assert(by >= 0 && by < CHUNK_HEIGHT);
    bx += chunk->x;
    bz += chunk->z;
    int cx = 1;
    int cz = 1;
    if (bx < chunk->x)
    {
        cx--;
    }
    else if (bx >= chunk->x + CHUNK_WIDTH)
    {
        cx++;
    }
    if (bz < chunk->z)
    {
        cz--;
    }
    else if (bz >= chunk->z + CHUNK_WIDTH)
    {
        cz++;
    }
    Chunk* neighbor = chunks[cx][cz];
    SDL_assert(neighbor);
    return GetChunkBlock(neighbor, bx, by, bz);
}

static void UploadVoxels(Chunk* chunk, CPUBuffer voxels[MESH_TYPE_COUNT])
{
    SDL_assert(SDL_GetAtomicInt(&chunk->block_state) == JOB_STATE_COMPLETED);
    SDL_assert(SDL_GetAtomicInt(&chunk->voxel_state) == JOB_STATE_RUNNING);
    bool has_voxels = false;
    for (int i = 0; i < MESH_TYPE_COUNT; i++)
    {
        GPUBuffer_Clear(&chunk->gpu_voxels[i]);
        has_voxels |= voxels[i].size > 0;
    }
    if (!has_voxels)
    {
        return;
    }
    if (!GPUBuffer_BeginUpload(&chunk->gpu_voxels[0]))
    {
        return;
    }
    for (int i = 0; i < MESH_TYPE_COUNT; i++)
    {
        GPUBuffer_Upload(&chunk->gpu_voxels[i], &voxels[i]);
    }
    GPUBuffer_EndUpload();
}

static void UploadLights(Chunk* chunk, CPUBuffer* lights)
{
    SDL_assert(SDL_GetAtomicInt(&chunk->block_state) == JOB_STATE_COMPLETED);
    SDL_assert(SDL_GetAtomicInt(&chunk->light_state) == JOB_STATE_RUNNING);
    GPUBuffer_Clear(&chunk->gpu_lights);
    if (!lights->size)
    {
        return;
    }
    if (!GPUBuffer_BeginUpload(&chunk->gpu_lights))
    {
        return;
    }
    GPUBuffer_Upload(&chunk->gpu_lights, lights);
    GPUBuffer_EndUpload();
}

static bool IsVisible(Block block, Block neighbor)
{
    if (neighbor == BLOCK_EMPTY)
    {
        return true;
    }
    if (Block_IsSprite(neighbor))
    {
        return true;
    }
    if (Block_IsOpaque(block) && !Block_IsOpaque(neighbor))
    {
        return true;
    }
    return false;
}

static int GetAO(Chunk* chunks[3][3], int bx, int by, int bz, Direction direction, int vertex)
{
    int position[3];
    Voxel_GetCubePosition(direction, vertex, position);
    int side1[3] = {DIRECTIONS[direction][0], DIRECTIONS[direction][1], DIRECTIONS[direction][2]};
    int side2[3] = {DIRECTIONS[direction][0], DIRECTIONS[direction][1], DIRECTIONS[direction][2]};
    int corner[3] = {DIRECTIONS[direction][0], DIRECTIONS[direction][1], DIRECTIONS[direction][2]};
    int side = 0;
    for (int i = 0; i < 3; i++)
    {
        if (DIRECTIONS[direction][i])
        {
            continue;
        }
        int offset = position[i] ? 1 : -1;
        if (side++ == 0)
        {
            side1[i] = offset;
        }
        else
        {
            side2[i] = offset;
        }
        corner[i] = offset;
    }
    SDL_assert(side == 2);
    bool has_side1 = Block_HasOcclusion(GetNeighborhoodBlock(chunks, bx, by, bz, side1[0], side1[1], side1[2]));
    bool has_side2 = Block_HasOcclusion(GetNeighborhoodBlock(chunks, bx, by, bz, side2[0], side2[1], side2[2]));
    bool has_corner = Block_HasOcclusion(GetNeighborhoodBlock(chunks, bx, by, bz, corner[0], corner[1], corner[2]));
    if (has_side1 && has_side2)
    {
        return 0;
    }
    return AO_LEVEL_CLEAR - has_side1 - has_side2 - has_corner;
}

static void GenerateChunkBlocks(Chunk* chunk)
{
    SDL_assert(SDL_GetAtomicInt(&chunk->block_state) == JOB_STATE_RUNNING);
    SDL_assert(SDL_GetAtomicInt(&chunk->voxel_state) == JOB_STATE_REQUESTED);
    SDL_assert(SDL_GetAtomicInt(&chunk->light_state) == JOB_STATE_REQUESTED);
    SDL_memset(chunk->blocks, 0, sizeof(chunk->blocks));
    chunk->light_size = 0;
    Rand_GetBlocks(chunk, chunk->x, chunk->z, SetChunkBlockFunction);
    Save_GetBlocks(chunk, chunk->x, chunk->z, SetChunkBlockFunction);
    SDL_assert(SDL_GetAtomicInt(&chunk->block_state) == JOB_STATE_RUNNING);
    SDL_SetAtomicInt(&chunk->block_state, JOB_STATE_COMPLETED);
    if (SDL_GetAtomicInt(&chunk->voxel_state) == JOB_STATE_REQUESTED)
    {
        SDL_SetAtomicInt(&chunk->light_state, JOB_STATE_REQUESTED);
    }
}

static void GenerateChunkVoxels(Chunk* chunks[3][3], CPUBuffer voxels[MESH_TYPE_COUNT])
{
    Chunk* chunk = chunks[1][1];
    SDL_assert(SDL_GetAtomicInt(&chunk->block_state) == JOB_STATE_COMPLETED);
    SDL_assert(SDL_GetAtomicInt(&chunk->voxel_state) == JOB_STATE_RUNNING);
    for (int bx = 0; bx < CHUNK_WIDTH; bx++)
    {
        for (int by = 0; by < CHUNK_HEIGHT; by++)
        {
            for (int bz = 0; bz < CHUNK_WIDTH; bz++)
            {
                Block block = chunk->blocks[bx][by][bz];
                if (block == BLOCK_EMPTY)
                {
                    continue;
                }
                if (Block_IsSprite(block))
                {
                    for (int j = 0; j < 4; j++)
                    {
                        for (int k = 0; k < 4; k++)
                        {
                            Voxel voxel = Voxel_PackSprite(block, bx, by, bz, j, k);
                            CPUBuffer_Append(&voxels[MESH_TYPE_OPAQUE], &voxel);
                        }
                    }
                    continue;
                }
                MeshType mesh = Block_IsOpaque(block) ? MESH_TYPE_OPAQUE : MESH_TYPE_TRANSPARENT;
                for (int j = 0; j < 6; j++)
                {
                    int dx = DIRECTIONS[j][0];
                    int dy = DIRECTIONS[j][1];
                    int dz = DIRECTIONS[j][2];
                    Block neighbor = GetNeighborhoodBlock(chunks, bx, by, bz, dx, dy, dz);
                    if (!IsVisible(block, neighbor))
                    {
                        continue;
                    }
                    int ao[4];
                    int order[] = {0, 1, 2, 3};
                    for (int k = 0; k < 4; k++)
                    {
                        ao[k] = GetAO(chunks, bx, by, bz, j, k);
                    }
                    if (ao[0] + ao[3] > ao[1] + ao[2])
                    {
                        order[0] = 1;
                        order[1] = 3;
                        order[2] = 0;
                        order[3] = 2;
                    }
                    for (int k = 0; k < 4; k++)
                    {
                        int vertex = order[k];
                        Voxel voxel = Voxel_PackCube(block, bx, by, bz, j, vertex, ao[vertex]);
                        CPUBuffer_Append(&voxels[mesh], &voxel);
                    }
                }
            }
        }
    }
    UploadVoxels(chunk, voxels);
    SDL_SetAtomicInt(&chunk->voxel_state, JOB_STATE_COMPLETED);
}

static void RegenerateChunkVoxels(int x, int z)
{
    SDL_assert(!IsChunkOnBorder(x, z));
    Chunk* chunks[3][3] = {0};
    GetNeighborhood(x, z, chunks);
    SDL_SetAtomicInt(&chunks[1][1]->voxel_state, JOB_STATE_RUNNING);
    GenerateChunkVoxels(chunks, cpu_voxels);
}

static void GenerateChunkLights(Chunk* chunks[3][3], CPUBuffer* lights)
{
    Chunk* chunk = chunks[1][1];
    SDL_assert(SDL_GetAtomicInt(&chunk->block_state) == JOB_STATE_COMPLETED);
    SDL_assert(SDL_GetAtomicInt(&chunk->light_state) == JOB_STATE_RUNNING);
    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            const Chunk* neighbor = chunks[i][j];
            SDL_assert(neighbor);
            for (int k = 0; k < neighbor->light_size; k++)
            {
                CPUBuffer_Append(lights, &neighbor->lights[k]);
            }
        }
    }
    UploadLights(chunk, lights);
    SDL_SetAtomicInt(&chunk->light_state, JOB_STATE_COMPLETED);
}

static void GenerateLights()
{
    CPUBuffer lights;
    CPUBuffer_Init(&lights, device, sizeof(Light));
    if (!GPUBuffer_BeginUpload(&gpu_empty_lights))
    {
        CPUBuffer_Free(&lights);
        return;
    }
    Light light = {0};
    CPUBuffer_Append(&lights, &light);
    GPUBuffer_Upload(&gpu_empty_lights, &lights);
    GPUBuffer_EndUpload();
    CPUBuffer_Free(&lights);
}

static void GenerateIndices()
{
    CPUBuffer indices;
    CPUBuffer_Init(&indices, device, sizeof(Uint32));
    if (!GPUBuffer_BeginUpload(&gpu_indices))
    {
        CPUBuffer_Free(&indices);
        return;
    }
    static const int INDICES[] = {0, 1, 2, 3, 2, 1};
    for (Uint32 i = 0; i < INDEX_COUNT / 6; i++)
    {
        for (Uint32 j = 0; j < 6; j++)
        {
            Uint32 index = i * 4 + INDICES[j];
            CPUBuffer_Append(&indices, &index);
        }
    }
    GPUBuffer_Upload(&gpu_indices, &indices);
    GPUBuffer_EndUpload();
    CPUBuffer_Free(&indices);
}

static int WorkerFunction(void* args)
{
    Worker* worker = args;
    while (true)
    {
        SDL_LockMutex(worker->mutex);
        while (worker->job.type == JOB_TYPE_NONE)
        {
            SDL_WaitCondition(worker->condition, worker->mutex);
        }
        Job job = worker->job;
        SDL_UnlockMutex(worker->mutex);
        if (job.type != JOB_TYPE_QUIT)
        {
            Chunk* chunk = GetChunk(job.x, job.z);
            SDL_assert(chunk);
            if (job.type == JOB_TYPE_BLOCKS)
            {
                GenerateChunkBlocks(chunk);
            }
            else
            {
                Chunk* chunks[3][3];
                GetNeighborhood(job.x, job.z, chunks);
                if (job.type == JOB_TYPE_VOXELS)
                {
                    GenerateChunkVoxels(chunks, worker->voxels);
                }
                else if (job.type == JOB_TYPE_LIGHTS)
                {
                    GenerateChunkLights(chunks, &worker->lights);
                }
                else
                {
                    SDL_assert(false);
                }
            }
        }
        SDL_LockMutex(worker->mutex);
        SDL_assert(worker->job.type != JOB_TYPE_NONE);
        worker->job.type = JOB_TYPE_NONE;
        SDL_SignalCondition(worker->condition);
        SDL_UnlockMutex(worker->mutex);
        if (job.type == JOB_TYPE_QUIT)
        {
            return 0;
        }
    }
    return 0;
}

static bool IsJobRunning(const Worker* worker)
{
    SDL_LockMutex(worker->mutex);
    bool running = worker->job.type != JOB_TYPE_NONE;
    SDL_UnlockMutex(worker->mutex);
    return running;
}

static void DispatchJob(Worker* worker, const Job* job)
{
    SDL_LockMutex(worker->mutex);
    SDL_assert(worker->job.type == JOB_TYPE_NONE);
    worker->job = *job;
    SDL_SignalCondition(worker->condition);
    SDL_UnlockMutex(worker->mutex);
}

static void StartWorker(Worker* worker)
{
    for (int i = 0; i < MESH_TYPE_COUNT; i++)
    {
        CPUBuffer_Init(&worker->voxels[i], device, sizeof(Voxel));
    }
    CPUBuffer_Init(&worker->lights, device, sizeof(Light));
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
    worker->thread = SDL_CreateThread(WorkerFunction, "worker", worker);
    if (!worker->thread)
    {
        SDL_Log("Failed to create thread: %s", SDL_GetError());
    }
}

static void StopWorker(Worker* worker)
{
    SDL_LockMutex(worker->mutex);
    while (worker->job.type != JOB_TYPE_NONE)
    {
        SDL_WaitCondition(worker->condition, worker->mutex);
    }
    SDL_UnlockMutex(worker->mutex);
    Job job = {0};
    job.type = JOB_TYPE_QUIT;
    DispatchJob(worker, &job);
    SDL_WaitThread(worker->thread, NULL);
    SDL_DestroyMutex(worker->mutex);
    SDL_DestroyCondition(worker->condition);
    worker->thread = NULL;
    worker->mutex = NULL;
    worker->condition = NULL;
    for (int i = 0; i < MESH_TYPE_COUNT; i++)
    {
        CPUBuffer_Free(&worker->voxels[i]);
    }
    CPUBuffer_Free(&worker->lights);
}

static int SortFunction(void* userdata, const void* lhs, const void* rhs)
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

void World_Init(SDL_GPUDevice* handle)
{
    device = handle;
    world_x = SDL_MAX_SINT32;
    world_z = SDL_MAX_SINT32;
    GPUBuffer_Init(&gpu_indices, device, SDL_GPU_BUFFERUSAGE_INDEX);
    GPUBuffer_Init(&gpu_empty_lights, device, SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ);
    for (int i = 0; i < MESH_TYPE_COUNT; i++)
    {
        CPUBuffer_Init(&cpu_voxels[i], device, sizeof(Voxel));
    }
    for (int i = 0; i < WORKERS; i++)
    {
        StartWorker(&workers[i]);
    }
    for (int x = 0; x < WORLD_WIDTH; x++)
    {
        for (int z = 0; z < WORLD_WIDTH; z++)
        {
            chunks[x][z] = CreateChunk();
            sorted_chunks[x][z][0] = x;
            sorted_chunks[x][z][1] = z;
        }
    }
    SDL_qsort_r(sorted_chunks, WORLD_WIDTH * WORLD_WIDTH, sizeof(int) * 2, SortFunction, NULL);
    GenerateLights();
    GenerateIndices();
}

void World_Free()
{
    for (int i = 0; i < WORKERS; i++)
    {
        StopWorker(&workers[i]);
    }
    for (int x = 0; x < WORLD_WIDTH; x++)
    {
        for (int z = 0; z < WORLD_WIDTH; z++)
        {
            FreeChunk(chunks[x][z]);
        }
    }
    GPUBuffer_Free(&gpu_indices);
    GPUBuffer_Free(&gpu_empty_lights);
    for (int i = 0; i < MESH_TYPE_COUNT; i++)
    {
        CPUBuffer_Free(&cpu_voxels[i]);
    }
}

static int GetWorkers(Worker* local_workers[WORKERS])
{
    int num_workers = 0;
    for (int i = 0; i < WORKERS; i++)
    {
        if (!IsJobRunning(&workers[i]))
        {
            local_workers[num_workers++] = &workers[i];
        }
    }
    return num_workers;
}

static void Shuffle(int offset_x, int offset_z)
{
    world_x += offset_x;
    world_z += offset_z;
    Chunk* in[WORLD_WIDTH][WORLD_WIDTH] = {0};
    Chunk* out[WORLD_WIDTH * WORLD_WIDTH] = {0};
    int size = 0;
    for (int x = 0; x < WORLD_WIDTH; x++)
    {
        for (int z = 0; z < WORLD_WIDTH; z++)
        {
            SDL_assert(chunks[x][z]);
            const int a = x - offset_x;
            const int b = z - offset_z;
            if (IsChunkLocal(a, b))
            {
                in[a][b] = chunks[x][z];
            }
            else
            {
                out[size++] = chunks[x][z];
            }
            chunks[x][z] = NULL;
        }
    }
    SDL_memcpy(chunks, in, sizeof(in));
    for (int x = 0; x < WORLD_WIDTH; x++)
    {
        for (int z = 0; z < WORLD_WIDTH; z++)
        {
            if (!chunks[x][z])
            {
                SDL_assert(size > 0);
                Chunk* chunk = out[--size];
                SDL_SetAtomicInt(&chunk->block_state, JOB_STATE_REQUESTED);
                SDL_SetAtomicInt(&chunk->voxel_state, JOB_STATE_REQUESTED);
                SDL_SetAtomicInt(&chunk->light_state, JOB_STATE_REQUESTED);
                chunks[x][z] = chunk;
            }
            Chunk* chunk = chunks[x][z];
            chunk->x = (world_x + x) * CHUNK_WIDTH;
            chunk->z = (world_z + z) * CHUNK_WIDTH;
        }
    }
    SDL_assert(!size);
}

static bool MoveChunks(const Camera* camera)
{
    const int offset_x = camera->x / CHUNK_WIDTH - WORLD_WIDTH / 2 - world_x;
    const int offset_z = camera->z / CHUNK_WIDTH - WORLD_WIDTH / 2 - world_z;
    if (!offset_x && !offset_z)
    {
        return false;
    }
    Worker* local_workers[WORKERS];
    if (GetWorkers(local_workers) != WORKERS)
    {
        return true;
    }
    Shuffle(offset_x, offset_z);
    return false;
}

static bool TryUpdateBlocks(int x, int z, Worker* worker)
{
    SDL_assert(IsChunkLocal(x, z));
    Chunk* chunk = chunks[x][z];
    if (SDL_GetAtomicInt(&chunk->block_state) != JOB_STATE_REQUESTED)
    {
        return false;
    }
    Job job = {JOB_TYPE_BLOCKS, x, z};
    SDL_SetAtomicInt(&chunk->block_state, JOB_STATE_RUNNING);
    DispatchJob(worker, &job);
    return true;
}

static bool TryUpdateVoxelsOrLights(int x, int z, Worker* worker)
{
    SDL_assert(IsChunkLocal(x, z));
    Chunk* chunk = chunks[x][z];
    if (IsChunkOnBorder(x, z))
    {
        return false;
    }
    bool do_voxel = SDL_GetAtomicInt(&chunk->voxel_state) == JOB_STATE_REQUESTED;
    bool do_light = SDL_GetAtomicInt(&chunk->light_state) == JOB_STATE_REQUESTED;
    if (!do_voxel && !do_light)
    {
        return false;
    }
    Chunk* neighborhood[3][3];
    GetNeighborhood(x, z, neighborhood);
    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            if (SDL_GetAtomicInt(&neighborhood[i][j]->block_state) != JOB_STATE_COMPLETED)
            {
                return false;
            }
        }
    }
    Job job;
    if (do_voxel)
    {
        job = (Job){JOB_TYPE_VOXELS, x, z};
        SDL_SetAtomicInt(&chunk->voxel_state, JOB_STATE_RUNNING);
    }
    else if (do_light)
    {
        job = (Job){JOB_TYPE_LIGHTS, x, z};
        SDL_SetAtomicInt(&chunk->light_state, JOB_STATE_RUNNING);
    }
    else
    {
        SDL_assert(false);
    }
    DispatchJob(worker, &job);
    return true;
}

void World_Update(const Camera* camera)
{
    if (MoveChunks(camera))
    {
        return;
    }
    Worker* local_workers[WORKERS] = {0};
    int num_workers = GetWorkers(local_workers);
    for (int x = 0; x < WORLD_WIDTH; x++)
    {
        for (int z = 0; z < WORLD_WIDTH; z++)
        {
            if (num_workers == 0)
            {
                return;
            }
            int a = sorted_chunks[x][z][0];
            int b = sorted_chunks[x][z][1];
            Worker* worker = local_workers[num_workers - 1];
            if (TryUpdateBlocks(a, b, worker))
            {
                num_workers--;
            }
            else if (TryUpdateVoxelsOrLights(a, b, worker))
            {
                num_workers--;
            }
        }
    }
}

static void Render(Chunk* chunk, SDL_GPUCommandBuffer* cbuf, SDL_GPURenderPass* pass, WorldFlags flags)
{
    MeshType mesh = flags & WORLD_FLAGS_OPAQUE ? MESH_TYPE_OPAQUE : MESH_TYPE_TRANSPARENT;
    GPUBuffer* gpu_voxels = &chunk->gpu_voxels[mesh];
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
        SDL_BindGPUFragmentStorageBuffers(pass, 1, &light_binding, 1);
    }
    SDL_PushGPUVertexUniformData(cbuf, 2, chunk->position, 8);
    SDL_BindGPUVertexBuffers(pass, 0, &voxel_binding, 1);
    SDL_BindGPUIndexBuffer(pass, &index_binding, SDL_GPU_INDEXELEMENTSIZE_32BIT);
    SDL_DrawGPUIndexedPrimitives(pass, gpu_voxels->size * 1.5, 1, 0, 0, 0);
}

void World_Render(const Camera* camera, SDL_GPUCommandBuffer* cbuf, SDL_GPURenderPass* pass, WorldFlags flags)
{
    SDL_PushGPUVertexUniformData(cbuf, 0, camera->proj, 64);
    SDL_PushGPUVertexUniformData(cbuf, 1, camera->view, 64);
    for (int x = 0; x < WORLD_WIDTH; x++)
    {
        for (int y = 0; y < WORLD_WIDTH; y++)
        {
            int a = sorted_chunks[x][y][0];
            int b = sorted_chunks[x][y][1];
            if (IsChunkOnBorder(a, b))
            {
                continue;
            }
            Chunk* chunk = chunks[a][b];
            if (SDL_GetAtomicInt(&chunk->voxel_state) != JOB_STATE_COMPLETED)
            {
                continue;
            }
            float sx = CHUNK_WIDTH;
            float sy = CHUNK_HEIGHT;
            float sz = CHUNK_WIDTH;
            if (!Camera_GetVisibility(camera, chunk->x, 0.0f, chunk->z, sx, sy, sz))
            {
                continue;
            }
            Render(chunk, cbuf, pass, flags);
        }
    }
}

static Chunk* GetWorldChunk(const int position[3], int* chunk_x, int* chunk_z)
{
    if (position[1] < 0 || position[1] >= CHUNK_HEIGHT)
    {
        return NULL;
    }
    *chunk_x = FloorChunkIndex(position[0] - world_x * CHUNK_WIDTH);
    *chunk_z = FloorChunkIndex(position[2] - world_z * CHUNK_WIDTH);
    Chunk* chunk = GetChunk(*chunk_x, *chunk_z);
    if (chunk)
    {
        SDL_assert(chunk->x == (world_x + *chunk_x) * CHUNK_WIDTH);
        SDL_assert(chunk->z == (world_z + *chunk_z) * CHUNK_WIDTH);
    }
    else
    {
        SDL_Log("Bad chunk position: %d, %d", *chunk_x, *chunk_z);
        return NULL;
    }
    bool has_blocks = SDL_GetAtomicInt(&chunk->block_state) == JOB_STATE_COMPLETED;
    bool has_voxels = SDL_GetAtomicInt(&chunk->voxel_state) == JOB_STATE_COMPLETED;
    if (!has_blocks || !has_voxels)
    {
        return NULL;
    }
    return chunk;
}

Block World_GetBlock(const int position[3])
{
    int chunk_x;
    int chunk_z;
    Chunk* chunk = GetWorldChunk(position, &chunk_x, &chunk_z);
    return chunk ? GetChunkBlock(chunk, position[0], position[1], position[2]) : BLOCK_EMPTY;
}

void World_SetBlock(const int position[3], Block block)
{
    int chunk_x;
    int chunk_z;
    Chunk* chunk = GetWorldChunk(position, &chunk_x, &chunk_z);
    if (!chunk)
    {
        return;
    }
    Save_SetBlock(chunk->x, chunk->z, position[0], position[1], position[2], block);
    int local_x = position[0];
    int local_y = position[1];
    int local_z = position[2];
    WorldToChunk(chunk, &local_x, &local_y, &local_z);
    Block old_block = SetChunkBlock(chunk, position[0], position[1], position[2], block);
    int min_x = local_x == 0 ? -1 : 0;
    int max_x = local_x == CHUNK_WIDTH - 1 ? 1 : 0;
    int min_z = local_z == 0 ? -1 : 0;
    int max_z = local_z == CHUNK_WIDTH - 1 ? 1 : 0;
    for (int x = min_x; x <= max_x; x++)
    {
        for (int z = min_z; z <= max_z; z++)
        {
            int cx = chunk_x + x;
            int cz = chunk_z + z;
            if (IsChunkLocal(cx, cz) && !IsChunkOnBorder(cx, cz))
            {
                RegenerateChunkVoxels(cx, cz);
            }
        }
    }
    Chunk* neighborhood[3][3] = {0};
    GetNeighborhood(chunk_x, chunk_z, neighborhood);
    if (Block_IsLight(block) || Block_IsLight(old_block))
    {
        for (int i = 0; i < 3; i++)
        {
            for (int j = 0; j < 3; j++)
            {
                SDL_SetAtomicInt(&neighborhood[i][j]->light_state, JOB_STATE_REQUESTED);
            }
        }
    }
}

WorldQuery World_Raycast(const Camera* camera, float length)
{
    WorldQuery query = {0};
    float direction[3] = {0};
    float distances[3] = {0};
    int steps[3] = {0};
    float deltas[3] = {0};
    Camera_GetVector(camera, &direction[0], &direction[1], &direction[2]);
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
        query.block = World_GetBlock(query.current);
        if (Block_IsSolid(query.block))
        {
            return query;
        }
        SDL_memcpy(query.previous, query.current, sizeof(query.current));
        int axis;
        if (distances[0] < distances[1] && distances[0] < distances[2])
        {
            axis = 0;
        }
        else if (distances[1] < distances[2])
        {
            axis = 1;
        }
        else
        {
            axis = 2;
        }
        traveled = distances[axis];
        distances[axis] += deltas[axis];
        query.current[axis] += steps[axis];
    }
    query.block = BLOCK_EMPTY;
    return query;
}
