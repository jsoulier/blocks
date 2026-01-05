#include <SDL3/SDL.h>

#include "block.h"
#include "buffer.h"
#include "camera.h"
#include "chunk.h"
#include "sort.h"
#include "worker.h"
#include "world.h"

static bool Contains(const World* world, int x, int z)
{
    return x >= 0 && z >= 0 && x < WORLD_WIDTH && z < WORLD_WIDTH;
}

void CreateWorld(World* world, SDL_GPUDevice* device)
{
    world->Device = device;
    world->X = INT32_MAX;
    world->Y = INT32_MAX;
    world->Z = INT32_MAX;
    CreateCpuBuffer(&world->CpuIndexBuffer, device, sizeof(Uint32));
    CreateGpuBuffer(&world->GpuIndexBuffer, device, SDL_GPU_BUFFERUSAGE_INDEX);
    for (int i = 0; i < WORLD_WORKERS; i++)
    {
        CreateWorker(&world->Workers[i], device);
    }
    for (int x = 0; x < WORLD_WIDTH; x++)
    for (int z = 0; z < WORLD_WIDTH; z++)
    {
        world->Chunks[x][z] = SDL_malloc(sizeof(Chunk));
        CreateChunk(world->Chunks[x][z], device);
    }
    for (int x = 0; x < WORLD_WIDTH - 2; x++)
    for (int z = 0; z < WORLD_WIDTH - 2; z++)
    {
        world->SortedChunks[x][z][0] = x + 1;
        world->SortedChunks[x][z][1] = z + 1;
    }
    int w = WORLD_WIDTH - 2;
    SortXY(w / 2 + 1, w / 2 + 1, (int*) world->SortedChunks, w * w);
}

void DestroyWorld(World* world)
{
    for (int i = 0; i < WORLD_WORKERS; i++)
    {
        DestroyWorker(&world->Workers[i]);
    }
    for (int x = 0; x < WORLD_WIDTH; x++)
    for (int z = 0; z < WORLD_WIDTH; z++)
    {
        DestroyChunk(world->Chunks[x][z]);
        SDL_free(world->Chunks[x][z]);
    }
    DestroyCpuBuffer(&world->CpuIndexBuffer);
    DestroyGpuBuffer(&world->GpuIndexBuffer);
}

static void Move(World* world, const Camera* camera)
{
    const int cameraX = camera->X / CHUNK_WIDTH - WORLD_WIDTH / 2;
    const int cameraZ = camera->Z / CHUNK_WIDTH - WORLD_WIDTH / 2;
    const int offsetX = cameraX - world->X;
    const int offsetZ = cameraZ - world->Z;
    if (!offsetX && !offsetZ)
    {
        return;
    }
    world->X = cameraX;
    world->Y = 0;
    world->Z = cameraZ;
    Chunk* in[WORLD_WIDTH][WORLD_WIDTH] = {0};
    Chunk* out[WORLD_WIDTH * WORLD_WIDTH] = {0};
    int size = 0;
    for (int x = 0; x < WORLD_WIDTH; x++)
    for (int z = 0; z < WORLD_WIDTH; z++)
    {
        SDL_assert(world->Chunks[x][z]);
        const int a = x - offsetX;
        const int b = z - offsetZ;
        if (Contains(world, a, b))
        {
            in[a][b] = world->Chunks[x][z];
        }
        else
        {
            out[size++] = world->Chunks[x][z];
        }
        world->Chunks[x][z] = NULL;
    }
    SDL_memcpy(world->Chunks, in, sizeof(in));
    for (int x = 0; x < WORLD_WIDTH; x++)
    for (int z = 0; z < WORLD_WIDTH; z++)
    {
        if (!world->Chunks[x][z])
        {
            SDL_assert(size > 0);
            Chunk* chunk = out[--size];
            chunk->Flags |= ChunkFlagGenerate;
            world->Chunks[x][z] = chunk;
        }
        Chunk* chunk = world->Chunks[x][z];
        chunk->X = (world->X + x) * CHUNK_WIDTH;
        chunk->Y = 0;
        chunk->Z = (world->Z + z) * CHUNK_WIDTH;
    }
    SDL_assert(!size);
}

