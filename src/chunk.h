#pragma once

#include <SDL3/SDL.h>

#include "block.h"
#include "buffer.h"
#include "map.h"

#define CHUNK_WIDTH 30
#define CHUNK_HEIGHT 240

typedef struct Noise Noise;

typedef enum ChunkMeshType
{
    ChunkMeshTypeDefault,
    ChunkMeshTypeTransparent,
    ChunkMeshTypeCount,
}
ChunkMeshType;

typedef enum ChunkFlag
{
    ChunkFlagNone = 0,
    ChunkFlagGenerate = 1,
    ChunkFlagMesh = 2,
}
ChunkFlag;

typedef struct Chunk
{
    ChunkFlag Flags;
    int X;
    int Y;
    int Z;
    Map Blocks;
    Map Lights;
    GpuBuffer VoxelBuffers[ChunkMeshTypeCount];
    GpuBuffer LightBuffer;
}
Chunk;

void CreateChunk(Chunk* chunk, SDL_GPUDevice* device);
void DestroyChunk(Chunk* chunk);
void SetChunkBlock(Chunk* chunk, int x, int y, int z, Block block);
Block GetChunkBlock(const Chunk* chunk, int x, int y, int z);
void GenerateChunk(Chunk* chunk, const Noise* noise);
void MeshChunk(Chunk* chunk, const Chunk* neighbors[3][3], SDL_GPUCopyPass* pass,
    CpuBuffer voxelBuffers[ChunkMeshTypeCount], CpuBuffer* lightBuffer);