#include <SDL3/SDL.h>

#include "block.h"
#include "buffer.h"
#include "camera.h"
#include "chunk.h"
#include "light.h"
#include "noise.h"
#include "save.h"
#include "sort.h"
#include "voxel.h"
#include "world.h"

static bool Contains(World* world, int x, int z)
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
    for (int i = 0; i < ChunkMeshTypeCount; i++)
    {
        CreateCpuBuffer(&world->CpuVoxelBuffers[i], device, sizeof(Voxel));
    }
    CreateCpuBuffer(&world->CpuLightBuffer, device, sizeof(Light));
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
    for (int x = 0; x < WORLD_WIDTH; x++)
    for (int z = 0; z < WORLD_WIDTH; z++)
    {
        DestroyChunk(world->Chunks[x][z]);
        SDL_free(world->Chunks[x][z]);
    }
    DestroyCpuBuffer(&world->CpuIndexBuffer);
    DestroyGpuBuffer(&world->GpuIndexBuffer);
    for (int i = 0; i < ChunkMeshTypeCount; i++)
    {
        DestroyCpuBuffer(&world->CpuVoxelBuffers[i]);
    }
    DestroyCpuBuffer(&world->CpuLightBuffer);
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
        // TODO: offset by WORLD_WIDTH / 2?
        chunk->X = (world->X + x) * CHUNK_WIDTH;
        chunk->Y = 0;
        chunk->Z = (world->Z + z) * CHUNK_WIDTH;
    }
    SDL_assert(!size);
}

static void CreateIndexBuffer(World* world, SDL_GPUCopyPass* pass, Uint32 size)
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
    UpdateGpuBuffer(&world->GpuIndexBuffer, pass, &world->CpuIndexBuffer);
}

void UpdateWorld(World* world, const Camera* camera, Save* save, Noise* noise)
{
    Move(world, camera);
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
    bool generated = true;
    for (int x = 0; x < WORLD_WIDTH; x++)
    for (int y = 0; y < WORLD_WIDTH; y++)
    {
        Chunk* chunk = world->Chunks[x][y];
        if (chunk->Flags & ChunkFlagGenerate)
        {
            GenerateChunk(chunk, noise);
            generated = false;
            // TODO: use save
            // return;
        }
    }
    if (generated)
    {
        for (int x = 0; x < WORLD_WIDTH - 2; x++)
        for (int y = 0; y < WORLD_WIDTH - 2; y++)
        {
            int a = world->SortedChunks[x][y][0];
            int b = world->SortedChunks[x][y][1];
            Chunk* chunk = world->Chunks[a][b];
            if (!(chunk->Flags & ChunkFlagMesh))
            {
                continue;
            }
            Chunk* neighbors[3][3] = {0};
            for (int i = -1; i <= 1; i++)
            for (int j = -1; j <= 1; j++)
            {
                int k = a + i;
                int l = b + j;
                if (Contains(world, k, l))
                {
                    neighbors[i + 1][j + 1] = world->Chunks[k][l];
                }
            }
            MeshChunk(chunk, neighbors, pass, world->CpuVoxelBuffers, &world->CpuLightBuffer);
            for (int i = 0; i < ChunkMeshTypeCount; i++)
            {
                CreateIndexBuffer(world, pass, chunk->VoxelBuffers[i].Size * 1.5);
            }
            // return;
        }
    }
    SDL_EndGPUCopyPass(pass);
    SDL_SubmitGPUCommandBuffer(commandBuffer);
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