static void CreateIndexBuffer(World* world, Uint32 size)
{
    if (world->GpuIndexBuffer.Size >= size)
    {
        return;
    }
    static const int kIndices[] = {0, 1, 2, 3, 2, 1};
    for (Uint32 i = 0; i < size; i++)
    for (Uint32 j = 0; j < 6; j++)
    {
        Uint32 index = i * 4 + kIndices[j];
        AppendCpuBuffer(&world->CpuIndexBuffer, &index);
    }
    SDL_GPUCommandBuffer* commandBuffer = SDL_AcquireGPUCommandBuffer(world->Device);
    if (!commandBuffer)
    {
        SDL_Log("Failed to acquire command buffer: %s", SDL_GetError());
        return;
    }
    SDL_GPUCopyPass* pass = SDL_BeginGPUCopyPass(commandBuffer);
    if (!pass)
    {
        SDL_Log("Failed to begin copy pass: %s", SDL_GetError());
        SDL_CancelGPUCommandBuffer(commandBuffer);
        return;
    }
    UpdateGpuBuffer(&world->GpuIndexBuffer, pass, &world->CpuIndexBuffer);
    SDL_EndGPUCopyPass(pass);
    SDL_SubmitGPUCommandBuffer(commandBuffer);
}

void UpdateWorld(World* world, const Camera* camera, Save* save, Noise* noise)
{
    // TODO: generating everything before meshing can cause artifacts to appear until generation finishes
    Move(world, camera);
    WorkerJob jobs[WORLD_WORKERS] = {0};
    int numJobs = 0;
    bool generated = true;
    for (int x = 0; x < WORLD_WIDTH; x++)
    for (int z = 0; z < WORLD_WIDTH; z++)
    {
        Chunk* chunk = world->Chunks[x][z];
        if (!(chunk->Flags & ChunkFlagGenerate))
        {
            continue;
        }
        // TODO: move
        if (numJobs >= WORLD_WORKERS)
        {
            continue;
        }
        Worker* worker = &world->Workers[numJobs];
        WorkerJob* job = &jobs[numJobs];
        numJobs++;
        job->Type = WorkerJobTypeGenerate;
        job->X = x;
        job->Y = 0;
        job->Z = z;
        job->WorldRef = world;
        job->SaveRef = save;
        job->NoiseRef = noise;
        DispatchWorker(worker, job);
        generated = false;
    }
    if (generated)
    {
        for (int x = 0; x < WORLD_WIDTH - 2; x++)
        for (int z = 0; z < WORLD_WIDTH - 2; z++)
        {
            int a = world->SortedChunks[x][z][0];
            int b = world->SortedChunks[x][z][1];
            Chunk* chunk = world->Chunks[a][b];
            if (!(chunk->Flags & ChunkFlagMesh))
            {
                continue;
            }
            // TODO: move
            if (numJobs >= WORLD_WORKERS)
            {
                continue;
            }
            Worker* worker = &world->Workers[numJobs];
            WorkerJob* job = &jobs[numJobs];
            numJobs++;
            job->Type = WorkerJobTypeMesh;
            job->X = a;
            job->Y = 0;
            job->Z = b;
            job->WorldRef = world;
            job->SaveRef = save;
            job->NoiseRef = noise;
            DispatchWorker(worker, job);
        }
    }
    for (int i = 0; i < numJobs; i++)
    {
        Worker* worker = &world->Workers[i];
        WorkerJob* job = &jobs[i];
        WaitForWorker(worker);
        Chunk* chunk = GetWorldChunk(world, job->X, job->Y, job->Z);
        SDL_assert(chunk);
        for (int i = 0; i < ChunkMeshTypeCount; i++)
        {
            CreateIndexBuffer(world, chunk->VoxelBuffers[i].Size * 1.5);
        }
    }
}

void RenderWorld(World* world, const Camera* camera, SDL_GPUCommandBuffer* commandBuffer, SDL_GPURenderPass* pass, ChunkMeshType type)
{
    for (int x = 0; x < WORLD_WIDTH - 2; x++)
    for (int y = 0; y < WORLD_WIDTH - 2; y++)
    {
        int a = world->SortedChunks[x][y][0];
        int b = world->SortedChunks[x][y][1];
        Chunk* chunk = world->Chunks[a][b];
        if (!chunk->VoxelBuffers[type].Size)
        {
            continue;
        }
        if (!GetCameraVisibility(camera, chunk->X, chunk->Y, chunk->Z, CHUNK_WIDTH, CHUNK_HEIGHT, CHUNK_WIDTH))
        {
            continue;
        }
        float position[] = {chunk->X, chunk->Y, chunk->Z};
        SDL_GPUBufferBinding vertexBuffer = {0};
        SDL_GPUBufferBinding indexBuffer = {0};
        vertexBuffer.buffer = chunk->VoxelBuffers[type].Buffer;
        indexBuffer.buffer = world->GpuIndexBuffer.Buffer;
        SDL_PushGPUVertexUniformData(commandBuffer, 1, position, sizeof(position));
        SDL_BindGPUVertexBuffers(pass, 0, &vertexBuffer, 1);
        SDL_BindGPUIndexBuffer(pass, &indexBuffer, SDL_GPU_INDEXELEMENTSIZE_32BIT);
        SDL_DrawGPUIndexedPrimitives(pass, chunk->VoxelBuffers[type].Size * 1.5, 1, 0, 0, 0);
    }
}

