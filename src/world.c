#include <SDL3/SDL.h>

#include "block.h"
#include "buffer.h"
#include "camera.h"
#include "map.h"
#include "rand.h"
#include "save.h"
#include "sort.h"
#include "voxel.h"
#include "voxel.inc"
#include "worker.h"
#include "world.h"

#define WORKERS 4

typedef enum TaskType
{
    TASK_TYPE_BLOCKS,
    TASK_TYPE_VOXELS,
    TASK_TYPE_LIGHTS,
} TaskType;

typedef enum TaskState
{
    TASK_STATE_REQUESTED,
    TASK_STATE_RUNNING,
    TASK_STATE_COMPLETED,
} TaskState;

typedef enum MeshType
{
    MESH_TYPE_OPAQUE,
    MESH_TYPE_TRANSPARENT,
    MESH_TYPE_COUNT,
} MeshType;

typedef struct Task
{
    TaskType type;
    int x;
    int z;
} Task;

typedef struct WorldWorker
{
    Worker worker;
    Task task;
    CPUBuffer voxels[MESH_TYPE_COUNT];
    CPUBuffer lights;
} WorldWorker;

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
    Map lights;
    GPUBuffer gpu_voxels[MESH_TYPE_COUNT];
    GPUBuffer gpu_lights;
} Chunk;

static SDL_GPUDevice* device;
static int world_x;
static int world_z;
static WorldWorker all_workers[WORKERS];
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

static bool IsNeighborhoodGenerated(Chunk* neighborhood[3][3])
{
    for (int x = 0; x < 3; x++)
    for (int z = 0; z < 3; z++)
    {
        if (SDL_GetAtomicInt(&neighborhood[x][z]->block_state) != TASK_STATE_COMPLETED)
        {
            return false;
        }
    }
    return true;
}

static Chunk* CreateChunk()
{
    Chunk* chunk = SDL_calloc(1, sizeof(Chunk));
    if (!chunk)
    {
        SDL_Log("Failed to allocate chunk");
    }
    SDL_SetAtomicInt(&chunk->block_state, TASK_STATE_REQUESTED);
    SDL_SetAtomicInt(&chunk->voxel_state, TASK_STATE_COMPLETED);
    SDL_SetAtomicInt(&chunk->light_state, TASK_STATE_COMPLETED);
    Map_Init(&chunk->lights, 8);
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
    Map_Free(&chunk->lights);
    SDL_free(chunk);
}

static Block SetChunkBlock(Chunk* chunk, int bx, int by, int bz, Block block)
{
    SDL_SetAtomicInt(&chunk->voxel_state, TASK_STATE_REQUESTED);
    WorldToChunk(chunk, &bx, &by, &bz);
    Block old_block = chunk->blocks[bx][by][bz];
    chunk->blocks[bx][by][bz] = block;
    if (!Block_IsLight(block) && !Block_IsLight(old_block))
    {
        return old_block;
    }
    SDL_SetAtomicInt(&chunk->light_state, TASK_STATE_REQUESTED);
    if (Block_IsLight(block))
    {
        Map_Set(&chunk->lights, bx, by, bz, block);
    }
    else
    {
        Map_Remove(&chunk->lights, bx, by, bz);
    }
    return old_block;
}

static void SetChunkBlockFunction(void* userdata, int bx, int by, int bz, Block block)
{
    Chunk* chunk = userdata;
    SDL_assert(SDL_GetAtomicInt(&chunk->block_state) == TASK_STATE_RUNNING);
    SetChunkBlock(userdata, bx, by, bz, block);
}

static Block GetChunkBlock(Chunk* chunk, int bx, int by, int bz)
{
    SDL_assert(SDL_GetAtomicInt(&chunk->block_state) == TASK_STATE_COMPLETED);
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
    SDL_assert(SDL_GetAtomicInt(&neighbor->block_state) == TASK_STATE_COMPLETED);
    return neighbor->blocks[bx][by][bz];
}

