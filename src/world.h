#pragma once

#include <SDL3/SDL.h>

#include "block.h"
#include "buffer.h"
#include "chunk.h"
#include "worker.h"

#define WORLD_WIDTH 20
#define WORLD_WORKERS 4

typedef struct Camera Camera;
typedef struct Noise Noise;
typedef struct Save Save;

typedef struct World
{
    SDL_GPUDevice* Device;
    int X;
    int Y;
    int Z;
    Worker Workers[WORLD_WORKERS];
    CpuBuffer CpuIndexBuffer;
    GpuBuffer GpuIndexBuffer;
    CpuBuffer CpuEmptyLightBuffer;
    GpuBuffer GpuEmptyLightBuffer;
    Chunk* Chunks[WORLD_WIDTH][WORLD_WIDTH];
    int SortedChunks[WORLD_WIDTH - 2][WORLD_WIDTH - 2][2];
}
World;

typedef struct WorldQuery
{
    Block HitBlock;
    int Position[3];
    int PreviousPosition[3];
}
WorldQuery;

void CreateWorld(World* world, SDL_GPUDevice* device);
void DestroyWorld(World* world);
void UpdateWorld(World* world, const Camera* camera, Save* save, Noise* noise);
void RenderWorld(World* world, const Camera* camera, SDL_GPUCommandBuffer* commandBuffer, SDL_GPURenderPass* pass, ChunkMeshType type);
Chunk* GetWorldChunk(const World* world, int x, int y, int z);

// TODO: convert to take arrays for position instead
Block GetWorldBlock(const World* world, int x, int y, int z);
void SetWorldBlock(World* world, int x, int y, int z, Block block, Save* save);

WorldQuery RaycastWorld(const World* world, float x, float y, float z, float dx, float dy, float dz, float length);