Chunk* GetWorldChunk(const World* world, int x, int y, int z)
{
    if (Contains(world, x, z))
    {
        return world->Chunks[x][z];
    }
    else
    {
        return NULL;
    }
}

static int FloorChunkIndex(float index)
{
    return SDL_floorf(index / CHUNK_WIDTH);
}

Block GetWorldBlock(const World* world, int x, int y, int z)
{
    if (y < 0 || y >= CHUNK_HEIGHT)
    {
        return BlockEmpty;
    }
    int chunkX = FloorChunkIndex(x - world->X * CHUNK_WIDTH);
    int chunkY = FloorChunkIndex(y - world->Y * CHUNK_HEIGHT);
    int chunkZ = FloorChunkIndex(z - world->Z * CHUNK_WIDTH);
    Chunk* chunk = GetWorldChunk(world, chunkX, chunkY, chunkZ);
    if (chunk && !(chunk->Flags & ChunkFlagGenerate))
    {
        return GetChunkBlock(chunk, x, y, z);
    }
    else
    {
        return BlockEmpty;
    }
}

void SetWorldBlock(World* world, int x, int y, int z, Block block)
{
    if (y < 0 || y >= CHUNK_HEIGHT)
    {
        return;
    }
    int chunkX = FloorChunkIndex(x - world->X * CHUNK_WIDTH);
    int chunkY = FloorChunkIndex(y - world->Y * CHUNK_HEIGHT);
    int chunkZ = FloorChunkIndex(z - world->Z * CHUNK_WIDTH);
    Chunk* chunk = GetWorldChunk(world, chunkX, chunkY, chunkZ);
    if (chunk && !(chunk->Flags & ChunkFlagGenerate))
    {
        SetChunkBlock(chunk, x, y, z, block);
    }
    else
    {
        SDL_Log("Bad block position: %d, %d, %d", x, y, z);
    }
    // TODO: remesh neighbors
}

WorldQuery RaycastWorld(const World* world, float x, float y, float z, float dx, float dy, float dz, float length)
{
    WorldQuery query = {0};
    query.Position[0] = SDL_floorf(x);
    query.Position[1] = SDL_floorf(y);
    query.Position[2] = SDL_floorf(z);
    float position[3] = {x, y, z};
    float direction[3] = {dx, dy, dz};
    float distances[3] = {0};
    int steps[3] = {0};
    float delta[3] = {0};
    for (int i = 0; i < 3; i++)
    {
        query.PreviousPosition[i] = query.Position[i];
        if (SDL_fabsf(direction[i]) > SDL_FLT_EPSILON)
        {
            delta[i] = SDL_fabsf(1.0f / direction[i]);
        }
        else
        {
            delta[i] = 1e6;
        }
        if (direction[i] < 0.0f)
        {
            steps[i] = -1;
            distances[i] = (position[i] - query.Position[i]) * delta[i];
        }
        else
        {
            steps[i] = 1;
            distances[i] = (query.Position[i] + 1.0f - position[i]) * delta[i];
        }
    }
    float traveled = 0.0f;
    while (traveled <= length)
    {
        query.HitBlock = GetWorldBlock(world, query.Position[0], query.Position[1], query.Position[2]);
        if (query.HitBlock != BlockEmpty)
        {
            return query;
        }
        for (int i = 0; i < 3; i++)
        {
            query.PreviousPosition[i] = query.Position[i];
        }
        if (distances[0] < distances[1])
        {
            if (distances[0] < distances[2])
            {
                traveled = distances[0];
                distances[0] += delta[0];
                query.Position[0] += steps[0];
            }
            else
            {
                traveled = distances[2];
                distances[2] += delta[2];
                query.Position[2] += steps[2];
            }
        }
        else
        {
            if (distances[1] < distances[2])
            {
                traveled = distances[1];
                distances[1] += delta[1];
                query.Position[1] += steps[1];
            }
            else
            {
                traveled = distances[2];
                distances[2] += delta[2];
                query.Position[2] += steps[2];
            }
        }
    }
    query.HitBlock = BlockEmpty;
    return query;
}