static void UploadVoxels(Chunk* chunk, CPUBuffer voxels[MESH_TYPE_COUNT])
{
    SDL_assert(SDL_GetAtomicInt(&chunk->block_state) == TASK_STATE_COMPLETED);
    SDL_assert(SDL_GetAtomicInt(&chunk->voxel_state) == TASK_STATE_RUNNING);
    bool has_voxels = false;
    for (int i = 0; i < MESH_TYPE_COUNT; i++)
    {
        GPUBuffer_Clear(&chunk->gpu_voxels[i]);
        has_voxels |= voxels[i].size > 0;
    }
    if (!has_voxels || !GPUBuffer_BeginUpload(&chunk->gpu_voxels[0]))
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
    SDL_assert(SDL_GetAtomicInt(&chunk->block_state) == TASK_STATE_COMPLETED);
    SDL_assert(SDL_GetAtomicInt(&chunk->light_state) == TASK_STATE_RUNNING);
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
    SDL_assert(SDL_GetAtomicInt(&chunk->block_state) == TASK_STATE_RUNNING);
    SDL_assert(SDL_GetAtomicInt(&chunk->voxel_state) == TASK_STATE_REQUESTED);
    SDL_assert(SDL_GetAtomicInt(&chunk->light_state) == TASK_STATE_REQUESTED);
    SDL_memset(chunk->blocks, 0, sizeof(chunk->blocks));
    Map_Clear(&chunk->lights);
    Rand_GetBlocks(chunk, chunk->x, chunk->z, SetChunkBlockFunction);
    Save_GetBlocks(chunk, chunk->x, chunk->z, SetChunkBlockFunction);
    SDL_assert(SDL_GetAtomicInt(&chunk->block_state) == TASK_STATE_RUNNING);
    SDL_SetAtomicInt(&chunk->block_state, TASK_STATE_COMPLETED);
}

static void GenerateChunkVoxels(Chunk* chunks[3][3], CPUBuffer voxels[MESH_TYPE_COUNT])
{
    Chunk* chunk = chunks[1][1];
    SDL_assert(SDL_GetAtomicInt(&chunk->block_state) == TASK_STATE_COMPLETED);
    SDL_assert(SDL_GetAtomicInt(&chunk->voxel_state) == TASK_STATE_RUNNING);
    for (int bx = 0; bx < CHUNK_WIDTH; bx++)
    for (int by = 0; by < CHUNK_HEIGHT; by++)
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
            for (int vertex = 0; vertex < 4; vertex++)
            {
                Voxel voxel = Voxel_PackSprite(block, bx, by, bz, direction, vertex);
                CPUBuffer_Append(&voxels[MESH_TYPE_OPAQUE], &voxel);
            }
            continue;
        }
        MeshType type = Block_IsOpaque(block) ? MESH_TYPE_OPAQUE : MESH_TYPE_TRANSPARENT;
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
                CPUBuffer_Append(&voxels[type], &voxel);
            }
        }
    }
    UploadVoxels(chunk, voxels);
    SDL_SetAtomicInt(&chunk->voxel_state, TASK_STATE_COMPLETED);
}

static void RegenerateChunkVoxels(int x, int z)
{
    SDL_assert(!IsChunkOnBorder(x, z));
    Chunk* chunks[3][3] = {0};
    GetNeighborhood(x, z, chunks);
    SDL_SetAtomicInt(&chunks[1][1]->voxel_state, TASK_STATE_RUNNING);
    GenerateChunkVoxels(chunks, cpu_voxels);
}

static void GenerateChunkLights(Chunk* chunks[3][3], CPUBuffer* lights)
{
    Chunk* chunk = chunks[1][1];
    SDL_assert(SDL_GetAtomicInt(&chunk->block_state) == TASK_STATE_COMPLETED);
    SDL_assert(SDL_GetAtomicInt(&chunk->light_state) == TASK_STATE_RUNNING);
    for (int x = 0; x < 3; x++)
    for (int z = 0; z < 3; z++)
    {
        const Chunk* neighbor = chunks[x][z];
        SDL_assert(neighbor);
        for (Uint32 i = 0; i < neighbor->lights.capacity; i++)
        {
            if (!Map_IsRowValid(&neighbor->lights, i))
            {
                continue;
            }
            MapRow row = Map_GetRow(&neighbor->lights, i);
            Block block = row.value;
            SDL_assert(Block_IsLight(block));
            Light light = Block_GetLight(block);
            light.x = neighbor->x + row.x;
            light.y = row.y;
            light.z = neighbor->z + row.z;
            CPUBuffer_Append(lights, &light);
        }
    }
    UploadLights(chunk, lights);
    SDL_SetAtomicInt(&chunk->light_state, TASK_STATE_COMPLETED);
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
    for (Uint32 i = 0; i < max_indices / 6; i++)
    for (Uint32 j = 0; j < 6; j++)
    {
        Uint32 index = i * 4 + INDICES[j];
        CPUBuffer_Append(&indices, &index);
    }
    GPUBuffer_Upload(&gpu_indices, &indices);
    GPUBuffer_EndUpload();
    CPUBuffer_Free(&indices);
}

