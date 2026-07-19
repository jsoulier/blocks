#include <SDL3/SDL.h>

#include "block.h"
#include "buffer.h"
#include "camera.h"
#include "rand.h"
#include "save.h"
#include "voxel.h"
#include "voxel.inc"
#include "worker.h"
#include "world.h"

#define WORKERS 4

typedef enum JobType
{
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

typedef struct WorldWorker
{
    Worker worker;
    Job job;
    CPUBuffer voxels[MESH_TYPE_COUNT];
    CPUBuffer lights;
} WorldWorker;

typedef struct Chunk
{
    SDL_AtomicInt block_state;
    SDL_AtomicInt state;
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
static WorldWorker workers[WORKERS];
static GPUBuffer gpu_indices;
static GPUBuffer gpu_empty_lights;
static CPUBuffer cpu_voxels[MESH_TYPE_COUNT];
static Chunk* chunks[WORLD_WIDTH][WORLD_WIDTH];
static int sorted_chunks[WORLD_WIDTH * WORLD_WIDTH][2];

static bool IsBlockLocal(int bx, int by, int bz)
{
    return bx >= 0 && bz >= 0 && bx < CHUNK_WIDTH && bz < CHUNK_WIDTH;
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

static void WorldToChunk(const Chunk* chunk, int* bx, int* by, int* bz)
{
    *bx -= chunk->x;
    *bz -= chunk->z;
    SDL_assert(IsBlockLocal(*bx, *by, *bz));
}

static Chunk* GetChunk(int cx, int cz)
{
    if (IsChunkLocal(cx, cz))
    {
        return chunks[cx][cz];
    }
    else
    {
        return NULL;
    }
}

static void GetNeighborhood(int cx, int cz, Chunk* neighborhood[3][3])
{
    for (int i = -1; i <= 1; i++)
    for (int j = -1; j <= 1; j++)
    {
        int x = cx + i;
        int z = cz + j;
        neighborhood[i + 1][j + 1] = GetChunk(x, z);
        SDL_assert(neighborhood[i + 1][j + 1]);
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
    SDL_SetAtomicInt(&chunk->state, JOB_STATE_COMPLETED);
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
    SDL_SetAtomicInt(&chunk->state, JOB_STATE_REQUESTED);
    WorldToChunk(chunk, &bx, &by, &bz);
    Block old_block = chunk->blocks[bx][by][bz];
    chunk->blocks[bx][by][bz] = block;
    if (!Block_IsLight(block) && !Block_IsLight(old_block))
    {
        return old_block;
    }
    SDL_SetAtomicInt(&chunk->light_state, JOB_STATE_REQUESTED);
    int light_index = -1;
    for (int index = 0; index < chunk->light_size; index++)
    {
        Light* light = &chunk->lights[index];
        if (light->x == chunk->x + bx && light->y == by && light->z == chunk->z + bz)
        {
            light_index = index;
            break;
        }
    }
    if (BLock_IsLight(block))
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
    int cx = 1;
    int cz = 1;
    if (bx < 0)
    {
        cx = 0;
        bx += CHUNK_WIDTH;
    }
    else if (bx >= CHUNK_WIDTH)
    {
        cx = 2;
        bx -= CHUNK_WIDTH;
    }
    if (bz < 0)
    {
        cz = 0;
        bz += CHUNK_WIDTH;
    }
    else if (bz >= CHUNK_WIDTH)
    {
        cz = 2;
        bz -= CHUNK_WIDTH;
    }
    Chunk* neighbor = chunks[cx][cz];
    SDL_assert(neighbor);
    SDL_assert(SDL_GetAtomicInt(&neighbor->block_state) == JOB_STATE_COMPLETED);
    return neighbor->blocks[bx][by][bz];
}

static void UploadVoxels(Chunk* chunk, CPUBuffer voxels[MESH_TYPE_COUNT])
{
    SDL_assert(SDL_GetAtomicInt(&chunk->block_state) == JOB_STATE_COMPLETED);
    SDL_assert(SDL_GetAtomicInt(&chunk->state) == JOB_STATE_RUNNING);
    bool has_voxels = false;
    for (int mesh_index = 0; mesh_index < MESH_TYPE_COUNT; mesh_index++)
    {
        GPUBuffer_Clear(&chunk->gpu_voxels[mesh_index]);
        has_voxels |= voxels[mesh_index].size > 0;
    }
    if (!has_voxels)
    {
        return;
    }
    if (!GPUBuffer_BeginUpload(&chunk->gpu_voxels[0]))
    {
        return;
    }
    for (int mesh_index = 0; mesh_index < MESH_TYPE_COUNT; mesh_index++)
    {
        GPUBuffer_Upload(&chunk->gpu_voxels[mesh_index], &voxels[mesh_index]);
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
    Voxel_GetPosition(direction, vertex, position);
    int side1[3] = {DIRECTIONS[direction][0], DIRECTIONS[direction][1], DIRECTIONS[direction][2]};
    int side2[3] = {DIRECTIONS[direction][0], DIRECTIONS[direction][1], DIRECTIONS[direction][2]};
    int corner[3] = {DIRECTIONS[direction][0], DIRECTIONS[direction][1], DIRECTIONS[direction][2]};
    int sides = 0;
    for (int i = 0; i < 3; i++)
    {
        if (DIRECTIONS[direction][i])
        {
            continue;
        }
        int offset = position[i] ? 1 : -1;
        if (sides++ == 0)
        {
            side1[i] = offset;
        }
        else
        {
            side2[i] = offset;
        }
        corner[i] = offset;
    }
    SDL_assert(sides == 2);
    bool has_side1 = Block_IsOccluded(GetNeighborhoodBlock(chunks, bx, by, bz, side1[0], side1[1], side1[2]));
    bool has_side2 = Block_IsOccluded(GetNeighborhoodBlock(chunks, bx, by, bz, side2[0], side2[1], side2[2]));
    bool has_corner = Block_IsOccluded(GetNeighborhoodBlock(chunks, bx, by, bz, corner[0], corner[1], corner[2]));
    if (!has_side1 || !has_side2)
    {
        return AO_MASK - has_side1 - has_side2 - has_corner;
    }
    else
    {
        return 0;
    }
}

static void GenerateChunkBlocks(Chunk* chunk)
{
    SDL_assert(SDL_GetAtomicInt(&chunk->block_state) == JOB_STATE_RUNNING);
    SDL_assert(SDL_GetAtomicInt(&chunk->state) == JOB_STATE_REQUESTED);
    SDL_assert(SDL_GetAtomicInt(&chunk->light_state) == JOB_STATE_REQUESTED);
    SDL_memset(chunk->blocks, 0, sizeof(chunk->blocks));
    chunk->light_size = 0;
    Rand_GetBlocks(chunk, chunk->x, chunk->z, SetChunkBlockFunction);
    Save_GetBlocks(chunk, chunk->x, chunk->z, SetChunkBlockFunction);
    SDL_assert(SDL_GetAtomicInt(&chunk->block_state) == JOB_STATE_RUNNING);
    SDL_SetAtomicInt(&chunk->block_state, JOB_STATE_COMPLETED);
}

static void GenerateChunkVoxels(Chunk* chunks[3][3], CPUBuffer voxels[MESH_TYPE_COUNT])
{
    Chunk* chunk = chunks[1][1];
    SDL_assert(SDL_GetAtomicInt(&chunk->block_state) == JOB_STATE_COMPLETED);
    SDL_assert(SDL_GetAtomicInt(&chunk->state) == JOB_STATE_RUNNING);
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
                    for (Direction direction = 0; direction < 4; direction++)
                    {
                        for (int vertex = 0; vertex < 4; vertex++)
                        {
                            Voxel voxel = Voxel_PackSprite(block, bx, by, bz, direction, vertex);
                            CPUBuffer_Append(&voxels[MESH_TYPE_OPAQUE], &voxel);
                        }
                    }
                    continue;
                }
                MeshType mesh_type = Block_IsOpaque(block) ? MESH_TYPE_OPAQUE : MESH_TYPE_TRANSPARENT;
                for (Direction direction = 0; direction < DIRECTION_COUNT; direction++)
                {
                    int dx = DIRECTIONS[direction][0];
                    int dy = DIRECTIONS[direction][1];
                    int dz = DIRECTIONS[direction][2];
                    Block neighbor = GetNeighborhoodBlock(chunks, bx, by, bz, dx, dy, dz);
                    if (!IsVisible(block, neighbor))
                    {
                        continue;
                    }
                    int ao[4];
                    for (int i = 0; i < 4; i++)
                    {
                        ao[i] = GetAO(chunks, bx, by, bz, direction, i);
                    }
                    int order[4];
                    Voxel_GetAO(ao, order);
                    for (int i = 0; i < 4; i++)
                    {
                        int index = order[i];
                        Voxel voxel = Voxel_PackCube(block, bx, by, bz, direction, index, ao[index]);
                        CPUBuffer_Append(&voxels[mesh_type], &voxel);
                    }
                }
            }
        }
    }
    UploadVoxels(chunk, voxels);
    SDL_SetAtomicInt(&chunk->state, JOB_STATE_COMPLETED);
}

static void RegenerateChunkVoxels(int x, int z)
{
    SDL_assert(!IsChunkOnBorder(x, z));
    Chunk* chunks[3][3] = {0};
    GetNeighborhood(x, z, chunks);
    SDL_SetAtomicInt(&chunks[1][1]->state, JOB_STATE_RUNNING);
    GenerateChunkVoxels(chunks, cpu_voxels);
}

static void GenerateChunkLights(Chunk* chunks[3][3], CPUBuffer* lights)
{
    Chunk* chunk = chunks[1][1];
    SDL_assert(SDL_GetAtomicInt(&chunk->block_state) == JOB_STATE_COMPLETED);
    SDL_assert(SDL_GetAtomicInt(&chunk->light_state) == JOB_STATE_RUNNING);
    for (int x = 0; x < 3; x++)
    {
        for (int z = 0; z < 3; z++)
        {
            const Chunk* neighbor = chunks[x][z];
            SDL_assert(neighbor);
            for (int light_index = 0; light_index < neighbor->light_size; light_index++)
            {
                CPUBuffer_Append(lights, &neighbor->lights[light_index]);
            }
        }
    }
    UploadLights(chunk, lights);
    SDL_SetAtomicInt(&chunk->light_state, JOB_STATE_COMPLETED);
}

static void GenerateEmptyLightBuffer()
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

static void GenerateIndexBuffer()
{
    CPUBuffer indices;
    CPUBuffer_Init(&indices, device, sizeof(Uint32));
    if (!GPUBuffer_BeginUpload(&gpu_indices))
    {
        CPUBuffer_Free(&indices);
        return;
    }
    static const int INDICES[] = {0, 1, 2, 3, 2, 1};
    Uint32 max_indices = CHUNK_WIDTH * CHUNK_HEIGHT * CHUNK_WIDTH * DIRECTION_COUNT * 6;
    for (Uint32 quad_index = 0; quad_index < max_indices / 6; quad_index++)
    {
        for (Uint32 index_index = 0; index_index < 6; index_index++)
        {
            Uint32 index = quad_index * 4 + INDICES[index_index];
            CPUBuffer_Append(&indices, &index);
        }
    }
    GPUBuffer_Upload(&gpu_indices, &indices);
    GPUBuffer_EndUpload();
    CPUBuffer_Free(&indices);
}

static void RunJob(void* data)
{
    WorldWorker* worker = data;
    Job job = worker->job;
    Chunk* chunk = GetChunk(job.x, job.z);
    SDL_assert(chunk);
    if (job.type == JOB_TYPE_BLOCKS)
    {
        GenerateChunkBlocks(chunk);
        return;
    }
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

static void DispatchJob(WorldWorker* worker, Job job)
{
    worker->job = job;
    Worker_Dispatch(&worker->worker, RunJob, worker);
}

static int SortFunction(void* userdata, const void* lhs, const void* rhs)
{
    int center = WORLD_WIDTH / 2;
    const int* left = lhs;
    const int* right = rhs;
    int left_distance = (left[0] - center) * (left[0] - center) + (left[1] - center) * (left[1] - center);
    int right_distance = (right[0] - center) * (right[0] - center) + (right[1] - center) * (right[1] - center);
    if (left_distance < right_distance)
    {
        return -1;
    }
    if (left_distance > right_distance)
    {
        return 1;
    }
    return 0;
}

void World_Init(SDL_GPUDevice* gpu_device)
{
    device = gpu_device;
    world_x = SDL_MAX_SINT32;
    world_z = SDL_MAX_SINT32;
    GPUBuffer_Init(&gpu_indices, device, SDL_GPU_BUFFERUSAGE_INDEX);
    GPUBuffer_Init(&gpu_empty_lights, device, SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ);
    for (int mesh_index = 0; mesh_index < MESH_TYPE_COUNT; mesh_index++)
    {
        CPUBuffer_Init(&cpu_voxels[mesh_index], device, sizeof(Voxel));
    }
    for (int worker_index = 0; worker_index < WORKERS; worker_index++)
    {
        WorldWorker* worker = &workers[worker_index];
        for (int mesh_index = 0; mesh_index < MESH_TYPE_COUNT; mesh_index++)
        {
            CPUBuffer_Init(&worker->voxels[mesh_index], device, sizeof(Voxel));
        }
        CPUBuffer_Init(&worker->lights, device, sizeof(Light));
        Worker_Init(&worker->worker);
    }
    for (int x = 0; x < WORLD_WIDTH; x++)
    {
        for (int z = 0; z < WORLD_WIDTH; z++)
        {
            chunks[x][z] = CreateChunk();
            int index = x * WORLD_WIDTH + z;
            sorted_chunks[index][0] = x;
            sorted_chunks[index][1] = z;
        }
    }
    SDL_qsort_r(sorted_chunks, WORLD_WIDTH * WORLD_WIDTH, sizeof(int) * 2, SortFunction, NULL);
    GenerateEmptyLightBuffer();
    GenerateIndexBuffer();
}

void World_Free()
{
    for (int worker_index = 0; worker_index < WORKERS; worker_index++)
    {
        WorldWorker* worker = &workers[worker_index];
        Worker_Free(&worker->worker);
        for (int mesh_index = 0; mesh_index < MESH_TYPE_COUNT; mesh_index++)
        {
            CPUBuffer_Free(&worker->voxels[mesh_index]);
        }
        CPUBuffer_Free(&worker->lights);
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
    for (int mesh_index = 0; mesh_index < MESH_TYPE_COUNT; mesh_index++)
    {
        CPUBuffer_Free(&cpu_voxels[mesh_index]);
    }
}

static int GetIdleWorkers(WorldWorker* idle_workers[WORKERS])
{
    int idle_count = 0;
    for (int worker_index = 0; worker_index < WORKERS; worker_index++)
    {
        if (!Worker_IsBusy(&workers[worker_index].worker))
        {
            idle_workers[idle_count++] = &workers[worker_index];
        }
    }
    return idle_count;
}

static void Shuffle(int offset_x, int offset_z)
{
    world_x += offset_x;
    world_z += offset_z;
    Chunk* retained_chunks[WORLD_WIDTH][WORLD_WIDTH] = {0};
    Chunk* recycled_chunks[WORLD_WIDTH * WORLD_WIDTH] = {0};
    int recycled_count = 0;
    for (int x = 0; x < WORLD_WIDTH; x++)
    {
        for (int z = 0; z < WORLD_WIDTH; z++)
        {
            SDL_assert(chunks[x][z]);
            int new_x = x - offset_x;
            int new_z = z - offset_z;
            if (IsChunkLocal(new_x, new_z))
            {
                retained_chunks[new_x][new_z] = chunks[x][z];
            }
            else
            {
                recycled_chunks[recycled_count++] = chunks[x][z];
            }
            chunks[x][z] = NULL;
        }
    }
    SDL_memcpy(chunks, retained_chunks, sizeof(retained_chunks));
    for (int x = 0; x < WORLD_WIDTH; x++)
    {
        for (int z = 0; z < WORLD_WIDTH; z++)
        {
            if (!chunks[x][z])
            {
                SDL_assert(recycled_count > 0);
                Chunk* chunk = recycled_chunks[--recycled_count];
                SDL_SetAtomicInt(&chunk->block_state, JOB_STATE_REQUESTED);
                SDL_SetAtomicInt(&chunk->state, JOB_STATE_REQUESTED);
                SDL_SetAtomicInt(&chunk->light_state, JOB_STATE_REQUESTED);
                chunks[x][z] = chunk;
            }
            Chunk* chunk = chunks[x][z];
            chunk->x = (world_x + x) * CHUNK_WIDTH;
            chunk->z = (world_z + z) * CHUNK_WIDTH;
        }
    }
    SDL_assert(!recycled_count);
}

static bool MoveChunks(const Camera* camera)
{
    const int offset_x = FloorChunkIndex(camera->x) - WORLD_WIDTH / 2 - world_x;
    const int offset_z = FloorChunkIndex(camera->z) - WORLD_WIDTH / 2 - world_z;
    if (!offset_x && !offset_z)
    {
        return false;
    }
    WorldWorker* idle_workers[WORKERS];
    if (GetIdleWorkers(idle_workers) != WORKERS)
    {
        return true;
    }
    Shuffle(offset_x, offset_z);
    return false;
}

static bool IsNeighborhoodReady(Chunk* neighborhood[3][3])
{
    for (int x = 0; x < 3; x++)
    {
        for (int z = 0; z < 3; z++)
        {
            if (SDL_GetAtomicInt(&neighborhood[x][z]->block_state) != JOB_STATE_COMPLETED)
            {
                return false;
            }
        }
    }
    return true;
}

static bool TryDispatchJob(int x, int z, WorldWorker* worker)
{
    SDL_assert(IsChunkLocal(x, z));
    Chunk* chunk = chunks[x][z];
    Job job = {.x = x, .z = z};
    if (SDL_GetAtomicInt(&chunk->block_state) == JOB_STATE_REQUESTED)
    {
        job.type = JOB_TYPE_BLOCKS;
        SDL_SetAtomicInt(&chunk->block_state, JOB_STATE_RUNNING);
    }
    else
    {
        if (IsChunkOnBorder(x, z))
        {
            return false;
        }
        bool needs_voxels = SDL_GetAtomicInt(&chunk->state) == JOB_STATE_REQUESTED;
        bool needs_lights = SDL_GetAtomicInt(&chunk->light_state) == JOB_STATE_REQUESTED;
        if (!needs_voxels && !needs_lights)
        {
            return false;
        }
        Chunk* neighborhood[3][3];
        GetNeighborhood(x, z, neighborhood);
        if (!IsNeighborhoodReady(neighborhood))
        {
            return false;
        }
        if (needs_voxels)
        {
            job.type = JOB_TYPE_VOXELS;
            SDL_SetAtomicInt(&chunk->state, JOB_STATE_RUNNING);
        }
        else
        {
            job.type = JOB_TYPE_LIGHTS;
            SDL_SetAtomicInt(&chunk->light_state, JOB_STATE_RUNNING);
        }
    }
    DispatchJob(worker, job);
    return true;
}

void World_Update(const Camera* camera)
{
    if (MoveChunks(camera))
    {
        return;
    }
    WorldWorker* idle_workers[WORKERS] = {0};
    int idle_count = GetIdleWorkers(idle_workers);
    for (int index = 0; index < WORLD_WIDTH * WORLD_WIDTH; index++)
    {
        if (idle_count == 0)
        {
            return;
        }
        int chunk_x = sorted_chunks[index][0];
        int chunk_z = sorted_chunks[index][1];
        WorldWorker* worker = idle_workers[idle_count - 1];
        if (TryDispatchJob(chunk_x, chunk_z, worker))
        {
            idle_count--;
        }
    }
}

static void Render(Chunk* chunk, SDL_GPUCommandBuffer* command_buffer, SDL_GPURenderPass* render_pass, WorldFlags flags)
{
    MeshType mesh = flags & WORLD_FLAGS_OPAQUE ? MESH_TYPE_OPAQUE : MESH_TYPE_TRANSPARENT;
    GPUBuffer* gpu_voxels = &chunk->gpu_voxels[mesh];
    if (gpu_voxels->size == 0)
    {
        return;
    }
    SDL_GPUBufferBinding binding = {0};
    SDL_GPUBufferBinding index_binding = {0};
    binding.buffer = gpu_voxels->buffer;
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
        SDL_PushGPUFragmentUniformData(command_buffer, 0, &light_count, sizeof(light_count));
        SDL_BindGPUFragmentStorageBuffers(render_pass, 1, &light_binding, 1);
    }
    SDL_PushGPUVertexUniformData(command_buffer, 2, chunk->position, sizeof(chunk->position));
    SDL_BindGPUVertexBuffers(render_pass, 0, &binding, 1);
    SDL_BindGPUIndexBuffer(render_pass, &index_binding, SDL_GPU_INDEXELEMENTSIZE_32BIT);
    Uint32 index_count = gpu_voxels->size / 4 * 6;
    SDL_DrawGPUIndexedPrimitives(render_pass, index_count, 1, 0, 0, 0);
}

void World_Render(
    const Camera* camera,
    SDL_GPUCommandBuffer* command_buffer,
    SDL_GPURenderPass* render_pass,
    WorldFlags flags)
{
    SDL_PushGPUVertexUniformData(command_buffer, 0, camera->proj, sizeof(camera->proj));
    SDL_PushGPUVertexUniformData(command_buffer, 1, camera->view, sizeof(camera->view));
    for (int index = 0; index < WORLD_WIDTH * WORLD_WIDTH; index++)
    {
        int chunk_x = sorted_chunks[index][0];
        int chunk_z = sorted_chunks[index][1];
        if (IsChunkOnBorder(chunk_x, chunk_z))
        {
            continue;
        }
        Chunk* chunk = chunks[chunk_x][chunk_z];
        if (SDL_GetAtomicInt(&chunk->state) != JOB_STATE_COMPLETED)
        {
            continue;
        }
        if (!Camera_IsVisible(camera, chunk->x, 0.0f, chunk->z, CHUNK_WIDTH, CHUNK_HEIGHT, CHUNK_WIDTH))
        {
            continue;
        }
        Render(chunk, command_buffer, render_pass, flags);
    }
}

static Chunk* GetWorldChunk(const int position[3])
{
    if (position[1] < 0 || position[1] >= CHUNK_HEIGHT)
    {
        return NULL;
    }
    int chunk_x = FloorChunkIndex(position[0] - world_x * CHUNK_WIDTH);
    int chunk_z = FloorChunkIndex(position[2] - world_z * CHUNK_WIDTH);
    Chunk* chunk = GetChunk(chunk_x, chunk_z);
    if (chunk)
    {
        SDL_assert(chunk->x == (world_x + chunk_x) * CHUNK_WIDTH);
        SDL_assert(chunk->z == (world_z + chunk_z) * CHUNK_WIDTH);
    }
    else
    {
        SDL_Log("Bad chunk position: %d, %d", chunk_x, chunk_z);
        return NULL;
    }
    bool has_blocks = SDL_GetAtomicInt(&chunk->block_state) == JOB_STATE_COMPLETED;
    bool has_voxels = SDL_GetAtomicInt(&chunk->state) == JOB_STATE_COMPLETED;
    if (!has_blocks || !has_voxels)
    {
        return NULL;
    }
    return chunk;
}

Block World_GetBlock(const int position[3])
{
    Chunk* chunk = GetWorldChunk(position);
    return chunk ? GetChunkBlock(chunk, position[0], position[1], position[2]) : BLOCK_EMPTY;
}

void World_SetBlock(const int position[3], Block block)
{
    Chunk* chunk = GetWorldChunk(position);
    if (!chunk)
    {
        return;
    }
    int chunk_x = chunk->x / CHUNK_WIDTH - world_x;
    int chunk_z = chunk->z / CHUNK_WIDTH - world_z;
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
        for (int neighbor_x = 0; neighbor_x < 3; neighbor_x++)
        {
            for (int neighbor_z = 0; neighbor_z < 3; neighbor_z++)
            {
                SDL_SetAtomicInt(&neighborhood[neighbor_x][neighbor_z]->light_state, JOB_STATE_REQUESTED);
            }
        }
    }
}

WorldQuery World_Raycast(const Camera* camera, float max_distance)
{
    WorldQuery query = {0};
    float direction[3] = {0};
    float next_distances[3] = {0};
    int steps[3] = {0};
    float step_distances[3] = {0};
    Camera_GetVector(camera, &direction[0], &direction[1], &direction[2]);
    for (int axis = 0; axis < 3; axis++)
    {
        query.current[axis] = SDL_floorf(camera->position[axis]);
        query.previous[axis] = query.current[axis];
        if (SDL_fabsf(direction[axis]) > SDL_FLT_EPSILON)
        {
            step_distances[axis] = SDL_fabsf(1.0f / direction[axis]);
        }
        else
        {
            step_distances[axis] = 1e6f;
        }
        if (direction[axis] < 0.0f)
        {
            steps[axis] = -1;
            next_distances[axis] = (camera->position[axis] - query.current[axis]) * step_distances[axis];
        }
        else
        {
            steps[axis] = 1;
            next_distances[axis] = (query.current[axis] + 1.0f - camera->position[axis]) * step_distances[axis];
        }
    }
    float distance = 0.0f;
    while (distance <= max_distance)
    {
        query.block = World_GetBlock(query.current);
        if (Block_IsSolid(query.block))
        {
            return query;
        }
        SDL_memcpy(query.previous, query.current, sizeof(query.current));
        int axis;
        if (next_distances[0] < next_distances[1] && next_distances[0] < next_distances[2])
        {
            axis = 0;
        }
        else if (next_distances[1] < next_distances[2])
        {
            axis = 1;
        }
        else
        {
            axis = 2;
        }
        distance = next_distances[axis];
        next_distances[axis] += step_distances[axis];
        query.current[axis] += steps[axis];
    }
    query.block = BLOCK_EMPTY;
    return query;
}
