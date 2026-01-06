#include <SDL3/SDL.h>

#include "block.h"
#include "buffer.h"
#include "chunk.h"
#include "direction.h"
#include "map.h"
#include "noise.h"
#include "voxel.h"

static bool Contains(const Chunk* chunk, int x, int y, int z)
{
    return x >= 0 && y >= 0 && z >= 0 && x < CHUNK_WIDTH && y < CHUNK_HEIGHT && z < CHUNK_WIDTH;
}

static void Transform(const Chunk* chunk, int* x, int* y, int* z)
{
    *x -= chunk->X;
    *y -= chunk->Y;
    *z -= chunk->Z;
    SDL_assert(Contains(chunk, *x, *y, *z));
}

void CreateChunk(Chunk* chunk, SDL_GPUDevice* device)
{
    chunk->Device = device;
    chunk->Flags = ChunkFlagGenerate;
    chunk->X = 0;
    chunk->Y = 0;
    chunk->Z = 0;
    CreateMap(&chunk->Blocks);
    CreateMap(&chunk->Lights);
    for (int i = 0; i < ChunkMeshTypeCount; i++)
    {
        CreateGpuBuffer(&chunk->VoxelBuffers[i], device, SDL_GPU_BUFFERUSAGE_VERTEX);
    }
    CreateGpuBuffer(&chunk->LightBuffer, device, SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ);
}

void DestroyChunk(Chunk* chunk)
{
    chunk->Device = NULL;
    chunk->Flags = ChunkFlagNone;
    for (int i = 0; i < ChunkMeshTypeCount; i++)
    {
        DestroyGpuBuffer(&chunk->VoxelBuffers[i]);
    }
    DestroyGpuBuffer(&chunk->LightBuffer);
    DestroyMap(&chunk->Blocks);
    DestroyMap(&chunk->Lights);
}

void SetChunkBlock(Chunk* chunk, int x, int y, int z, Block block)
{
    chunk->Flags |= ChunkFlagMesh;
    Transform(chunk, &x, &y, &z);
    if (block != BlockEmpty)
    {
        SetMapValue(&chunk->Blocks, x, y, z, block);
    }
    else
    {
        RemoveMapValue(&chunk->Blocks, x, y, z);
    }
    if (IsBlockLightSource(block))
    {
        SetMapValue(&chunk->Lights, x, y, z, block);
    }
    else
    {
        RemoveMapValue(&chunk->Lights, x, y, z);
    }
}

Block GetChunkBlock(const Chunk* chunk, int x, int y, int z)
{
    SDL_assert(!(chunk->Flags & ChunkFlagGenerate));
    Transform(chunk, &x, &y, &z);
    return GetMapValue(&chunk->Blocks, x, y, z);
}

void GenerateChunk(Chunk* chunk, const Noise* noise)
{
    SDL_assert(chunk->Flags & ChunkFlagGenerate);
    ClearMap(&chunk->Blocks);
    ClearMap(&chunk->Lights);
    GenerateChunkNoise(noise, chunk);
    chunk->Flags &= ~ChunkFlagGenerate;
    chunk->Flags |= ChunkFlagMesh;
}

void MeshChunk(Chunk* chunk, const Chunk* neighbors[3][3], CpuBuffer voxelBuffers[ChunkMeshTypeCount], CpuBuffer* lightBuffer)
{
    SDL_assert(!(chunk->Flags & ChunkFlagGenerate));
    SDL_assert(chunk->Flags & ChunkFlagMesh);
    SDL_GPUCommandBuffer* commandBuffer = SDL_AcquireGPUCommandBuffer(chunk->Device);
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
    for (Uint32 i = 0; i < chunk->Blocks.Capacity; i++)
    {
        if (!IsMapRowValid(&chunk->Blocks, i))
        {
            continue;
        }
        MapRow row = GetMapRow(&chunk->Blocks, i);
        SDL_assert(row.Value != BlockEmpty);
        if (IsBlockSprite(row.Value))
        {
            continue;
        }
        for (int j = 0; j < 6; j++)
        {
            int neighborX = row.X + kDirections[j][0];
            int neighborY = row.Y + kDirections[j][1];
            int neighborZ = row.Z + kDirections[j][2];
            Block neighborBlock;
            if (neighborY == CHUNK_HEIGHT)
            {
                neighborBlock = BlockEmpty;
            }
            else if (neighborY == -1)
            {
                continue;
            }
            else if (Contains(chunk, neighborX, neighborY, neighborZ))
            {
                neighborBlock = GetMapValue(&chunk->Blocks, neighborX, neighborY, neighborZ);
            }
            else
            {
                neighborX += chunk->X;
                neighborY += chunk->Y;
                neighborZ += chunk->Z;
                SDL_assert(kDirections[j][1] == 0);
                int directionX = kDirections[j][0] + 1;
                int directionZ = kDirections[j][2] + 1;
                const Chunk* neighbor = neighbors[directionX][directionZ];
                // TODO: replace condition with SDL_assert
                if (neighbor)
                {
                    neighborBlock = GetChunkBlock(neighbor, neighborX, neighborY, neighborZ);
                }
                else
                {
                    neighborBlock = BlockEmpty;
                }
            }
            if (IsBlockOpaque(neighborBlock))
            {
                continue;
            }
            for (int k = 0; k < 4; k++)
            {
                Voxel voxel = VoxelPackCube(row.Value, row.X, row.Y, row.Z, j, 0, k);
                AppendCpuBuffer(&voxelBuffers[ChunkMeshTypeDefault], &voxel);
            }
        }
    }
    for (int i = -1; i <= 1; i++)
    for (int j = -1; j <= 1; j++)
    {
        // NOTE: not really a neighbor since it's also us
        const Chunk* neighbor = neighbors[i + 1][j + 1];
        if (!neighbor)
        {
            continue;
        }
        for (Uint32 i = 0; i < neighbor->Lights.Capacity; i++)
        {
            if (!IsMapRowValid(&neighbor->Lights, i))
            {
                continue;
            }
            MapRow row = GetMapRow(&neighbor->Lights, i);
            SDL_assert(row.Value != BlockEmpty);
            SDL_assert(IsBlockLightSource(row.Value));
            Light light = GetBlockLight(row.Value);
            light.X = neighbor->X + row.X;
            light.Y = neighbor->Y + row.Y;
            light.Z = neighbor->Z + row.Z;
            AppendCpuBuffer(lightBuffer, &light);
        }
    }
    for (int i = 0; i < ChunkMeshTypeCount; i++)
    {
        UpdateGpuBuffer(&chunk->VoxelBuffers[i], pass, &voxelBuffers[i]);
    }
    UpdateGpuBuffer(&chunk->LightBuffer, pass, lightBuffer);
    SDL_EndGPUCopyPass(pass);
    SDL_SubmitGPUCommandBuffer(commandBuffer);
    chunk->Flags &= ~ChunkFlagMesh;
}