static void RunTask(void* data)
{
    WorldWorker* worker = data;
    Task task = worker->task;
    Chunk* chunk = GetChunk(task.x, task.z);
    SDL_assert(chunk);
    if (task.type == TASK_TYPE_BLOCKS)
    {
        GenerateChunkBlocks(chunk);
        return;
    }
    Chunk* chunks[3][3];
    GetNeighborhood(task.x, task.z, chunks);
    if (task.type == TASK_TYPE_VOXELS)
    {
        GenerateChunkVoxels(chunks, worker->voxels);
    }
    else if (task.type == TASK_TYPE_LIGHTS)
    {
        GenerateChunkLights(chunks, &worker->lights);
    }
    else
    {
        SDL_assert(false);
    }
}

static void DispatchTask(WorldWorker* worker, Task task)
{
    worker->task = task;
    Worker_Dispatch(&worker->worker, RunTask, worker);
}

static bool TryDispatchTask(int x, int z, WorldWorker* worker)
{
    SDL_assert(IsChunkLocal(x, z));
    Chunk* chunk = chunks[x][z];
    Task task = {.x = x, .z = z};
    if (SDL_GetAtomicInt(&chunk->block_state) == TASK_STATE_REQUESTED)
    {
        task.type = TASK_TYPE_BLOCKS;
        SDL_SetAtomicInt(&chunk->block_state, TASK_STATE_RUNNING);
    }
    else
    {
        if (IsChunkOnBorder(x, z))
        {
            return false;
        }
        bool voxels = SDL_GetAtomicInt(&chunk->voxel_state) == TASK_STATE_REQUESTED;
        bool lights = SDL_GetAtomicInt(&chunk->light_state) == TASK_STATE_REQUESTED;
        if (!voxels && !lights)
        {
            return false;
        }
        Chunk* neighborhood[3][3];
        GetNeighborhood(x, z, neighborhood);
        if (!IsNeighborhoodGenerated(neighborhood))
        {
            return false;
        }
        if (voxels)
        {
            task.type = TASK_TYPE_VOXELS;
            SDL_SetAtomicInt(&chunk->voxel_state, TASK_STATE_RUNNING);
        }
        else
        {
            task.type = TASK_TYPE_LIGHTS;
            SDL_SetAtomicInt(&chunk->light_state, TASK_STATE_RUNNING);
        }
    }
    DispatchTask(worker, task);
    return true;
}


void World_Init(SDL_GPUDevice* gpu_device)
{
    device = gpu_device;
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
        WorldWorker* worker = &all_workers[i];
        for (int j = 0; j < MESH_TYPE_COUNT; j++)
        {
            CPUBuffer_Init(&worker->voxels[j], device, sizeof(Voxel));
        }
        CPUBuffer_Init(&worker->lights, device, sizeof(Light));
        Worker_Init(&worker->worker);
    }
    for (int x = 0; x < WORLD_WIDTH; x++)
    for (int z = 0; z < WORLD_WIDTH; z++)
    {
        chunks[x][z] = CreateChunk();
        int index = x * WORLD_WIDTH + z;
        sorted_chunks[index][0] = x;
        sorted_chunks[index][1] = z;
    }
    Sort_Distance2D(sorted_chunks, WORLD_WIDTH * WORLD_WIDTH, WORLD_WIDTH / 2);
    GenerateEmptyLightBuffer();
    GenerateIndexBuffer();
}

void World_Free()
{
    for (int i = 0; i < WORKERS; i++)
    {
        WorldWorker* worker = &all_workers[i];
        Worker_Free(&worker->worker);
        for (int mesh_index = 0; mesh_index < MESH_TYPE_COUNT; mesh_index++)
        {
            CPUBuffer_Free(&worker->voxels[mesh_index]);
        }
        CPUBuffer_Free(&worker->lights);
    }
    for (int x = 0; x < WORLD_WIDTH; x++)
    for (int z = 0; z < WORLD_WIDTH; z++)
    {
        FreeChunk(chunks[x][z]);
    }
    GPUBuffer_Free(&gpu_indices);
    GPUBuffer_Free(&gpu_empty_lights);
    for (int i = 0; i < MESH_TYPE_COUNT; i++)
    {
        CPUBuffer_Free(&cpu_voxels[i]);
    }
}

static int GetIdleWorkers(WorldWorker* workers[WORKERS])
{
    int count = 0;
    for (int i = 0; i < WORKERS; i++)
    {
        if (!Worker_IsBusy(&all_workers[i].worker))
        {
            workers[count++] = &all_workers[i];
        }
    }
    return count;
}

