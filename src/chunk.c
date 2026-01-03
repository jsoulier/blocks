#include <SDL3/SDL.h>

#include "block.h"
#include "buffer.h"
#include "chunk.h"
#include "map.h"
#include "noise.h"

static void Transform(const Chunk* chunk, int* x, int* y, int* z)
{
    *x -= chunk->X;
    *y -= chunk->Y;
    *z -= chunk->Z;
}

void CreateChunk(Chunk* chunk, SDL_GPUDevice* device)
{
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
    SetMapValue(&chunk->Blocks, x, y, z, block);
    // TODO: check if light (if so, add to lights)
}

Block GetChunkBlock(const Chunk* chunk, int x, int y, int z)
{
    SDL_assert(chunk->Flags & ChunkFlagGenerate);
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

void MeshChunk(Chunk* chunk, const Chunk* neighbors[3][3], SDL_GPUCopyPass* pass,
    CpuBuffer voxelBuffers[ChunkMeshTypeCount], CpuBuffer* lightBuffer)
{
    for (Uint32 i = 0; i < chunk->Blocks.Capacity; i++)
    {
        if (!IsMapRowValid(&chunk->Blocks, i))
        {
            continue;
        }
        MapRow row = GetMapRow(&chunk->Blocks, i);
        SDL_assert(row.Value != BlockEmpty);
    }
}