static void Shuffle(int dx, int dz)
{
    world_x += dx;
    world_z += dz;
    Chunk* kept[WORLD_WIDTH][WORLD_WIDTH] = {0};
    Chunk* recycled[WORLD_WIDTH * WORLD_WIDTH] = {0};
    int count = 0;
    for (int x = 0; x < WORLD_WIDTH; x++)
    for (int z = 0; z < WORLD_WIDTH; z++)
    {
        SDL_assert(chunks[x][z]);
        int new_x = x - dx;
        int new_z = z - dz;
        if (IsChunkLocal(new_x, new_z))
        {
            kept[new_x][new_z] = chunks[x][z];
        }
        else
        {
            recycled[count++] = chunks[x][z];
        }
        chunks[x][z] = NULL;
    }
    SDL_memcpy(chunks, kept, sizeof(kept));
    for (int x = 0; x < WORLD_WIDTH; x++)
    for (int z = 0; z < WORLD_WIDTH; z++)
    {
        if (!chunks[x][z])
        {
            SDL_assert(count > 0);
            Chunk* chunk = recycled[--count];
            SDL_SetAtomicInt(&chunk->block_state, TASK_STATE_REQUESTED);
            SDL_SetAtomicInt(&chunk->voxel_state, TASK_STATE_REQUESTED);
            SDL_SetAtomicInt(&chunk->light_state, TASK_STATE_REQUESTED);
            chunks[x][z] = chunk;
        }
        Chunk* chunk = chunks[x][z];
        chunk->x = (world_x + x) * CHUNK_WIDTH;
        chunk->z = (world_z + z) * CHUNK_WIDTH;
    }
    SDL_assert(!count);
}

static bool MoveChunks(const Camera* camera)
{
    const int dx = FloorChunkIndex(camera->x) - WORLD_WIDTH / 2 - world_x;
    const int dz = FloorChunkIndex(camera->z) - WORLD_WIDTH / 2 - world_z;
    if (!dx && !dz)
    {
        return true;
    }
    WorldWorker* workers[WORKERS];
    if (GetIdleWorkers(workers) != WORKERS)
    {
        return false;
    }
    Shuffle(dx, dz);
    return true;
}

void World_Update(const Camera* camera)
{
    if (!MoveChunks(camera))
    {
        return;
    }
    WorldWorker* workers[WORKERS] = {0};
    int count = GetIdleWorkers(workers);
    for (int i = 0; i < WORLD_WIDTH * WORLD_WIDTH; i++)
    {
        if (count == 0)
        {
            return;
        }
        int cx = sorted_chunks[i][0];
        int cz = sorted_chunks[i][1];
        WorldWorker* worker = workers[count - 1];
        if (TryDispatchTask(cx, cz, worker))
        {
            count--;
        }
    }
}

static void Render(Chunk* chunk, SDL_GPUCommandBuffer* command_buffer, SDL_GPURenderPass* render_pass, WorldFlags flags)
{
    SDL_assert(SDL_HasExactlyOneBitSet32(flags & (WORLD_FLAGS_OPAQUE | WORLD_FLAGS_TRANSPARENT)));
    MeshType mesh = flags & WORLD_FLAGS_OPAQUE ? MESH_TYPE_OPAQUE : MESH_TYPE_TRANSPARENT;
    GPUBuffer* voxels = &chunk->gpu_voxels[mesh];
    if (!voxels->size)
    {
        return;
    }
    SDL_GPUBufferBinding voxel_binding = {0};
    SDL_GPUBufferBinding index_binding = {0};
    voxel_binding.buffer = voxels->buffer;
    index_binding.buffer = gpu_indices.buffer;
    if (flags & WORLD_FLAGS_LIGHT)
    {
        Sint32 count;
        SDL_GPUBuffer* light_binding;
        if (SDL_GetAtomicInt(&chunk->light_state) == TASK_STATE_COMPLETED && chunk->gpu_lights.size)
        {
            light_binding = chunk->gpu_lights.buffer;
            count = chunk->gpu_lights.size;
        }
        else
        {
            light_binding = gpu_empty_lights.buffer;
            count = 0;
        }
        SDL_PushGPUFragmentUniformData(command_buffer, 0, &count, sizeof(count));
        SDL_BindGPUFragmentStorageBuffers(render_pass, 1, &light_binding, 1);
    }
    SDL_PushGPUVertexUniformData(command_buffer, 2, chunk->position, sizeof(chunk->position));
    SDL_BindGPUVertexBuffers(render_pass, 0, &voxel_binding, 1);
    SDL_BindGPUIndexBuffer(render_pass, &index_binding, SDL_GPU_INDEXELEMENTSIZE_32BIT);
    SDL_DrawGPUIndexedPrimitives(render_pass, voxels->size / 4 * 6, 1, 0, 0, 0);
}

void World_Render(const Camera* camera, SDL_GPUCommandBuffer* command_buffer, SDL_GPURenderPass* render_pass, WorldFlags flags)
{
    SDL_PushGPUVertexUniformData(command_buffer, 0, camera->proj, sizeof(camera->proj));
    SDL_PushGPUVertexUniformData(command_buffer, 1, camera->view, sizeof(camera->view));
    for (int i = 0; i < WORLD_WIDTH * WORLD_WIDTH; i++)
    {
        int cx = sorted_chunks[i][0];
        int cz = sorted_chunks[i][1];
        if (IsChunkOnBorder(cx, cz))
        {
            continue;
        }
        Chunk* chunk = chunks[cx][cz];
        if (SDL_GetAtomicInt(&chunk->voxel_state) != TASK_STATE_COMPLETED)
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
    int cx = FloorChunkIndex(position[0] - world_x * CHUNK_WIDTH);
    int cz = FloorChunkIndex(position[2] - world_z * CHUNK_WIDTH);
    Chunk* chunk = GetChunk(cx, cz);
    if (chunk)
    {
        SDL_assert(chunk->x == (world_x + cx) * CHUNK_WIDTH);
        SDL_assert(chunk->z == (world_z + cz) * CHUNK_WIDTH);
    }
    else
    {
        SDL_Log("Bad chunk position: %d, %d", cx, cz);
        return NULL;
    }
    bool blocks = SDL_GetAtomicInt(&chunk->block_state) == TASK_STATE_COMPLETED;
    bool voxels = SDL_GetAtomicInt(&chunk->voxel_state) == TASK_STATE_COMPLETED;
    if (blocks && voxels)
    {
        return chunk;
    }
    else
    {
        return NULL;
    }
}

Block World_GetBlock(const int position[3])
{
    Chunk* chunk = GetWorldChunk(position);
    if (chunk)
    {
        return GetChunkBlock(chunk, position[0], position[1], position[2]);
    }
    else
    {
        return BLOCK_EMPTY;
    }
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
    for (int dx = min_x; dx <= max_x; dx++)
    for (int dz = min_z; dz <= max_z; dz++)
    {
        int cx = chunk_x + dx;
        int cz = chunk_z + dz;
        if (IsChunkLocal(cx, cz) && !IsChunkOnBorder(cx, cz))
        {
            RegenerateChunkVoxels(cx, cz);
        }
    }
    Chunk* neighborhood[3][3] = {0};
    GetNeighborhood(chunk_x, chunk_z, neighborhood);
    if (!Block_IsLight(block) && !Block_IsLight(old_block))
    {
        return;
    }
    for (int dx = 0; dx < 3; dx++)
    for (int dz = 0; dz < 3; dz++)
    {
        SDL_SetAtomicInt(&neighborhood[dx][dz]->light_state, TASK_STATE_REQUESTED);
    }
}

WorldQuery World_Raycast(const Camera* camera, float max_distance)
{
    WorldQuery query = {0};
    float vector[3] = {0};
    int step[3] = {0};
    float distance[3] = {0};
    float delta[3] = {0};
    Camera_GetVector(camera, &vector[0], &vector[1], &vector[2]);
    for (int i = 0; i < 3; i++)
    {
        query.current[i] = SDL_floorf(camera->position[i]);
        query.previous[i] = query.current[i];
        if (SDL_fabsf(vector[i]) > SDL_FLT_EPSILON)
        {
            delta[i] = SDL_fabsf(1.0f / vector[i]);
        }
        else
        {
            delta[i] = 1e6f;
        }
        if (vector[i] < 0.0f)
        {
            step[i] = -1;
            distance[i] = (camera->position[i] - query.current[i]) * delta[i];
        }
        else
        {
            step[i] = 1;
            distance[i] = (query.current[i] + 1.0f - camera->position[i]) * delta[i];
        }
    }
    float length = 0.0f;
    while (length <= max_distance)
    {
        query.block = World_GetBlock(query.current);
        if (Block_IsSolid(query.block))
        {
            return query;
        }
        SDL_memcpy(query.previous, query.current, sizeof(query.current));
        int axis;
        if (distance[0] < distance[1] && distance[0] < distance[2])
        {
            axis = 0;
        }
        else if (distance[1] < distance[2])
        {
            axis = 1;
        }
        else
        {
            axis = 2;
        }
        length = distance[axis];
        distance[axis] += delta[axis];
        query.current[axis] += step[axis];
    }
    query.block = BLOCK_EMPTY;
    return query;
}
